// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "output.h"
#include "utility.h"
#include "resource.h"
#include "layer.h"
#include <log.h>
#include <util.h>
#include <playsoundapi.h>

using namespace openxr_api_layer::log;
namespace output
{
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
                         utility::LastErrorMsg().c_str());
            }
        }
        else
        {
            ErrorLog("%s: unknown event identifier: %d", __FUNCTION__, event);
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
                         utility::LastErrorMsg().c_str());
            }
        }
        TraceLoggingWriteStop(local, "AudioOut::CountDown");
    }

    PoseRecorder::PoseRecorder()
    {
        m_FileStream << std::fixed << std::setprecision(5);
    }

    PoseRecorder::~PoseRecorder()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Destroy");
        if (m_FileStream.is_open())
        {
            TraceLoggingWriteTagged(local, "PoseRecorder::Stop", TLArg(true, "Stream_Closed"));
            m_FileStream.close();
        }
        TraceLoggingWriteStop(local, "PoseRecorder::Destroy");
    }

    void PoseRecorder::Toggle()
    {
        if (!m_Started)
        {
            Start();
            return;
        }
        Stop();
    }

    void PoseRecorder::AddPose(const XrPosef& pose, RecorderPoseInput type)
    {
        if (!m_Started)
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "PoseRecorder::AddReference",
                               TLArg(static_cast<uint32_t>(type), "Type"),
                               TLArg(xr::ToString(pose).c_str(), "Pose"));
        if (m_Counter > m_RecorderMax)
        {
            m_Started = NewFileStream();
            TraceLoggingWriteTagged(local,
                                    "PoseRecorder::AddReference",
                                    TLArg(true, "Size_Exceeded"),
                                    TLArg(m_Started, "Started"));
        }
        
        switch (type)
        {
        case Reference:
            m_Poses.reference = std::move(pose);
            break;
        case Input:
            m_Poses.input = std::move(pose);
            break;
        case Filtered:
            m_Poses.filtered = std::move(pose);
            break;
        case Modified:
            m_Poses.modified = std::move(pose);
            break;
        case Delta:
            m_Poses.delta = std::move(pose);
            break;
        default:
            break;
        }

        TraceLoggingWriteStop(local, "PoseRecorder::AddReference", TLArg(true, "Success"));
    }

    void PoseRecorder::Write(const XrTime time, bool newLine)
    {
        if (!m_Started)
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Write", TLArg(newLine, "NewLine"));
        if (m_FileStream.is_open())
        {
            const XrVector3f& iP = m_Poses.input.position;            
            const XrVector3f& fP = m_Poses.filtered.position;
            const XrVector3f& mP = m_Poses.modified.position;
            const XrVector3f& rP = m_Poses.reference.position;
            const XrVector3f& dP = m_Poses.delta.position;

            const XrQuaternionf& iO = m_Poses.input.orientation;
            const XrQuaternionf& fO = m_Poses.filtered.orientation;
            const XrQuaternionf& mO = m_Poses.modified.orientation;
            const XrQuaternionf& rO = m_Poses.reference.orientation;
            const XrQuaternionf& dO = m_Poses.delta.orientation;

            m_FileStream << time << ";"
                << iP.x << ";" << fP.x << ";" << mP.x << ";" << rP.x << ";" << dP.x << ";"
                << iP.y << ";" << fP.y << ";" << mP.y << ";" << rP.y << ";" << dP.y << ";"
                << iP.z << ";" << fP.z << ";" << mP.z << ";" << rP.z << ";" << dP.z << ";"
                << iO.x << ";" << fO.x << ";" << mO.x << ";" << rO.x << ";" << dO.x << ";"
                << iO.y << ";" << fO.y << ";" << mO.y << ";" << rO.y << ";" << dO.y << ";"
                << iO.z << ";" << fO.z << ";" << mO.z << ";" << rO.z << ";" << dO.z << ";"
                << iO.w << ";" << fO.w << ";" << mO.w << ";" << rO.w << ";" << dO.w;
            if (newLine)
            {
                m_FileStream << "\n";
            }
            m_FileStream.flush();

            m_Counter++;

            TraceLoggingWriteStop(local, "PoseRecorder::Write", TLArg(true, "Success"));
            return;
        }
        TraceLoggingWriteStop(local, "PoseRecorder::Write", TLArg(false, "Stream_Open"));
    }

    void PoseRecorder::Start()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Start");
        if ((m_Started = NewFileStream()))
        {
            m_FileStream << m_HeadLine << "\n";
            m_FileStream.flush();
            TraceLoggingWriteStop(local, "PoseRecorder::Start", TLArg(true, "Success"));
            return;
        }
        TraceLoggingWriteStop(local, "PoseRecorder::Start", TLArg(false, "Success"));
    }

    void PoseRecorder::Stop()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Stop");

        m_Started = false;
        if (m_FileStream.is_open())
        {
            m_FileStream.close();
            AudioOut::Execute(Event::RecorderOff);
            TraceLoggingWriteStop(local, "PoseRecorder::Stop", TLArg(true, "Stream_Closed"));
            return;
        }
        AudioOut::Execute(Event::Error);
        ErrorLog("%s: recording started but output stream is closed", __FUNCTION__);
        TraceLoggingWriteStop(local, "PoseRecorder::Stop", TLArg(false, "Stream_Closed"));
    }

    bool PoseRecorder::NewFileStream()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::NewFileStream");
        if (m_FileStream.is_open())
        {
            TraceLoggingWriteTagged(local, "PoseRecorder::NewFileStream", TLArg(true, "Previous_Stream_Closed"));
            m_FileStream.close();
        }
        SYSTEMTIME lt;
        GetLocalTime(&lt);
        char buf[1024];
        size_t offset = sprintf(buf,
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

        TraceLoggingWriteTagged(local, "PoseRecorder::NewFileStream", TLArg(fileName.c_str(), "Filename"));
       
        if (m_FileStream.is_open())
        {
            AudioOut::Execute(Event::RecorderOn);
            TraceLoggingWriteStop(local, "PoseRecorder::NewFileStream", TLArg(true, "Success"));
            return true;
        }

        AudioOut::Execute(Event::Error);
        ErrorLog("%s: unable to open output stream for file: %s", __FUNCTION__, fileName.c_str());
        TraceLoggingWriteStop(local, "PoseRecorder::NewFileStream", TLArg(false, "Success"));
        return false;
    }

    void PoseAndMmfRecorder::AddMmfValue(const MmfValueSample& mmfValue)
    {
        if (!m_Started)
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseAndMmfRecorder::AddMmfValue");
        
        m_MmfValue = std::move(mmfValue);
        
        TraceLoggingWriteStop(local, "PoseAndMmfRecorder::AddMmfValue", TLArg(true, "Success"));
    }

    void PoseAndMmfRecorder::Write(XrTime time, bool newLine)
    {
        if (!m_Started)
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseAndMmfRecorder::Write", TLArg(newLine, "NewLine"));
        if (m_FileStream.is_open())
        {
            PoseRecorder::Write(time, false);

            m_FileStream << ";" << m_MmfValue.sway << ";" << m_MmfValue.surge << ";" << m_MmfValue.heave << ";"
                         << m_MmfValue.yaw << ";" << m_MmfValue.roll << ";" << m_MmfValue.pitch;
            if (newLine)
            {
                m_FileStream << "\n";
            }
            m_FileStream.flush();

            TraceLoggingWriteStop(local, "PoseAndMmfRecorder::Write", TLArg(true, "Success"));
            return;
        }
        TraceLoggingWriteStop(local, "PoseAndMmfRecorder::Write", TLArg(false, "Stream_Open"));
    }
} // namespace output