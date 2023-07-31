// Copyright(c) 2022 Sebastian Veith

#pragma once

#include <log.h>

namespace Filter
{
    // TODO: compensate for non-equidistant sample timing?
    template <typename Value>
    class FilterBase
    {
      public:
        explicit FilterBase(const float strength)
        {
            FilterBase<Value>::SetStrength(strength);
        }
        virtual ~FilterBase(){};
        virtual float SetStrength(const float strength)
        {
            const float limitedStrength = std::min(1.0f, std::max(0.0f, strength));
            openxr_api_layer::log::DebugLog("Filter(%s) set strength: %f\n", typeid(Value).name(), limitedStrength);
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
        explicit SingleEmaFilter(float strength);
        float SetStrength(float strength) override;
        void ApplyFilter(XrVector3f& location) override;
        void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_Alpha{1.0f - m_Strength, 1.0f - m_Strength, 1.0f - m_Strength};
        XrVector3f m_OneMinusAlpha{m_Strength, m_Strength, m_Strength};
        XrVector3f m_Ema{0, 0, 0};
        [[nodiscard]] XrVector3f EmaFunction(XrVector3f current, XrVector3f stored) const;
      private:
        float m_VerticalFactor{1.f};
    };

    class DoubleEmaFilter : public SingleEmaFilter
    {
      public:
        explicit DoubleEmaFilter(const float strength) : SingleEmaFilter(strength){};
        void ApplyFilter(XrVector3f& location) override;
        void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_EmaEma{0, 0, 0};
    };

    class TripleEmaFilter : public DoubleEmaFilter
    {
      public:
        explicit TripleEmaFilter(const float strength) : DoubleEmaFilter(strength){};
        void ApplyFilter(XrVector3f& location) override;
        void Reset(const XrVector3f& location) override;

      protected:
        XrVector3f m_EmaEmaEma{0, 0, 0};
    };

    // rotational filters
    class SingleSlerpFilter : public FilterBase<XrQuaternionf>
    {
      public:
        explicit SingleSlerpFilter(const float strength) : FilterBase(strength){};
        void ApplyFilter(XrQuaternionf& rotation) override;
        void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_FirstStage = xr::math::Quaternion::Identity();
    };

    class DoubleSlerpFilter : public SingleSlerpFilter
    {
      public:
        explicit DoubleSlerpFilter(const float strength) : SingleSlerpFilter(strength){};
        void ApplyFilter(XrQuaternionf& rotation) override;
        void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_SecondStage = xr::math::Quaternion::Identity();
    };

    class TripleSlerpFilter : public DoubleSlerpFilter
    {
      public:
        explicit TripleSlerpFilter(const float strength) : DoubleSlerpFilter(strength){};
        void ApplyFilter(XrQuaternionf& rotation) override;
        void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_ThirdStage = xr::math::Quaternion::Identity();
    };
} // namespace Filter