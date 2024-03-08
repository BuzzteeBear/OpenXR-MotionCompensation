// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "output.h"
#include "resource.h"
#include "layer.h"
#include <log.h>
#include <util.h>
#include <playsoundapi.h>

using namespace openxr_api_layer::log;
using namespace utility;
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
                         LastErrorMsg().c_str());
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
                         LastErrorMsg().c_str());
            }
        }
        TraceLoggingWriteStop(local, "AudioOut::CountDown");
    }

    bool NoRecorder::Toggle(bool isCalibrated)
    {
        ErrorLog("%s: unable to toggle recording", __FUNCTION__);
        AudioOut::Execute(Event::Error);
        return false;
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

    bool PoseRecorder::Toggle(bool isCalibrated)
    {
        if (!m_Started )
        {
            if (isCalibrated)
            {
                return Start();
            }
            ErrorLog("%s: recording requires the tracker to be calibrated", __FUNCTION__);
            AudioOut::Execute(Event::Error);
            return false;
        }
        Stop();
        return false;
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

        if (++m_Counter > m_RecorderMax)
        {
            Start();
        }
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

            TraceLoggingWriteStop(local, "PoseRecorder::Write", TLArg(true, "Success"));
            return;
        }
        TraceLoggingWriteStop(local, "PoseRecorder::Write", TLArg(false, "Stream_Open"));
    }

    bool PoseRecorder::Start()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseRecorder::Start");
        if (m_FileStream.is_open())
        {
            TraceLoggingWriteTagged(local, "PoseRecorder::Start", TLArg(true, "Previous_Stream_Closed"));
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

        TraceLoggingWriteTagged(local, "PoseRecorder::Start", TLArg(fileName.c_str(), "Filename"));

        if (m_FileStream.is_open())
        {
            m_Started = true;
            m_FileStream << m_HeadLine << "\n";
            m_FileStream.flush();

            AudioOut::Execute(Event::RecorderOn);
            TraceLoggingWriteStop(local, "PoseRecorder::Start", TLArg(true, "Success"));
            return true;
        }

        AudioOut::Execute(Event::Error);
        ErrorLog("%s: unable to open output stream for file: %s", __FUNCTION__, fileName.c_str());
        TraceLoggingWriteStop(local, "PoseRecorder::Start", TLArg(false, "Success"));
        return false;
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
        ErrorLog("%s: recording stopped but output stream is already closed", __FUNCTION__);
        TraceLoggingWriteStop(local, "PoseRecorder::Stop", TLArg(false, "Stream_Closed"));
    }

    void PoseAndDofRecorder::AddDofValues(const Dof& dofValues, RecorderDofInput type)
    {
        if (!m_Started)
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseAndDofRecorder::AddDofValues", TLArg(static_cast<uint32_t>(type), "Type"));

        switch (type)
        {
        case Raw:
            m_DofValues.raw = dofValues;
            break;
        case Stabilized:
            m_DofValues.stabilized = dofValues;
            break;
        default:
            break;
        }
        
        TraceLoggingWriteStop(local, "PoseAndDofRecorder::AddDofValues", TLArg(true, "Success"));
    }

    void PoseAndDofRecorder::Write(XrTime time, bool newLine)
    {
        if (!m_Started)
        {
            return;
        }
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PoseAndDofRecorder::Write", TLArg(newLine, "NewLine"));

        if (m_FileStream.is_open())
        {
            PoseRecorder::Write(time, false);

            m_FileStream << ";" << m_DofValues.raw.data[sway] << ";" << m_DofValues.stabilized.data[sway]
                         << ";" << m_DofValues.raw.data[surge] << ";" << m_DofValues.stabilized.data[surge]
                         << ";" << m_DofValues.raw.data[heave] << ";" << m_DofValues.stabilized.data[heave]
                         << ";" << m_DofValues.raw.data[yaw] << ";" << m_DofValues.stabilized.data[yaw]
                         << ";" << m_DofValues.raw.data[pitch] << ";" << m_DofValues.stabilized.data[pitch]
                         << ";" << m_DofValues.raw.data[roll] << ";" << m_DofValues.stabilized.data[roll];
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