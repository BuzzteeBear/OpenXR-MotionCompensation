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

Sampler::Sampler(bool (*read)(Dof&, int64_t, DataSource*),
                 DataSource* source,
                 const std::shared_ptr<output::RecorderBase>& recorder)
    : m_Read(read), m_Source(source), m_Recorder(recorder)
{
    if (int window{}; GetConfig()->GetInt(Cfg::StabilizerWindow, window))
    {
        window = std::max(std::min(window, 1000), 1) * 1000000;
        m_Stabilizer =
            std::make_shared<filter::WeightedMedianStabilizer>(std::vector<DofValue>{yaw, roll, pitch}, window);
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
        if (m_Source->Open(0))
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
    time_point<steady_clock> waitUntil{};

    while (m_IsSampling)
    {
        // set timing
        auto now = steady_clock::now();
        waitUntil = now + m_Interval;
        const int64_t time = time_point_cast<nanoseconds>(now).time_since_epoch().count();

        Dof dof;
        
        if (!m_Read(dof, time, m_Source))
        {
            break;
        }
        m_Stabilizer->InsertSample(dof, time);
        
        if (m_RecordSamples && m_Recorder)
        {
            m_Recorder->AddDofValues(dof, Sampled);
            m_Recorder->Write();
        }

        std::this_thread::sleep_until(waitUntil);
    }
    m_IsSampling = false;
}