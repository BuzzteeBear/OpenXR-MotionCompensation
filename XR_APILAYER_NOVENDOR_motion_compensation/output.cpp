// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "output.h"
#include "resource.h"
#include "layer.h"
#include <log.h>
#include <util.h>
#include <playsoundapi.h>

using namespace xr::math;
using namespace Pose;
using namespace openxr_api_layer::log;
using namespace utility;
namespace output
{
    void EventSink::Execute(Event event, bool silent)
    {
        if (!silent)
        {
            AudioOut::Execute(event);
        }
        GetEventMmf()->Execute(event);
        GetStatusMmf()->Execute(event);
    }

    void AudioOut::Execute(const Event event)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "AudioOut::Execute", TLArg(static_cast<int>(event), "Event"));

        const auto soundResource = m_SoundResources.find(event);
        if (m_SoundResources.end() != soundResource)
        {
            if (PlaySound(nullptr, nullptr, 0) && PlaySound(MAKEINTRESOURCE(soundResource->second),
                                                            openxr_api_layer::dllModule,
                                                            SND_RESOURCE | SND_ASYNC))
            {
                TraceLoggingWriteTagged(local, "AudioOut::Execute", TLArg(soundResource->second, "Resource"));
            }
            else
            {
                ErrorLog("%s: unable to play sound (%d : % d): %s",
                         __FUNCTION__,
                         soundResource->first,
                         soundResource->second,
                         LastErrorMsg().c_str());
            }
        }
        TraceLoggingWriteStop(local, "AudioOut::Execute");
    }

    void AudioOut::CountDown(const int seconds)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "AudioOut::CountDown", TLArg(seconds, "Seconds"));

        if (seconds > 0 && seconds <= 10)
        {
            if (PlaySound(nullptr, nullptr, 0) &&
                PlaySound(MAKEINTRESOURCE(COUNT0_WAV + static_cast<uint64_t>(seconds)),
                          openxr_api_layer::dllModule,
                          SND_RESOURCE | SND_ASYNC))

            {
                TraceLoggingWriteTagged(local,
                                        "AudioOut::CountDown",
                                        TLArg(seconds, "Seconds"),
                                        TLArg(COUNT0_WAV + seconds, "Resource"));
            }
            else
            {
                ErrorLog("%s: unable to play sound (%d : % d): %s",
                         __FUNCTION__,
                         -seconds,
                         COUNT0_WAV + seconds,
                         LastErrorMsg().c_str());
            }
        }
        TraceLoggingWriteStop(local, "AudioOut::CountDown");
    }

    void EventMmf::Execute(Event event)
    {
        using namespace std::chrono;

        if (!m_RelevantEvents.contains(event))
        {
            return;
        }

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "EventMmf::Execute", TLArg(static_cast<int>(event), "Event"));

        auto now = time_point_cast<milliseconds>(system_clock::now()).time_since_epoch().count();

        // check communication thread;
        if (m_MmfError)
        {
            StopThread();
            if (!m_LastError)
            {
                m_LastError = now;
            }
            else if (now - m_LastError > 1000)
            {
                // retry starting thread after a second
                m_LastError = 0;
                m_MmfError = false;
                m_StopThread = false;
                m_Thread = new std::thread(&QueuedMmf::UpdateMmf, this);
            }
        }

        // queue up event
        std::lock_guard lock(m_QueueMutex);
        m_EventQueue.push_back({event, now});

        TraceLoggingWriteStop(local, "EventMmf::Execute", TLArg(static_cast<int>(event), "Event"));
    }

    bool EventMmf::WriteImpl(Mmf& mmf)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        EventData info;
        if (!mmf.Read(&info, sizeof(info), 0))
        {
            m_MmfError.store(true);
            return false;
        }
        if (info.id)
        {
            // waiting on processing
            return true;
        }
      

        std::lock_guard lock(m_QueueMutex);
        if (m_EventQueue.empty())
        {
            return true;
        }

        info.id = static_cast<int>(m_EventQueue.front().first);
        info.eventTime = m_EventQueue.front().second;
        if (!mmf.Write(&info, sizeof(info)))
        {
            m_MmfError.store(true);
            return false;
        }
        m_EventQueue.pop_front();
        return true;
    }

    std::unique_ptr<EventMmf> g_EventMmf = nullptr;

    EventMmf* GetEventMmf()
    {
        if (!g_EventMmf)
        {
            g_EventMmf = std::make_unique<EventMmf>();
        }
        return g_EventMmf.get();
    }

    void PoseMmf::Transmit(const XrPosef& position, int poseType)
    {
        using namespace std::chrono;

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "PoseMmf::Transmit",
                               TLArg(xr::ToString(position).c_str(), "Dof"),
                               TLArg(poseType, "PoseType"));

        auto now = time_point_cast<milliseconds>(system_clock::now()).time_since_epoch().count();

        // check communication thread;
        if (m_MmfError)
        {
            StopThread();
            if (!m_LastError)
            {
                m_LastError = now;
            }
            else if (now - m_LastError > 1000)
            {
                DebugLog("PoseMmf::Transmit: retry starting thread");
                m_LastError = 0;
                m_MmfError = false;
                m_StopThread = false;
                m_Thread = new std::thread(&QueuedMmf::UpdateMmf, this);
            }
        }
      
        // queue up event
        if (poseType > 0)
        {
            std::unique_lock lock(m_QueueMutex);
            m_EventQueue.push_back({position, poseType});
            DebugLog("PoseMmf::Transmit: pushed pose: %d / %s, queue size = %u", poseType, xr::ToString(position).c_str(), m_EventQueue.size());
        }

        TraceLoggingWriteStop(local, "PoseMmf::Transmit");
    }

    void PoseMmf::Reset()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseMmf::Reset");

        std::unique_lock lock(m_QueueMutex);
        m_EventQueue.clear();
        m_EventQueue.push_back({{}, 0});

        TraceLoggingWriteStop(local, "PoseMmf::Reset");
    }

    bool PoseMmf::WriteImpl(Mmf& mmf)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));

        std::pair<XrPosef, int32_t> data;
        if (!mmf.Read(&data, sizeof(data), 0))
        {
            m_MmfError.store(true);
            return false;
        }
        if (data.second > 0)
        {
            // waiting on processing
            return true;
        }

        std::unique_lock lock(m_QueueMutex);
        if (m_EventQueue.empty())
        {
            return true;
        }

        DebugLog("PoseMmf::WriteImpl: writing pose: %d / %s",
                 m_EventQueue.front().second,
                 xr::ToString(m_EventQueue.front().first).c_str());   
        if (!mmf.Write(&m_EventQueue.front(), sizeof(data)))
        {
            m_MmfError.store(true);
            return false;
        }
        m_EventQueue.pop_front();
        return true;
    }

    StatusMmf::StatusMmf()
    {
        m_Mmf.SetWriteable(sizeof(int));
        m_Mmf.SetName("Local\\OXRMC_Status");
    }

    void StatusMmf::Execute(Event event)
    {
        if (!m_RelevantEvents.contains(event))
        {
            return;
        }

        int prevStatus = StatusToInt(m_Status);

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "EventMmf::Execute",
                               TLArg(static_cast<int>(event), "Event"),
                               TLArg(prevStatus, "PrevStatus"));

        Status status = m_Status;
        switch (event)
        {
        case Event::Initialized:
            status.initialized = true;
            [[fallthrough]];
        case Event::Load:
            status.activated = false;
            status.calibrated = false;
            status.critical = false;
            status.modified = false;
            status.connectionLost = false;
            status.error = false;
            break;

        case Event::Activated:
            status.activated = true;
            [[fallthrough]];
        case Event::Calibrated:
            status.connectionLost = false;
            [[fallthrough]];
        case Event::Restored:
            status.calibrated = true;
            status.critical = false;
            break;

        case Event::CalibrationLost:
            status.calibrated = false;
            [[fallthrough]];
        case Event::Deactivated:
            status.activated = false;
            break;

        case Event::ConnectionLost:
            status.activated = false;
            status.connectionLost = true;
            break;

        case Event::Critical:
            status.activated = false;
            status.critical = true;
            break;

        case Event::Error:
            status.error = true;
            break;

        case Event::Save:
            status.modified = false;
            break;

        case Event::Plus:
        case Event::Minus:
        case Event::Max:
        case Event::Min:
        case Event::Up:
        case Event::Down:
        case Event::Forward:
        case Event::Back:
        case Event::Left:
        case Event::Right:
        case Event::RotLeft:
        case Event::RotRight:
        case Event::EyeCached:
        case Event::EyeCalculated:
        case Event::ModifierOn:
        case Event::ModifierOff:
        case Event::VerboseOn:
        case Event::VerboseOff:
        case Event::StabilizerOn:
        case Event::StabilizerOff:
            status.modified = true;
            break;
        default:
            break;
        }
        int newStatus = StatusToInt(status);
        ;
        TraceLoggingWriteTagged(local, "EventMmf::Execute", TLArg(newStatus , "NewStatus"));

        if (prevStatus != newStatus)
        {
            if (m_Mmf.Write(&newStatus, sizeof(newStatus)))
            {
                m_Status = status;
                TraceLoggingWriteStop(local, "EventMmf::Execute", TLArg(true, "Update"));
                return;
            }
            TraceLoggingWriteStop(local, "EventMmf::Execute", TLArg(false, "Update"));
            return;
        }
        TraceLoggingWriteStop(local, "EventMmf::Execute", TLArg(false, "Changed"));
    }

    int StatusMmf::StatusToInt(const Status& status)
    {
        using enum StatusFlags;
        return static_cast<int>(initialized) * status.initialized +
               static_cast<int>(calibrated) * status.calibrated +
               static_cast<int>(activated) * status.activated +
               static_cast<int>(critical) * status.critical +
               static_cast<int>(error) * status.error +
               static_cast<int>(connection) * status.connectionLost +
               static_cast<int>(modified) * status.modified;
    }

    std::unique_ptr<StatusMmf> g_StatusMmf = nullptr;

    StatusMmf* GetStatusMmf()
    {
        if (!g_StatusMmf)
        {
            g_StatusMmf = std::make_unique<StatusMmf>();
        }
        return g_StatusMmf.get();
    }

    bool NoRecorder::Toggle(bool isCalibrated)
    {
        ErrorLog("%s: unable to toggle recording", __FUNCTION__);
        EventSink::Execute(Event::Error);
        return false;
    }

    PoseRecorder::PoseRecorder()
    {
        m_FileStream << std::fixed << std::setprecision(5);
        GetConfig()->GetBool(Cfg::RecordSamples, m_RecordSamples);
        Log("recording of samples is %s", (m_RecordSamples ? "activated" : "off"));
    }

    PoseRecorder::~PoseRecorder()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Destroy");
        if (m_FileStream.is_open())
        {
            TraceLoggingWriteTagged(local, "PoseRecorder::Destroy", TLArg(true, "Stream_Closed"));
            m_FileStream.close();
        }
        TraceLoggingWriteStop(local, "PoseRecorder::Destroy");
    }

    void PoseRecorder::SetFwdToStage(const XrPosef& pose)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::SetFwdToStage", TLArg(xr::ToString(pose).c_str(), "Pose"));

        m_StageToFwd = xr::math::Pose::Invert(pose);

        TraceLoggingWriteStop(local,
                              "PoseRecorder::SetFwdToStage",
                              TLArg(xr::ToString(m_StageToFwd).c_str(), "Inverted Pose"));
    }

    bool PoseRecorder::Toggle(bool isCalibrated)
    {
        if (!m_Started.load())
        {
            if (isCalibrated)
            {
                return Start();
            }
            ErrorLog("%s: recording requires reference tracker to be calibrated", __FUNCTION__);
            EventSink::Execute(Event::Error);
            return false;
        }
        Stop();
        return false;
    }

    void PoseRecorder::AddFrameTime(XrTime time)
    {
        if (!m_Started.load())
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::AddFrameTime", TLArg(time, "Time"));

        m_FrameTime = time;

        TraceLoggingWriteStop(local, "PoseRecorder::AddFrameTime");
    }

    void PoseRecorder::AddPose(const XrPosef& pose, RecorderPoseInput type)
    {
        if (!m_Started.load())
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "PoseRecorder::AddReference",
                               TLArg(static_cast<uint32_t>(type), "Type"),
                               TLArg(xr::ToString(pose).c_str(), "Pose"));

        std::lock_guard lock{m_RecorderMutex};
        if (Reference == type)
        {
            m_Ref = pose;
            m_InvertedRef = Invert(pose);

            TraceLoggingWriteStop(local, "PoseRecorder::AddReference", TLArg(true, "Success"));
            return;
        }

        const XrPosef deltaFwd = xr::Normalize(Multiply(Multiply(pose, m_InvertedRef), m_StageToFwd));

        //calculate angles
        XrVector3f angles = utility::ToEulerAngles(deltaFwd.orientation);

        // rotate translations into rig orientation
        XrVector3f translations = xr::Normalize(Pose::Invert(deltaFwd)).position;
        
        m_Poses[type] = {{-translations.x, -translations.y, translations.z}, {-angles.x, angles.y, -angles.z}};

        if (Modified == type && !m_PoseRecorded.load())
        {
            // update start time to avoid offset on elapsed time
            const std::chrono::nanoseconds now = std::chrono::steady_clock::now().time_since_epoch();
            m_StartTime = now.count();
            m_PoseRecorded.store(true);
        }

        TraceLoggingWriteStop(local, "PoseRecorder::AddReference", TLArg(true, "Success"));
    }

    void PoseRecorder::Write(const bool sampled, const bool newLine)
    {
        if (!m_Started || (m_Sampling.load() && m_RecordSamples && !sampled))
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Write", TLArg(newLine, "NewLine"));

        if (++m_Counter > m_RecorderMax)
        {
            Start();
        }
        if (m_FileStream.is_open())
        {
            const XrVector3f& iP = m_Poses[Unfiltered].first;            
            const XrVector3f& fP = m_Poses[Filtered].first;
            const XrVector3f& mP = m_Poses[Modified].first;

            const XrVector3f& iO = m_Poses[Unfiltered].second;
            const XrVector3f& fO = m_Poses[Filtered].second;
            const XrVector3f& mO = m_Poses[Modified].second;

            const std::chrono::nanoseconds now = std::chrono::steady_clock::now().time_since_epoch();
            const float elapsed = ((now.count() - m_StartTime) / 1000) / 1000.f;
            m_FileStream << elapsed << ";" << now.count() << ";" << m_FrameTime << ";"
                << iP.x * 1000.f << ";" << fP.x * 1000.f << ";" << mP.x * 1000.f << ";" 
                << iP.z * 1000.f << ";" << fP.z * 1000.f << ";" << mP.z * 1000.f << ";" 
                << iP.y * 1000.f << ";" << fP.y * 1000.f << ";" << mP.y * 1000.f << ";"
                << iO.y / angleToRadian << ";" << fO.y / angleToRadian << ";" << mO.y / angleToRadian << ";"
                << iO.z / angleToRadian << ";" << fO.z / angleToRadian << ";" << mO.z / angleToRadian << ";"
                << iO.x / -angleToRadian << ";" << fO.x / -angleToRadian << ";" << mO.x / -angleToRadian;
            if (newLine)
            {
                m_FileStream << "\n";
            }
            m_FileStream.flush();

            TraceLoggingWriteStop(local, "PoseRecorder::Write", TLArg(true, "Success"));
            return;
        }
        TraceLoggingWriteStop(local, "PoseRecorder::Write", TLArg(false, "Stream_Open"));
    }

    bool PoseRecorder::Start()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Start");

        std::lock_guard lock{m_RecorderMutex};
        if (m_FileStream.is_open())
        {
            TraceLoggingWriteTagged(local, "PoseRecorder::Start", TLArg(true, "Previous_Stream_Closed"));
            m_FileStream.close();
        }
        SYSTEMTIME lt;
        GetLocalTime(&lt);
        char buf[1024];
        sprintf(buf,
                "%d-%02d-%02d_%02d-%02d-%02d-%03d",
                lt.wYear,
                lt.wMonth,
                lt.wDay,
                lt.wHour,
                lt.wMinute,
                lt.wSecond,
                lt.wMilliseconds);
        const std::string fileName =
            (openxr_api_layer::localAppData / ("recording_" + std::string(buf) + ".csv")).string();
        m_FileStream.open(fileName, std::ios_base::ate);
        m_Counter = 0;

        TraceLoggingWriteTagged(local, "PoseRecorder::Start", TLArg(fileName.c_str(), "Filename"));

        if (m_FileStream.is_open())
        {
            m_Started.store(true);
            const std::chrono::nanoseconds now = std::chrono::steady_clock::now().time_since_epoch();
            m_StartTime = now.count();
            m_FileStream << m_HeadLine << "\n";
            m_FileStream.flush();

            EventSink::Execute(Event::RecorderOn);
            TraceLoggingWriteStop(local, "PoseRecorder::Start", TLArg(true, "Success"));
            return true;
        }

        EventSink::Execute(Event::Error);
        ErrorLog("%s: unable to open output stream for file: %s", __FUNCTION__, fileName.c_str());
        TraceLoggingWriteStop(local, "PoseRecorder::Start", TLArg(false, "Success"));
        return false;
    }

    void PoseRecorder::Stop()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Stop");

        std::unique_ptr<std::lock_guard<std::mutex>> lock{};
        m_Started.store(false);
        m_StartTime = 0;
        m_PoseRecorded.store(false);
        if (m_FileStream.is_open())
        {
            m_FileStream.close();
            EventSink::Execute(Event::RecorderOff);
            TraceLoggingWriteStop(local, "PoseRecorder::Stop", TLArg(true, "Stream_Closed"));
            return;
        }
        EventSink::Execute(Event::Error);
        ErrorLog("%s: recording stopped but output stream is already closed", __FUNCTION__);
        TraceLoggingWriteStop(local, "PoseRecorder::Stop", TLArg(false, "Stream_Closed"));
    }

    void PoseAndDofRecorder::AddDofValues(const Dof& dof, RecorderDofInput type)
    {
        if (!m_Started.load())
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseAndDofRecorder::AddDofValues", TLArg(static_cast<uint32_t>(type), "Type"));

        std::lock_guard lock{m_RecorderMutex};
        switch (type)
        {
        case Sampled:
            m_DofValues.sampled = dof;
            break;
        case Read:
            m_DofValues.read = dof;
            break;
        case Momentary:
            m_DofValues.momentary = dof;
            break;
        default:
            break;
        }

        TraceLoggingWriteStop(local, "PoseAndDofRecorder::AddDofValues", TLArg(true, "Success"));
    }

    void PoseAndDofRecorder::Write(bool sampled, bool newLine)
    {
        if (!m_Started.load() || !m_PoseRecorded.load() || (m_Sampling.load() && m_RecordSamples && !sampled))
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseAndDofRecorder::Write", TLArg(newLine, "NewLine"));

        std::lock_guard lock{m_RecorderMutex};
        if (m_FileStream.is_open())
        {
            PoseRecorder::Write(sampled, false);
            m_FileStream << ";" << m_DofValues.sampled.data[sway] << ";" << m_DofValues.read.data[sway] << ";"
                         << m_DofValues.momentary.data[sway] << ";" << m_DofValues.sampled.data[surge] << ";"
                         << m_DofValues.read.data[surge] << ";" << m_DofValues.momentary.data[surge] << ";"
                         << m_DofValues.sampled.data[heave] << ";" << m_DofValues.read.data[heave] << ";"
                         << m_DofValues.momentary.data[heave] << ";" << m_DofValues.sampled.data[yaw] << ";"
                         << m_DofValues.read.data[yaw] << ";" << m_DofValues.momentary.data[yaw] << ";"
                         << m_DofValues.sampled.data[roll] << ";" << m_DofValues.read.data[roll] << ";"
                         << m_DofValues.momentary.data[roll] << ";" << m_DofValues.sampled.data[pitch] << ";"
                         << m_DofValues.read.data[pitch] << ";" << m_DofValues.momentary.data[pitch];
            if (newLine)
            {
                m_FileStream << "\n";
            }
            m_FileStream.flush();

            TraceLoggingWriteStop(local, "PoseAndDofRecorder::Write", TLArg(true, "Success"));
            return;
        }
        TraceLoggingWriteStop(local, "PoseAndDofRecorder::Write", TLArg(false, "Stream_Open"));
    }
} // namespace output