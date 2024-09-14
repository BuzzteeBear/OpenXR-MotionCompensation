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
        m_OneMinusAlpha = {1.f - m_Alpha.x, 1.f - m_Alpha.y, 1.f - m_Alpha.z};

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

    PassThroughStabilizer::PassThroughStabilizer(const std::vector<utility::DofValue>& relevant) : m_Relevant(relevant)
    {
        auto SetFactor = [this](const Cfg key, const DofValue value) {
            GetConfig()->GetFloat(key, m_Factor.data[value]);
            m_Factor.data[value] = std::max(m_Factor.data[value], 0.f);
        };
        SetFactor(Cfg::StabilizerSway, sway);
        SetFactor(Cfg::StabilizerSurge, surge);
        SetFactor(Cfg::StabilizerHeave, heave);
        SetFactor(Cfg::StabilizerYaw, yaw);
        SetFactor(Cfg::StabilizerRoll, roll);
        SetFactor(Cfg::StabilizerPitch, pitch);
    }

    void PassThroughStabilizer::Insert(utility::Dof& dof, int64_t now)
    {
        std::unique_lock lock(m_SampleMutex);
        m_CurrentSample = dof;
    }

    void PassThroughStabilizer::Read(utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PassThroughStabilizer::Read", TLArg(xr::ToString(dof).c_str(), "DofIn"));

        std::unique_lock lock(m_SampleMutex);
        for (const DofValue value : m_Relevant)
        {
            dof.data[value] = m_CurrentSample.data[value];
        }
        TraceLoggingWriteStop(local, "PassThroughStabilizer::Read", TLArg(xr::ToString(dof).c_str(), "DofOut"));
    }

    LowPassStabilizer::LowPassStabilizer(const std::vector<utility::DofValue>& relevant)
        : PassThroughStabilizer(relevant)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "LowPassStabilizer::LowPassStabilizer");

        float strength;
        GetConfig()->GetFloat(Cfg::StabilizerStrength, strength);
        strength = std::min(std::max(strength, 0.0f), 1.f);
        SetFrequencies(strength);

        DebugLog("stabilizer strength: %.4f", strength);
        TraceLoggingWriteStop(local, "LowPassStabilizer::LowPassStabilizer");
    }

    void LowPassStabilizer::SetFrequencies(float strength)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "LowPassStabilizer::SetFrequencies", TLArg(strength, "Strength"));

        m_Blocking = strength > 0.999f;
        m_Disabled = m_Blocking || strength < 0.001f;
        if (m_Disabled)
        {
            DebugLog("stabilizer filters are %s", m_Blocking ? "blocking" : "disabled");
            TraceLoggingWriteStop(local,
                                  "LowPassStabilizer::SetFrequencies",
                                  TLArg(m_Disabled, "Disabled"),
                                  TLArg(m_Blocking, "Blocked"));
            return;
        }

        for (const DofValue value : m_Relevant)
        {
            const double withFactor = std::min(strength * m_Factor.data[value], 1.f);
            if (withFactor < 0.001f)
            {
                m_Frequency.data[value] = -1.f;
            }
            else
            {
                m_Frequency.data[value] = static_cast<float>(12.5 / (2 * withFactor + 0.1) - 12.5 / 2.1);
            }
            DebugLog("stabilizer low pass frequency(%u) = %f", value, m_Frequency.data[value]);
            TraceLoggingWriteTagged(local,
                                    "LowPassStabilizer::SetFrequencies",
                                    TLArg(static_cast<int>(value), "Value"),
                                    TLArg(m_Frequency.data[value], "Frequency"));
        }
        TraceLoggingWriteStop(local, "LowPassStabilizer::SetFrequencies");
    }

    bool LowPassStabilizer::Disabled(const utility::Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "LowPassStabilizer::Disabled");

        if (m_Disabled)
        {
            for (const DofValue value : m_Relevant)
            {
                m_CurrentSample.data[value] = m_Blocking ? 0.f : dof.data[value];
            }
        }
        TraceLoggingWriteStop(local,
                              "LowPassStabilizer::Disabled",
                              TLArg(m_Disabled, "Disabled"),
                              TLArg(m_Blocking, "Blocked"));
        return m_Disabled;
    }

    void EmaStabilizer::SetStrength(const float strength)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "EmaStabilizer::SetStrength", TLArg(strength, "Strength"));

        std::unique_lock lock(m_SampleMutex);
        SetFrequencies(strength);

        TraceLoggingWriteStop(local, "EmaStabilizer::SetStrength");
    }

    void EmaStabilizer::SetStartTime(int64_t now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "EmaStabilizer::SetStartTime", TLArg(now, "Now"));

        std::unique_lock lock(m_SampleMutex);
        m_LastSampleTime = now;
        m_Initialized = false;

        TraceLoggingWriteStop(local, "EmaStabilizer::SetStartTime");
    }

    void EmaStabilizer::Insert(utility::Dof& dof, int64_t now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "EmaStabilizer::Insert",
                               TLArg(xr::ToString(dof).c_str(), "Sample"),
                               TLArg(now, "Now"));

        std::unique_lock lock(m_SampleMutex);
        if (Disabled(dof))
        {
            TraceLoggingWriteStop(local, "EmaStabilizer::Insert", TLArg(true, "Disabled"));
            return;
        }

        const float duration = (now - std::exchange(m_LastSampleTime, now)) / 1e9f;
        if (!m_Initialized)
        {
            for (const DofValue value : m_Relevant)
            {
                m_CurrentSample.data[value] = m_Frequency.data[value] == 0.f ? 0.f : dof.data[value];
            }
            m_Initialized = true;
            TraceLoggingWriteStop(local, "EmaStabilizer::Insert", TLArg(true, "InitialSample"));
            return;
        }

        const double durationFactor = duration * -2.0 * M_PI;
        for (const DofValue value : m_Relevant)
        {
            if (m_Frequency.data[value] == -1.f)
            {
                m_CurrentSample.data[value] = dof.data[value];
                TraceLoggingWriteTagged(local,
                                        "EmaStabilizer::Insert",
                                        TLArg(static_cast<int>(value), "Value"),
                                        TLArg(true, "Disabled"));
                continue;
            }
            const float factor = static_cast<float>(1 - exp(durationFactor * m_Frequency.data[value]));
            m_CurrentSample.data[value] += (dof.data[value] - m_CurrentSample.data[value]) * factor;
            TraceLoggingWriteTagged(local,
                                    "EmaStabilizer::Insert",
                                    TLArg(static_cast<int>(value), "Value"),
                                    TLArg(factor, "Factor"),
                                    TLArg(this->m_CurrentSample.data[value], "Current_Sample"));
        }
        TraceLoggingWriteStop(local, "EmaStabilizer::Insert");
    }

    void BiQuadStabilizer::SetStrength(float strength)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "BiQuadStabilizer::SetStrength", TLArg(strength, "Strength"));

        std::unique_lock lock(m_SampleMutex);
        SetFrequencies(strength);
        ResetFilters();

        TraceLoggingWriteStop(local, "BiQuadStabilizer::SetStrength");
    }

    void BiQuadStabilizer::SetStartTime(int64_t now)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "BiQuadStabilizer::SetStartTime", TLArg(now, "Now"));

        std::unique_lock lock(m_SampleMutex);
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
        if (Disabled(dof))
        {
            TraceLoggingWriteStop(local, "BiQuadStabilizer::Insert", TLArg(true, "Disabled"));
            return;
        }

        for (const DofValue value : m_Relevant)
        {
            if (m_Frequency.data[value] == -1.f)
            {
                m_CurrentSample.data[value] = dof.data[value];
                TraceLoggingWriteTagged(local,
                                        "BiQuadStabilizer::Insert",
                                        TLArg(static_cast<int>(value), "Value"),
                                        TLArg(true, "Disabled"));
                continue;
            }
            if (!m_Initialized)
            {
                // skip attack time
                for (int i = 0; i < 1e5; i++)
                {
                    Filter(dof.data[value], value);
                }
            }
            m_CurrentSample.data[value] = static_cast<float>(Filter(dof.data[value], value));
            TraceLoggingWriteTagged(local,
                                    "BiQuadStabilizer::Insert",
                                    TLArg(static_cast<int>(value), "Value"),
                                    TLArg(this->m_CurrentSample.data[value], "Current_Sample"));
        }
        m_Initialized = true;

        TraceLoggingWriteStop(local, "BiQuadStabilizer::Insert");
    }

    void BiQuadStabilizer::ResetFilters()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "BiQuadStabilizer::ResetFilters");

        for (const DofValue value : m_Relevant)
        {
            m_Filter[value] = std::make_unique<BiQuadFilter>(BiQuadFilter(m_Frequency.data[value]));
            if (value >= yaw)
            {
                m_Filter[value + 3] = std::make_unique<BiQuadFilter>(BiQuadFilter(m_Frequency.data[value]));
            }
        }
        m_Initialized = false;

        TraceLoggingWriteStop(local, "BiQuadStabilizer::ResetFilters");
    }

    double BiQuadStabilizer::Filter(float dofValue, utility::DofValue value) const
    {
        if (value < yaw)
        {
            return m_Filter[value]->Filter(dofValue);
        }
        // avoid jump when going from -180 to 180 degrees and vice versa

        // angle to radian (between pi and -pi
        double radian = dofValue / 180.0 * M_PI;
        radian = fmod(radian + M_PI, 2.0 * M_PI);
        radian += radian > 0 ? -M_PI : M_PI;

        // use complex numbers to decompose radian angle into sine and cosine
        const std::complex<double> complex = std::polar(1.0, radian);
        const double real = m_Filter[value]->Filter(complex.real());
        const double imag = m_Filter[value + 3]->Filter(complex.imag());
        return std::arg(std::complex<double>{real, imag}) * 180.0 / M_PI;
    }

    BiQuadStabilizer::BiQuadFilter::BiQuadFilter(float frequency)
    {
        const double r = sin(M_PI_4);
        const double a = tan(M_PI * frequency / m_SamplingFrequency);
        const double aSquare = a * a;
        const double s = (aSquare + 2.0 * a * r + 1.0);
        m_A = aSquare / s;
        m_D1 = 2.0 * (1.0 - aSquare) / s;
        m_D2 = -(aSquare - 2.0 * a * r + 1.0) / s;
        m_W0 = m_W1 = m_W2 = 0.0;
    }

    double BiQuadStabilizer::BiQuadFilter::Filter(const double value)
    {
        m_W0 = m_D1 * m_W1 + m_D2 * m_W2 + value;
        return m_A * (std::exchange(m_W2, m_W1) + 2.0f * std::exchange(m_W1, m_W0) + m_W0);
    }
} // namespace filter