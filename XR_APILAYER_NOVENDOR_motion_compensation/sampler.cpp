// Copyright(c) 2024 Sebastian Veith

#include "pch.h"

#include "sampler.h"
#include "filter.h"
#include "output.h"
#include <log.h>
#include <util.h>

using namespace openxr_api_layer;
using namespace log;
using namespace output;
using namespace utility;

namespace sampler
{

    Sampler::Sampler(tracker::VirtualTracker* tracker, const std::shared_ptr<output::RecorderBase>& recorder)
        : m_Tracker(tracker), m_Recorder(recorder)
    {
        if (int window{}; GetConfig()->GetInt(Cfg::StabilizerWindow, window))
        {
            // TODO: scale input parameter t0 [0..1] range?
            window = std::max(std::min(window, 1000), 1) * 1000000;
            m_Stabilizer =
                std::make_shared<filter::EmaStabilizer>(std::vector<DofValue>{yaw, roll, pitch}, 1.f);
            DebugLog("stabilizer averaging time: %u ms", window / 1000000);
        }
        GetConfig()->GetBool(Cfg::RecordSamples, m_RecordSamples);
    }

    Sampler::~Sampler()
    {
        StopSampling();
    }

    void Sampler::SetWindowSize(const unsigned size) const
    {
        m_Stabilizer->SetWindowSize(size * 1000000);
    }

    bool Sampler::ReadData(Dof& dof, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Sampler::ReadData");

        if (!m_IsSampling)
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
        m_Stabilizer->Stabilize(dof);

        TraceLoggingWriteStop(local, "Sampler::ReadData", TLArg(true, "Success"));
        return true;
    }

    void Sampler::StartSampling()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Sampler::StartSampling");

        if (m_IsSampling)
        {
            return;
        }
        if (m_Thread)
        {
            StopSampling();
        }
        m_IsSampling = true;
        m_Thread = new std::thread(&Sampler::DoSampling, this);

        TraceLoggingWriteStop(local, "Sampler::StartSampling");
    }

    void Sampler::StopSampling()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Sampler::StopSampling");

        m_IsSampling = false;
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

        TraceLoggingWriteStop(local, "Sampler::StopSampling");
    }

    void Sampler::DoSampling()
    {
        using namespace std::chrono;

        const nanoseconds start = steady_clock::now().time_since_epoch();
        m_Stabilizer->SetStartTime(start.count());

        while (m_IsSampling)
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
            m_Stabilizer->InsertSample(dof, time);

            // record sample
            if (m_RecordSamples && m_Recorder)
            {
                m_Recorder->AddDofValues(dof, Sampled);
                m_Recorder->Write();
            }

            // wait for next sampling cycle
            std::this_thread::sleep_until(now + m_Interval);
        }
        m_IsSampling = false;
    }
} // namespace sampler