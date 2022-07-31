// Copyright(c) 2022 Sebastian Veith

// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "filter.h"
#include "layer.h"

using namespace Filter;
using namespace xr::math;

XrVector3f SingleEmaFilter::EmaFunction(XrVector3f current, XrVector3f stored) const
{
    return m_Alpha * current + m_OneMinusAlpha * stored;
}

float SingleEmaFilter::SetStrength(float strength)
{
    FilterBase::SetStrength(strength);
    m_Alpha = {1.0f - m_Strength, 1.0f - m_Strength, 1.0f - m_Strength};
    m_OneMinusAlpha = {m_Strength, m_Strength, m_Strength};
    return m_Strength;
}

void SingleEmaFilter::Filter(XrVector3f& location)
{
    m_Ema = EmaFunction(location, m_Ema);
    location = m_Ema;
}

void SingleEmaFilter::Reset(const XrVector3f& location)
{
    m_Ema = location;
}

void DoubleEmaFilter::Filter(XrVector3f& location)
{
    m_Ema = EmaFunction(location, m_Ema);
    m_EmaEma = EmaFunction(m_Ema, m_EmaEma);
    location = XrVector3f{2, 2, 2} * m_Ema - m_EmaEma;
}

void DoubleEmaFilter::Reset(const XrVector3f& location)
{
    SingleEmaFilter::Reset(location);
    m_EmaEma = location;
}

void TripleEmaFilter::Filter(XrVector3f& location)
{
    m_Ema = EmaFunction(location, m_Ema);
    m_EmaEma = EmaFunction(m_Ema, m_EmaEma);
    m_EmaEmaEma = EmaFunction(m_EmaEma, m_EmaEmaEma);
    XrVector3f three{3, 3, 3};
    location = three * m_Ema - three * m_EmaEma + m_EmaEmaEma;
}

void TripleEmaFilter::Reset(const XrVector3f& location)
{
    DoubleEmaFilter::Reset(location);
    m_EmaEmaEma = location;
}

void SingleSlerpFilter::Filter(XrQuaternionf& rotation)
{
    m_FirstStage = Quaternion::Slerp(rotation, m_FirstStage, m_Strength);
    rotation = m_FirstStage;
}

void SingleSlerpFilter::Reset(const XrQuaternionf& rotation)
{
    m_FirstStage = rotation;
}

void DoubleSlerpFilter::Filter(XrQuaternionf& rotation)
{
    m_FirstStage = Quaternion::Slerp(rotation, m_FirstStage, m_Strength);
    m_SecondStage = Quaternion::Slerp(m_FirstStage, m_SecondStage, m_Strength);
    rotation = m_SecondStage;
}

void DoubleSlerpFilter::Reset(const XrQuaternionf& rotation)
{
    SingleSlerpFilter::Reset(rotation);
    m_SecondStage = rotation;
}

void TripleSlerpFilter::Filter(XrQuaternionf& rotation)
{
    m_FirstStage = Quaternion::Slerp(rotation, m_FirstStage, m_Strength);
    m_SecondStage = Quaternion::Slerp(m_FirstStage, m_SecondStage, m_Strength);
    m_ThirdStage = Quaternion::Slerp(m_SecondStage, m_ThirdStage, m_Strength);
    rotation = m_ThirdStage;
}

void TripleSlerpFilter::Reset(const XrQuaternionf& rotation)
{
    SingleSlerpFilter::Reset(rotation);
    m_ThirdStage = rotation;
}