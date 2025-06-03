// Copyright(c) 2024 Sebastian Veith

#include "pch.h"

#include "sampler.h"
#include "config.h"
#include "filter.h"
#include "output.h"
#include <log.h>

using namespace openxr_api_layer;
using namespace log;
using namespace output;
using namespace utility;

namespace sampler
{
    Sampler::Sampler(tracker::TrackerBase* tracker,
                     const std::vector<utility::DofValue>& relevant,
                     const std::shared_ptr<output::RecorderBase>& recorder)
        : m_Tracker(tracker), m_Recorder(recorder)
    {
        QueryPerformanceFrequency(&m_CounterFrequency);
        m_Stabilizer = std::make_shared<filter::BiQuadStabilizer>(relevant);
        GetConfig()->GetBool(Cfg::RecordSamples, m_SampleRecording);
    }

    Sampler::~Sampler()
    {
        StopSampling();
    }

    void Sampler::SetStrength(const float strength) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Sampler::SetStrength", TLArg(strength, "Strength"));

        m_Stabilizer->SetStrength(strength);

        TraceLoggingWriteStop(local, "Sampler::SetStrength", TLArg(strength, "Strength"));
    }

    bool Sampler::ReadData(Dof& dof, XrTime now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Sampler::ReadData", TLArg(now, "Now"));

        if (!m_IsSampling.load())
        {
            // try to reconnect
            if (m_Tracker->GetSource()->Open(0))
            {
                TraceLoggingWriteTagged(local, "Sampler::ReadData", TLArg(true, "Restart"));
                StartSampling();
            }
            else
            {
                TraceLoggingWriteStop(local, "Sampler::ReadData", TLArg(false, "Success"));
                return false;
            }
        }
        m_Stabilizer->Read(dof);

        TraceLoggingWriteStop(local, "Sampler::ReadData", TLArg(true, "Success"));
        return true;
    }

    void Sampler::SetFrameTime(const XrTime frameTime)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "Sampler::SetFrameTime",
                               TLArg(m_XrFrameTime, "Previous_XrFrameTime"),
                               TLArg(m_FrameStart.QuadPart, "Previous_FrameStart"));
        std::lock_guard lock(m_Tracker->m_SampleMutex);

        QueryPerformanceCounter(&m_FrameStart);
        m_XrFrameTime = frameTime;

        TraceLoggingWriteTagged(local,
                                "Sampler::SetFrameTime",
                                TLArg(m_XrFrameTime, "New_XrFrameTime"),
                                TLArg(m_FrameStart.QuadPart, "New_FrameStart"));
        
        if (Dof dof; m_Tracker->m_Calibrated && m_Tracker->ReadSource(frameTime, dof))
        {
            // sample value
            m_Stabilizer->Insert(dof, frameTime);
        }

        TraceLoggingWriteStop(local, "Sampler::SetFrameTime");
    }

    void Sampler::StartSampling()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Sampler::StartSampling");

        if (m_IsSampling.load())
        {
            return;
        }
        if (m_Thread)
        {
            StopSampling();
        }
        m_IsSampling.store(true);
        m_Thread = new std::thread(&Sampler::DoSampling, this);

        TraceLoggingWriteStop(local, "Sampler::StartSampling");
    }

    void Sampler::StopSampling()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Sampler::StopSampling");

        m_IsSampling.store(false);
        if (m_Thread)
        {
            if (m_Thread->joinable())
            {
                m_Thread->join();
            }
            delete m_Thread;
            m_Thread = nullptr;
            TraceLoggingWriteTagged(local, "Sampler::StopSampling", TLArg(true, "Stopped"));
        }
        if (m_SampleRecording && m_Recorder)
        {
            constexpr Dof zero{};
            m_Recorder->AddDofValues(zero, Sampled);
            m_Recorder->AddDofValues(zero, Momentary);
        }

        TraceLoggingWriteStop(local, "Sampler::StopSampling");
    }

    void Sampler::DoSampling()
    {
        using namespace std::chrono;

        m_Stabilizer->SetStartTime(m_XrFrameTime);

        while (m_IsSampling.load())
        {
            time_point<steady_clock> now;
            {
                std::lock_guard lock(m_Tracker->m_SampleMutex);
                now = steady_clock::now();

                // determine sampling time
                LARGE_INTEGER current;
                QueryPerformanceCounter(&current);
                const auto sinceFrame = (current.QuadPart - m_FrameStart.QuadPart) * 1000000000 / m_CounterFrequency.QuadPart;
                const XrTime time = m_XrFrameTime + sinceFrame;

                // sample value
                Dof dof;
                if (!m_Tracker->ReadSource(time, dof))
                {
                    break;
                }
                m_Stabilizer->Insert(dof, time);

                // record sample
                if (m_SampleRecording && m_Recorder)
                {
                    m_Recorder->AddDofValues(dof, Sampled);
                    m_Recorder->Write(true);
                }
            }
            // wait for next sampling cycle
            std::this_thread::sleep_until(now + m_Interval);
        }
        m_IsSampling.store(false);
    }
} // namespace sampler