// Copyright(c) 2024 Sebastian Veith

#include "pch.h"

#include "sampler.h"

Sampler::~Sampler()
{
    StopSampling();
}

bool Sampler::ReadData(utility::Dof& dof, XrTime time)
{
    if (!m_IsSampling)
    {
        // try to reconnect
        if (m_Source->Open(0))
        {
            StartSampling();
        }
        else
        {
            return false;
        }
    }
    dof = m_Stabilizer->GetValue();
    return true;
}

void Sampler::StartSampling()
{
    if (m_IsSampling)
    {
        return;
    }
    if (m_thread)
    {
        StopSampling();
    }
    m_IsSampling = true;
    m_thread = new std::thread(&Sampler::DoSampling, this);
}

void Sampler::StopSampling()
{
    m_IsSampling = false;
    if (m_thread)
    {
        if (m_thread->joinable())
        {
            m_thread->join();
        }
        delete m_thread;
        m_thread = nullptr;
    }
}

void Sampler::DoSampling()
{
    using namespace std::chrono;
    std::condition_variable condition;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    auto waitUntil = steady_clock::now() + m_Interval;
    while (m_IsSampling)
    {
        // set timing
        condition.wait_until(lock, waitUntil);
        auto now = time_point_cast<nanoseconds>(steady_clock::now());
        waitUntil = now + m_Interval;
        const int64_t nanoTicks = now.time_since_epoch().count();

        // TODO: remove
        openxr_api_layer::log::DebugLog("sample: %u", nanoTicks);

        utility::Dof dof;
        if (!m_Read(dof, nanoTicks, m_Source))
        {
            break;
        }
        m_Stabilizer->InsertSample(dof, nanoTicks);
    }
    m_IsSampling = false;
}