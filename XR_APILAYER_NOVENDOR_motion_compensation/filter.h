// Copyright(c) 2022 Sebastian Veith

#pragma once

#include "pch.h"

#include "log.h"

namespace Filter
{
    // TODO: compensate for non-equidistant sample timing?
    template <typename Value>
    class FilterBase
    {
      public:
        FilterBase(float strength)
        {
            SetStrength(strength);
        }
        virtual ~FilterBase(){};
        virtual float SetStrength(float strength)
        {
            float limitedStrength = std::min(1.0f, std::max(0.0f, strength));
            LAYER_NAMESPACE::log::DebugLog("Filter(%s) set strength: %f\n", typeid(Value).name(), limitedStrength);
            m_Strength = limitedStrength;
            return m_Strength;
        }
        void Filter(Value& value)
        {
            if (0.0f < m_Strength)
            {
                ApplyFilter(value);
            }
        }
        virtual void ApplyFilter(Value& value) = 0;
        virtual void Reset(const Value& value) = 0;

      protected:
        float m_Strength;
    };

    // translational filters
    class SingleEmaFilter : public FilterBase<XrVector3f>
    {
      public:
        SingleEmaFilter(float strength) : FilterBase(strength){};
        virtual ~SingleEmaFilter(){};
        virtual float SetStrength(float strength) override;
        virtual void ApplyFilter(XrVector3f& location) override;
        virtual void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_Alpha{1.0f - m_Strength, 1.0f - m_Strength, 1.0f - m_Strength};
        XrVector3f m_OneMinusAlpha{m_Strength, m_Strength, m_Strength};
        XrVector3f m_Ema{0, 0, 0};
        XrVector3f EmaFunction(XrVector3f current, XrVector3f stored) const;
    };

    class DoubleEmaFilter : public SingleEmaFilter
    {
      public:
        DoubleEmaFilter(float strength) : SingleEmaFilter(strength){};
        virtual ~DoubleEmaFilter(){};
        virtual void ApplyFilter(XrVector3f& location) override;
        virtual void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_EmaEma{0, 0, 0};
    };

    class TripleEmaFilter : public DoubleEmaFilter
    {
      public:
        TripleEmaFilter(float strength) : DoubleEmaFilter(strength){};
        virtual ~TripleEmaFilter(){};
        virtual void ApplyFilter(XrVector3f& location) override;
        virtual void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_EmaEmaEma{0, 0, 0};
    };

    // rotational filters
    class SingleSlerpFilter : public FilterBase<XrQuaternionf>
    {
      public:
        SingleSlerpFilter(float strength) : FilterBase(strength){};
        virtual ~SingleSlerpFilter(){};
        virtual void ApplyFilter(XrQuaternionf& rotation) override;
        virtual void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_FirstStage = xr::math::Quaternion::Identity();
    };

    class DoubleSlerpFilter : public SingleSlerpFilter
    {
      public:
        DoubleSlerpFilter(float strength) : SingleSlerpFilter(strength){};
        virtual ~DoubleSlerpFilter(){};
        virtual void ApplyFilter(XrQuaternionf& rotation) override;
        virtual void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_SecondStage = xr::math::Quaternion::Identity();
    };

    class TripleSlerpFilter : public DoubleSlerpFilter
    {
      public:
        TripleSlerpFilter(float strength) : DoubleSlerpFilter(strength){};
        virtual ~TripleSlerpFilter(){};
        virtual void ApplyFilter(XrQuaternionf& rotation) override;
        virtual void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_ThirdStage = xr::math::Quaternion::Identity();
    };
} // namespace Filter