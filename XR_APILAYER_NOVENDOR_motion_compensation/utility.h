// Copyright(c) 2022 Sebastian Veith

#pragma once

#include "pch.h"

#include "config.h"
#include "log.h"

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

    template <typename Sample>
    class Cache
    {
      public:
        Cache(XrTime tolerance, Sample fallback) : m_Tolerance(tolerance), m_Fallback(fallback){};

        void AddSample(XrTime time, Sample sample)
        {
            m_Cache.insert({time, sample});
        }

        Sample GetSample(XrTime time) const
        {
            TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider, "GetSample", TLArg(time, "Time"));

            LAYER_NAMESPACE::log::DebugLog("GetSample(%s): %d\n", typeid(Sample).name(), time);

            auto it = m_Cache.lower_bound(time);
            bool itIsEnd = m_Cache.end() == it;
            if (!itIsEnd)
            {
                if (it->first == time)
                {
                    // exact entry found
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Found",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Exact", "Match"),
                                      TLArg(it->first, "Time"));

                    LAYER_NAMESPACE::log::DebugLog("GetSample(%s): exact match found\n", typeid(Sample).name());

                    return it->second;
                }
                else if (it->first <= time + m_Tolerance)
                {
                    // succeeding entry is within tolerance
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Found",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Later", "Match"),
                                      TLArg(it->first, "Time"));
                    LAYER_NAMESPACE::log::DebugLog("GetSample(%s): later match found %d\n",
                                                   typeid(Sample).name(),
                                                   it->first);

                    return it->second;
                }
            }
            bool itIsBegin = m_Cache.begin() == it;
            if (!itIsBegin)
            {
                auto lowerIt = it;
                lowerIt--;
                if (lowerIt->first >= time - m_Tolerance)
                {
                    // preceding entry is within tolerance
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Found",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Earlier", "Match"),
                                      TLArg(lowerIt->first, "Time"));
                    LAYER_NAMESPACE::log::DebugLog("GetSample(%s): earlier match found: %d\n",
                                                   typeid(Sample).name(),
                                                   lowerIt->first);

                    return lowerIt->second;
                }
            }
            LAYER_NAMESPACE::log::ErrorLog("GetSample(%s) unable to find sample %d+-%dms\n",
                                           typeid(Sample).name(),
                                           time,
                                           m_Tolerance);
            if (!itIsEnd)
            {
                if (!itIsBegin)
                {
                    auto lowerIt = it;
                    lowerIt--;
                    // both etries are valid -> select better match
                    auto returnIt = (time - lowerIt->first < it->first - time ? lowerIt : it);
                    TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                      "GetSample_Failed",
                                      TLArg(typeid(Sample).name(), "Type"),
                                      TLArg("Estimated Both", "Match"),
                                      TLArg(it->first, "Time"));
                    LAYER_NAMESPACE::log::ErrorLog("Using best match: t = %d \n", returnIt->first);

                    return returnIt->second;
                }
                // higher entry is first in cache -> use it

                TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                  "GetSample_Found",
                                  TLArg(typeid(Sample).name(), "Type"),
                                  TLArg("Estimated Earlier", "Match"),
                                  TLArg(it->first, "Time"));
                LAYER_NAMESPACE::log::ErrorLog("Using best match: t = %d \n", it->first);
                return it->second;
            }
            if (!itIsBegin)
            {
                auto lowerIt = it;
                lowerIt--;
                // lower entry is last in cache-> use it
                LAYER_NAMESPACE::log::ErrorLog("Using best match: t = %d \n", lowerIt->first);
                TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider,
                                  "GetSample_Failed",
                                  TLArg(typeid(Sample).name(), "Type"),
                                  TLArg("Estimated Earlier", "Type"),
                                  TLArg(lowerIt->first, "Time"));
                return lowerIt->second;
            }
            // cache is emtpy -> return fallback
            LAYER_NAMESPACE::log::ErrorLog("Using fallback!!!\n", time);
            TraceLoggingWrite(LAYER_NAMESPACE::log::g_traceProvider, "GetSample_Failed", TLArg("Fallback", "Type"));
            return m_Fallback;
        }

        // remove outdated entries
        void CleanUp(XrTime time)
        {
            auto it = m_Cache.lower_bound(time - m_Tolerance);
            if (m_Cache.end() != it && m_Cache.begin() != it)
            {
                m_Cache.erase(m_Cache.begin(), it);
            }
        }

        bool empty()
        {
            return m_Cache.empty();
        }

      private:
        std::map<XrTime, Sample> m_Cache{};
        Sample m_Fallback;
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