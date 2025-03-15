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

        const nanoseconds start = steady_clock::now().time_since_epoch();
        m_Stabilizer->SetStartTime(start.count());

        while (m_IsSampling.load())
        {
            // set timing
            auto now = steady_clock::now();
            const int64_t time = time_point_cast<nanoseconds>(now).time_since_epoch().count();

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

            // wait for next sampling cycle
            std::this_thread::sleep_until(now + m_Interval);
        }
        m_IsSampling.store(false);
    }
} // namespace sampler