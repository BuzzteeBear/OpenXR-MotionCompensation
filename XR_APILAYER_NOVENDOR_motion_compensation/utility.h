// Copyright(c) 2022 Sebastian Veith

#pragma once

#include "pch.h"

#include "config.h"

namespace utility
{
    class KeyboardInput
    {
      public:
        bool Init();
        bool GetKeyState(Cfg key, bool& isRepeat);

      private:
        bool UpdateKeyState(const std::set<int>& vkKeySet, bool& isRepeat);

        std::map<Cfg, std::set<int>> m_ShortCuts;
        std::map <std::set<int>, std::pair<bool, std::chrono::steady_clock::time_point>> m_KeyStates;
        const std::chrono::milliseconds m_KeyRepeatDelay = 200ms;
    };

    class PoseCache
    {
      public:
        PoseCache(XrTime tolerance) : m_Tolerance(tolerance){};

        void AddPose(XrTime time, XrPosef pose);
        XrPosef GetPose(XrTime time) const;

        // remove outdated entries
        void CleanUp(XrTime time);

      private:
        std::map<XrTime, XrPosef> m_Cache{};
        XrTime m_Tolerance;
    };

    // filters

    // TODO: compensate for non-equidistant sample timing?
    template< typename Value>
    class FilterBase
    {
      public:
        FilterBase(float strength)
        {
            SetStrength(strength);
        }
        virtual ~FilterBase(){};
        virtual void SetStrength(float strength)
        {
            float limitedStrength = std::min(1.0f, std::max(0.0f, strength));
            m_Strength = limitedStrength;
        }
        virtual void Filter(Value& value) = 0;
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
        virtual void SetStrength(float strength) override;
        virtual void Filter(XrVector3f& location) override;
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
        virtual void Filter(XrVector3f& location) override;
        virtual void Reset(const XrVector3f& location) override;

      protected:  
        XrVector3f m_EmaEma{0, 0, 0};

    };

    class TripleEmaFilter : public DoubleEmaFilter
    {
      public:
        TripleEmaFilter(float strength) : DoubleEmaFilter(strength){};
        virtual ~TripleEmaFilter(){};
        virtual void Filter(XrVector3f& location) override;
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
        virtual void Filter(XrQuaternionf& rotation) override;
        virtual void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_FirstStage = xr::math::Quaternion::Identity();
    };

    class DoubleSlerpFilter : public SingleSlerpFilter
    {
      public:
        DoubleSlerpFilter(float strength) : SingleSlerpFilter(strength){};
        virtual ~DoubleSlerpFilter(){};
        virtual void Filter(XrQuaternionf& rotation) override;
        virtual void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_SecondStage = xr::math::Quaternion::Identity();
    };

    class TripleSlerpFilter : public DoubleSlerpFilter
    {
      public:
        TripleSlerpFilter(float strength) : DoubleSlerpFilter(strength){};
        virtual ~TripleSlerpFilter(){};
        virtual void Filter(XrQuaternionf& rotation) override;
        virtual void Reset(const XrQuaternionf& rotation) override;

      protected:
        XrQuaternionf m_ThirdStage = xr::math::Quaternion::Identity();
    };

    std::string LastErrorMsg(DWORD error);
}