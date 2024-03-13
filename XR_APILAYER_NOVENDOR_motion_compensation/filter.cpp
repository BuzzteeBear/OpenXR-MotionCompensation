// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "filter.h"
#include "config.h"
#include <util.h>

using namespace openxr_api_layer::log;
using namespace xr::math;
using namespace utility;

namespace filter
{
    SingleEmaFilter::SingleEmaFilter(const float strength) : FilterBase(strength, "translational")
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "SingleEmaFilter::SingleEmaFilter", TLArg(strength, "Strength"));

        GetConfig()->GetFloat(Cfg::TransVerticalFactor, m_VerticalFactor);
        m_VerticalFactor = std::max(0.0f, m_VerticalFactor);
        openxr_api_layer::log::DebugLog("%s filter vertical factor set: %f", m_Type.c_str(), m_VerticalFactor);
        SingleEmaFilter::SetStrength(m_Strength);

        TraceLoggingWriteStop(local, "SingleEmaFilter::SingleEmaFilter", TLArg(m_VerticalFactor, "VerticalFactor"));
    }

    XrVector3f SingleEmaFilter::EmaFunction(const XrVector3f current, const XrVector3f stored) const
    {
        return m_Alpha * current + m_OneMinusAlpha * stored;
    }

    float SingleEmaFilter::SetStrength(const float strength)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "SingleEmaFilter::SetStrength", TLArg(strength, "Strength"));

        FilterBase::SetStrength(strength);
        m_Alpha = {1.0f - m_Strength, std::max(0.f, 1.0f - (m_VerticalFactor * m_Strength)), 1.0f - m_Strength};
        m_OneMinusAlpha = {1.f - m_Alpha.x , 1.f - m_Alpha.y, 1.f - m_Alpha.z};
        
        TraceLoggingWriteStop(local,
                              "SingleEmaFilter::SetStrength",
                              TLArg(xr::ToString(m_Alpha).c_str(), "Alpha"),
                              TLArg(xr::ToString(m_OneMinusAlpha).c_str(), "OneMinusAlpha"));

        return m_Strength;
    }

    void SingleEmaFilter::ApplyFilter(XrVector3f& location)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "SingleEmaFilter::ApplyFilter",
                               TLArg(xr::ToString(location).c_str(), "location"),
                               TLArg(xr::ToString(this->m_Ema).c_str(), "m_Ema"),
                               TLArg(xr::ToString(this->m_Alpha).c_str(), "m_Alpha"));

        m_Ema = EmaFunction(location, m_Ema);
        location = m_Ema;

        TraceLoggingWriteStop(local,
                              "SingleEmaFilter::ApplyFilter",
                              TLArg(xr::ToString(location).c_str(), "location"),
                              TLArg(xr::ToString(this->m_Ema).c_str(), "m_Ema"));
    }

    void SingleEmaFilter::Reset(const XrVector3f& location)
    {
        m_Ema = location;
    }

    void DoubleEmaFilter::ApplyFilter(XrVector3f& location)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "DoubleEmaFilter::ApplyFilter",
                               TLArg(xr::ToString(location).c_str(), "location"),
                               TLArg(xr::ToString(this->m_Ema).c_str(), "m_Ema"),
                               TLArg(xr::ToString(this->m_EmaEma).c_str(), "m_EmaEma"),
                               TLArg(xr::ToString(this->m_Alpha).c_str(), "m_Alpha"));

        m_Ema = EmaFunction(location, m_Ema);
        m_EmaEma = EmaFunction(m_Ema, m_EmaEma);
        location = XrVector3f{2, 2, 2} * m_Ema - m_EmaEma;

        TraceLoggingWriteStop(local,
                              "DoubleEmaFilter::ApplyFilter",
                              TLArg(xr::ToString(location).c_str(), "location"),
                              TLArg(xr::ToString(this->m_Ema).c_str(), "m_Ema"),
                              TLArg(xr::ToString(this->m_EmaEma).c_str(), "m_EmaEma"));
    }

    void DoubleEmaFilter::Reset(const XrVector3f& location)
    {
        SingleEmaFilter::Reset(location);
        m_EmaEma = location;
    }

    void TripleEmaFilter::ApplyFilter(XrVector3f& location)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TripleEmaFilter::ApplyFilter",
                               TLArg(xr::ToString(location).c_str(), "location"),
                               TLArg(xr::ToString(this->m_Ema).c_str(), "m_Ema"),
                               TLArg(xr::ToString(this->m_EmaEma).c_str(), "m_EmaEma"),
                               TLArg(xr::ToString(this->m_EmaEmaEma).c_str(), "m_EmaEmaEma"),
                               TLArg(xr::ToString(this->m_Alpha).c_str(), "m_Alpha"));

        m_Ema = EmaFunction(location, m_Ema);
        m_EmaEma = EmaFunction(m_Ema, m_EmaEma);
        m_EmaEmaEma = EmaFunction(m_EmaEma, m_EmaEmaEma);
        constexpr XrVector3f three{3, 3, 3};
        location = three * m_Ema - three * m_EmaEma + m_EmaEmaEma;

        TraceLoggingWriteStop(local,
                              "TripleEmaFilter::ApplyFilter",
                              TLArg(xr::ToString(location).c_str(), "location"),
                              TLArg(xr::ToString(this->m_Ema).c_str(), "m_Ema"),
                              TLArg(xr::ToString(this->m_EmaEma).c_str(), "m_EmaEma"),
                              TLArg(xr::ToString(this->m_EmaEmaEma).c_str(), "m_EmaEmaEma"));
    }

    void TripleEmaFilter::Reset(const XrVector3f& location)
    {
        DoubleEmaFilter::Reset(location);
        m_EmaEmaEma = location;
    }

    void SingleSlerpFilter::ApplyFilter(XrQuaternionf& rotation)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "SingleSlerpFilter::ApplyFilter",
                               TLArg(xr::ToString(rotation).c_str(), "rotation"),
                               TLArg(xr::ToString(this->m_FirstStage).c_str(), "m_FirstStage"),
                               TLArg(std::to_string(this->m_Strength).c_str(), "m_Strength"));

        m_FirstStage = Quaternion::Slerp(rotation, m_FirstStage, m_Strength);
        rotation = m_FirstStage;

        TraceLoggingWriteStop(local,
                              "SingleSlerpFilter::ApplyFilter",
                              TLArg(xr::ToString(rotation).c_str(), "rotation"),
                              TLArg(xr::ToString(this->m_FirstStage).c_str(), "m_FirstStage"));
    }

    void SingleSlerpFilter::Reset(const XrQuaternionf& rotation)
    {
        m_FirstStage = rotation;
    }

    void DoubleSlerpFilter::ApplyFilter(XrQuaternionf& rotation)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "DoubleSlerpFilter::ApplyFilter",
                               TLArg(xr::ToString(rotation).c_str(), "rotation"),
                               TLArg(xr::ToString(this->m_FirstStage).c_str(), "m_FirstStage"),
                               TLArg(xr::ToString(this->m_SecondStage).c_str(), "m_SecondStage"),
                               TLArg(std::to_string(this->m_Strength).c_str(), "m_Strength"));

        m_FirstStage = Quaternion::Slerp(rotation, m_FirstStage, m_Strength);
        m_SecondStage = Quaternion::Slerp(m_FirstStage, m_SecondStage, m_Strength);
        rotation = m_SecondStage;

        TraceLoggingWriteStop(local,
                              "DoubleSlerpFilter::ApplyFilter",
                              TLArg(xr::ToString(rotation).c_str(), "rotation"),
                              TLArg(xr::ToString(this->m_FirstStage).c_str(), "m_FirstStage"),
                              TLArg(xr::ToString(this->m_SecondStage).c_str(), "m_SecondStage"));
    }

    void DoubleSlerpFilter::Reset(const XrQuaternionf& rotation)
    {
        SingleSlerpFilter::Reset(rotation);
        m_SecondStage = rotation;
    }

    void TripleSlerpFilter::ApplyFilter(XrQuaternionf& rotation)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TripleSlerpFilter::ApplyFilter",
                               TLArg(xr::ToString(rotation).c_str(), "rotation"),
                               TLArg(xr::ToString(this->m_FirstStage).c_str(), "m_FirstStage"),
                               TLArg(xr::ToString(this->m_SecondStage).c_str(), "m_SecondStage"),
                               TLArg(xr::ToString(this->m_ThirdStage).c_str(), "m_ThirdStage"),
                               TLArg(std::to_string(this->m_Strength).c_str(), "m_Strength"));

        m_FirstStage = Quaternion::Slerp(rotation, m_FirstStage, m_Strength);
        m_SecondStage = Quaternion::Slerp(m_FirstStage, m_SecondStage, m_Strength);
        m_ThirdStage = Quaternion::Slerp(m_SecondStage, m_ThirdStage, m_Strength);
        rotation = m_ThirdStage;

        TraceLoggingWriteStop(local,
                              "TripleSlerpFilter::ApplyFilter",
                              TLArg(xr::ToString(rotation).c_str(), "rotation"),
                              TLArg(xr::ToString(this->m_FirstStage).c_str(), "m_FirstStage"),
                              TLArg(xr::ToString(this->m_SecondStage).c_str(), "m_SecondStage"),
                              TLArg(xr::ToString(this->m_ThirdStage).c_str(), "m_ThirdStage"));
    }

    void TripleSlerpFilter::Reset(const XrQuaternionf& rotation)
    {
        SingleSlerpFilter::Reset(rotation);
        m_ThirdStage = rotation;
    }

    void PassThroughStabilizer::InsertSample(utility::Dof& sample, int64_t time)
    {
        m_CurrentSample = sample;
    }

    void PassThroughStabilizer::Stabilize(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PassThroughStabilizer::Stabilize");

        for (const DofValue value : m_RelevantValues)
        {
            dof.data[value] = m_CurrentSample.data[value];
        }

        TraceLoggingWriteStop(local, "PassThroughStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof")); 
    }

    void EmaStabilizer::SetStartTime(int64_t time)
    {
        m_LastSampleTime = time;
        m_Initialized = false;
    }

    void EmaStabilizer::InsertSample(utility::Dof& sample, int64_t time)
    {
        const float duration = (time - std::exchange(m_LastSampleTime, time)) / 1e9f;

        if (!m_Initialized)
        {
            for (const DofValue value : m_RelevantValues)
            {
                m_CurrentSample.data[value] = sample.data[value];
            }
            m_Initialized = true;
            return;
        }

        const float factor = 1 - exp(-duration * 2 * floatPi * m_Frequency);
        for (const DofValue value : m_RelevantValues)
        {
            m_CurrentSample.data[value] += (sample.data[value] - m_CurrentSample.data[value]) * factor;
        }
    }

    void EmaStabilizer::Stabilize(utility::Dof& dof)
    {
        for (const DofValue value : m_RelevantValues)
        {
            dof.data[value] = m_CurrentSample.data[value];
        }
    }

    void WindowStabilizer::SetWindowSize(unsigned size)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "WindowStabilizer::SetWindowSize", TLArg(size, "Size"));

        m_WindowSize = size;
        openxr_api_layer::log::DebugLog("stabilizer averaging time: %u ms", size);

        TraceLoggingWriteStop(local, "WindowStabilizer::SetWindowSize");
    }

    void WindowStabilizer::SetStartTime(int64_t time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "WindowStabilizer::SetStartTime", TLArg(time, "Time"));

        m_LastSampleTime = time;

        TraceLoggingWriteStop(local, "WindowStabilizer::SetStartTime");
    }

    void WindowStabilizer::InsertSample(Dof& sample, int64_t time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "WindowStabilizer::InsertSample",
                               TLArg(xr::ToString(sample).c_str(), "Sample"),
                               TLArg(time, "Time"));

        PassThroughStabilizer::InsertSample(sample, time);

        const int64_t windowBegin = time - m_WindowSize;
        TraceLoggingWriteTagged(local, "MedianStabilizer::InsertSample", TLArg(windowBegin, "WindowBegin"));

        const int64_t duration = time - std::exchange(m_LastSampleTime, time);
        m_WindowHalf = std::min(m_WindowSize / 2, m_WindowHalf + duration / 2);

        for (const DofValue value : m_RelevantValues)
        {
            std::unique_lock lock(m_SampleMutex);
            auto& samples = m_Samples[value];
            // remove outdated sample(s)
            for (auto it = samples.cbegin(); it != samples.cend();)
            {
                if (it->second.first <= windowBegin)
                {
                    TraceLoggingWriteTagged(local,
                                            "WindowStabilizer::InsertSample",
                                            TLArg(it->second.first, "Dropped"),
                                            TLArg(static_cast<int>(value), "RelevantValue"));
                    it = samples.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            // insert sample
            samples.insert({sample.data[value], {time, duration}});

            TraceLoggingWriteTagged(local,
                                    "WindowStabilizer::InsertSample",
                                    TLArg(samples.size(), "SamplesSize"),
                                    TLArg(static_cast<int>(value), "RelevantValue"));
        }
        TraceLoggingWriteStop(local, "WindowStabilizer::InsertSample");
    }

    void AverageStabilizer::Stabilize(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "SimpleAverageStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));
        for (const DofValue value : m_RelevantValues)
        {
            std::unique_lock lock(m_SampleMutex);

            // calculate average
            float sum{0};
            for (const auto& sample : m_Samples[value])
            {
                sum += sample.first;
            }
            dof.data[value] = sum / m_Samples[value].size();
        }
        TraceLoggingWriteStop(local, "SimpleAverageStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));
    }

    void WeightedAverageStabilizer::Stabilize(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "WeightedAverageStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));
        for (const DofValue value : m_RelevantValues)
        {
            std::unique_lock lock(m_SampleMutex);

            // calculate average
            float sum{0};
            int64_t durationSum{0};     
            for (const auto& sample : m_Samples[value])
            {
                sum += sample.first * sample.second.second;
                durationSum += sample.second.second;
            }
            dof.data[value] = sum / durationSum;
        }
        TraceLoggingWriteStop(local, "WeightedAverageStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));
    }

    void MedianStabilizer::Stabilize(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "MedianStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));

        for (const DofValue value : m_RelevantValues)
        {
            std::unique_lock lock(m_SampleMutex);

            // select median
            const size_t halfSize = m_Samples[value].size() / 2;
            auto med = m_Samples[value].begin();
            std::advance(med, halfSize);
            dof.data[value] = med->first;
        }
        TraceLoggingWriteStop(local, "MedianStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));
    }

    void WeightedMedianStabilizer::Stabilize(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "WeightedMedianStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));

        for (const DofValue value : m_RelevantValues)
        {
            std::unique_lock lock(m_SampleMutex);

            // sum up durations and select weighted median
            int64_t durationSum{0};            
            for (const auto& [sample, times] : m_Samples[value])
            {
                if ((durationSum += times.second) > m_WindowHalf)
                {
                    dof.data[value] = sample;
                    break;
                }
            }
        }
        TraceLoggingWriteStop(local, "WeightedMedianStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));
    }

    
    void HybridStabilizer::Stabilize(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "WeightedMedianStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));

        for (const DofValue value : m_RelevantValues)
        {
            std::unique_lock lock(m_SampleMutex);

            // calculate average
            float sum{0};
            int64_t durationSum{0};
            float median{};
            bool medianFound{false};
            for (const auto& sample : m_Samples[value])
            {
                sum += sample.first * sample.second.second;
                if ((durationSum += sample.second.second) > m_WindowHalf && !medianFound)
                {
                    median = sample.first;
                    medianFound = true;
                }
            }
            dof.data[value] = (sum / durationSum + median) / 2;
        }

        TraceLoggingWriteStop(local, "WeightedMedianStabilizer::Stabilize", TLArg(xr::ToString(dof).c_str(), "Dof"));
    }
} // namespace filter