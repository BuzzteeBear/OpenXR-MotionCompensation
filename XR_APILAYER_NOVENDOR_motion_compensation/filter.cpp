// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "filter.h"
#include "config.h"
#include <util.h>

using namespace openxr_api_layer::log;
using namespace xr::math;

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
} // namespace filter