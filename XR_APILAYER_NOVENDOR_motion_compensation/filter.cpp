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

    void PassThroughStabilizer::Insert(utility::Dof& dof, int64_t now)
    {
        std::unique_lock lock(m_SampleMutex);
        m_CurrentSample = dof;
    }

    void PassThroughStabilizer::Read(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PassThroughStabilizer::Read");

        std::unique_lock lock(m_SampleMutex);
        for (const DofValue value : m_RelevantValues)
        {
            dof.data[value] = m_CurrentSample.data[value];
        }

        TraceLoggingWriteStop(local, "PassThroughStabilizer::Read", TLArg(xr::ToString(dof).c_str(), "Dof")); 
    }

    LowPassStabilizer::LowPassStabilizer(const std::vector<utility::DofValue>& relevantValues): PassThroughStabilizer(relevantValues)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "LowPassStabilizer::LowPassStabilizer");

        float frequency;
        GetConfig()->GetFloat(Cfg::StabilizerFrequency, frequency);
        frequency = std::min(std::max(frequency, 0.1f), 100.f);
        m_Frequency = frequency;

        DebugLog("stabilizer frequency: %.4f", frequency);
        TraceLoggingWriteStop(local, "LowPassStabilizer::LowPassStabilizer");
    }

    void LowPassStabilizer::SetStartTime(int64_t now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "LowPassStabilizer::SetStartTime", TLArg(now, "Now"));

        m_LastSampleTime = now;
        m_Initialized = false;

        TraceLoggingWriteStop(local, "LowPassStabilizer::SetStartTime");
    }
    void LowPassStabilizer::Read(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "LowPassStabilizer::Read", TLArg(xr::ToString(dof).c_str(), "DofIn"));

        std::unique_lock lock(m_SampleMutex);
        for (const DofValue value : m_RelevantValues)
        {
            dof.data[value] = m_CurrentSample.data[value];
        }
        TraceLoggingWriteStop(local, "LowPassStabilizer::Read", TLArg(xr::ToString(dof).c_str(), "DofOut"));
    }

    void EmaStabilizer::SetFrequency(const float frequency)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "EmaStabilizer::SetFrequency", TLArg(frequency, "Frequency"));

        std::unique_lock lock(m_SampleMutex);
        m_Frequency = frequency;

        TraceLoggingWriteStop(local, "EmaStabilizer::SetFrequency");
    }

    void EmaStabilizer::Insert(utility::Dof& dof, int64_t now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "EmaStabilizer::Insert",
                               TLArg(xr::ToString(dof).c_str(), "Sample"),
                               TLArg(now, "Now"));

        std::unique_lock lock(m_SampleMutex);
        const float duration = (now - std::exchange(m_LastSampleTime, now)) / 1e9f;
        if (!m_Initialized)
        {
            for (const DofValue value : m_RelevantValues)
            {
                m_CurrentSample.data[value] = dof.data[value];
            }
            m_Initialized = true;
            TraceLoggingWriteStop(local, "EmaStabilizer::Insert", TLArg(true, "InitialSample"));
            return;
        }

        const float factor = 1 - exp(duration * -2.f * utility::floatPi * m_Frequency);
        for (const DofValue value : m_RelevantValues)
        {
            m_CurrentSample.data[value] += (dof.data[value] - m_CurrentSample.data[value]) * factor;
        }
        TraceLoggingWriteStop(local, "EmaStabilizer::Insert");
    }

    void BiQuadStabilizer::SetFrequency(float frequency)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "BiQuadStabilizer::SetFrequency", TLArg(frequency, "Frequency"));

        std::unique_lock lock(m_SampleMutex);
        m_Frequency = frequency;
        ResetFilters();

        TraceLoggingWriteStop(local, "BiQuadStabilizer::SetFrequency");
    }

    void BiQuadStabilizer::SetStartTime(int64_t now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "BiQuadStabilizer::SetStartTime", TLArg(now, "Now"));

        std::unique_lock lock(m_SampleMutex);

        LowPassStabilizer::SetStartTime(now);
        ResetFilters();

        TraceLoggingWriteStop(local, "BiQuadStabilizer::SetStartTime");
    }

    void BiQuadStabilizer::Insert(utility::Dof& dof, int64_t now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "BiQuadStabilizer::Insert",
                               TLArg(xr::ToString(dof).c_str(), "Sample"),
                               TLArg(now, "Now"));

        std::unique_lock lock(m_SampleMutex);
        m_LastSampleTime = now;
        for (const DofValue value : m_RelevantValues)
        {
            if (!m_Initialized)
            {
                // skip attack time
                for (int i = 0; i < 1e5; i++)
                {
                    m_Filter[value]->Filter(dof.data[value]);
                }
            }
            m_CurrentSample.data[value] = m_Filter[value]->Filter(dof.data[value]);
        }
        m_Initialized = true;

        TraceLoggingWriteStop(local, "BiQuadStabilizer::Insert");
    }

    BiQuadStabilizer::BiQuadFilter::BiQuadFilter(float frequency)
    {
        const float a = tanf(utility::floatPi * frequency / m_SamplingFrequency);
        const float a2 = a * a;
        const float r = sinf(utility::floatPi / 4.f);
        const float s = (a2 + 2.f * a * r + 1.f);
        m_A = a2 / s;
        m_D1 = 2.f * (1.f - a2) / s;
        m_D2 = -(a2 - 2.f * a * r + 1.f) / s;
        m_W0 = m_W1 = m_W2 = 0.f;
    }

    float BiQuadStabilizer::BiQuadFilter::Filter(const float value)
    {
        m_W0 = m_D1 * m_W1 + m_D2 * m_W2 + value;
        return  m_A * (std::exchange(m_W2, m_W1) + 2.0f * std::exchange(m_W1, m_W0) + m_W0);
    }


    void BiQuadStabilizer::ResetFilters()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "BiQuadStabilizer::ResetFilters");

        for (const DofValue value : m_RelevantValues)
        {
            m_Filter[value] = std::make_unique<BiQuadFilter>(BiQuadFilter(m_Frequency));
        }
        m_Initialized = false;

        TraceLoggingWriteStop(local, "BiQuadStabilizer::ResetFilters");
    }
} // namespace filter