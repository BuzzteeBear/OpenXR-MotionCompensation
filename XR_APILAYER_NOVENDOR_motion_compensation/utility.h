// Copyright(c) 2022 Sebastian Veith

#pragma once

#include <log.h>
#include "input.h"

namespace utility
{
    constexpr float floatPi{static_cast<float>(M_PI)};
    constexpr float angleToRadian{floatPi / 180.0f};

    struct VirtualTrackerData
    {
        float sway{0.f};
        float surge{0.f};
        float heave{0.f};
        float yaw{0.f};
        float roll{0.f};
        float pitch{0.f};
    };

    class AutoActivator
    {
      private:
        std::shared_ptr<input::InputHandler> m_Input;
        bool m_Activate{false};
        bool m_Countdown{false};
        int m_SecondsLeft{0};
        XrTime m_ActivationTime{0};

      public:
        explicit AutoActivator(const std::shared_ptr<input::InputHandler>& input);
        void ActivateIfNecessary(XrTime time);
    };

    template <typename Sample>
    class Cache
    {
      public:
        explicit Cache(const std::string& type, Sample fallback) : m_Fallback(fallback), m_SampleType(type){};

        void SetTolerance(const XrTime tolerance)
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::SetTolerance", TLArg(tolerance, "Tolerance"));

            m_Tolerance = tolerance;

            TraceLoggingWriteStop(local, "Cache::SetTolerance");
        }

        void AddSample(XrTime time, Sample sample, const bool override)
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::AddSample", TLArg(m_SampleType.c_str(), "Type"), TLArg(time, "Time"));

            std::unique_lock lock(m_CacheLock);
            if (m_Cache.contains(time))
            {
                if (!override)
                {
                    DebugLog("AddSample(%s) at %u: omitted", m_SampleType.c_str(), time);
                    TraceLoggingWriteStop(local, "Cache::AddSample", TLArg(true, "Omitted"));
                    return;
                }
                DebugLog("AddSample(%s) at %u: overriden", m_SampleType.c_str(), time);
                TraceLoggingWriteTagged(local, "Cache::AddSample", TLArg(true, "Override"));
            }
            else
            {
                DebugLog("AddSample(%s) at %u: inserted", m_SampleType.c_str(), time); 
            }
            m_Cache[time] = sample;
            TraceLoggingWriteStop(local, "Cache::AddSample");
        }

        Sample GetSample(XrTime time) const
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::GetSample", TLArg(m_SampleType.c_str(), "Type"), TLArg(time, "Time"));

            std::unique_lock lock(m_CacheLock);

            auto it = m_Cache.lower_bound(time);
            const bool itIsEnd = m_Cache.end() == it;
            if (!itIsEnd)
            {
                if (it->first == time)
                {
                    // exact entry found
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Exact", "Match"),
                                          TLArg(it->first, "Time"));

                    DebugLog("GetSample(%s) at %u: exact match found", m_SampleType.c_str(), time);

                    return it->second;
                }
                else if (it->first <= time + m_Tolerance)
                {
                    // succeeding entry is within tolerance
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Later", "Match"),
                                          TLArg(it->first, "Time"));
                    DebugLog("GetSample(%s) at %u: later match found: %u", m_SampleType.c_str(), time, it->first);

                    return it->second;
                }
            }
            const bool itIsBegin = m_Cache.begin() == it;
            if (!itIsBegin)
            {
                auto lowerIt = it;
                --lowerIt;
                if (lowerIt->first >= time - m_Tolerance)
                {
                    // preceding entry is within tolerance
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Earlier", "Match"),
                                          TLArg(lowerIt->first, "Time"));
                    DebugLog("GetSample(%s) at %u: earlier match found: %u", m_SampleType.c_str(), time, lowerIt->first);

                    return lowerIt->second;
                }
            }
            ErrorLog("GetSample(%s) unable to find sample %u+-%.3fms",
                     m_SampleType.c_str(),
                     time,
                     m_Tolerance / 1000000.0);
            if (!itIsEnd)
            {
                if (!itIsBegin)
                {
                    auto lowerIt = it;
                    --lowerIt;
                    // both entries are valid -> select better match
                    auto returnIt = (time - lowerIt->first < it->first - time ? lowerIt : it);
                    TraceLoggingWriteStop(local,
                                          "Cache::GetSample",
                                          TLArg(m_SampleType.c_str(), "Type"),
                                          TLArg("Estimated Both", "Match"),
                                          TLArg(it->first, "Time"));
                    ErrorLog("GetSample(%s) at %u: using best match: %u ", m_SampleType.c_str(), time, returnIt->first);

                    return returnIt->second;
                }
                // higher entry is first in cache -> use it

                TraceLoggingWriteStop(local,
                                      "Cache::GetSample",
                                      TLArg(m_SampleType.c_str(), "Type"),
                                      TLArg("Estimated Later", "Match"),
                                      TLArg(it->first, "Time"));
                ErrorLog("GetSample(%s) at %u: using best match: t = %u ", m_SampleType.c_str(), time, it->first);
                return it->second;
            }
            if (!itIsBegin)
            {
                auto lowerIt = it;
                --lowerIt;
                // lower entry is last in cache-> use it
                ErrorLog("GetSample(%s) at %u: using best match: t = %u ", m_SampleType.c_str(), time, lowerIt->first);
                TraceLoggingWriteStop(local,
                                      "Cache::GetSample",
                                      TLArg(m_SampleType.c_str(), "Type"),
                                      TLArg("Estimated Earlier", "Match"),
                                      TLArg(lowerIt->first, "Time"));
                return lowerIt->second;
            }
            // cache is empty -> return fallback
            ErrorLog("GetSample(%s) at %u: using fallback!!!", m_SampleType.c_str(), time);
            TraceLoggingWriteStop(local,
                                  "Cache::GetSample",
                                  TLArg(m_SampleType.c_str(), "Type"),
                                  TLArg("Fallback", "Match"));
            return m_Fallback;
        }

        // remove outdated entries
        void CleanUp(const XrTime time)
        {
            using namespace openxr_api_layer::log;
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Cache::CleanUp", TLArg(m_SampleType.c_str(), "Type"), TLArg(time, "Time"));

            std::unique_lock lock(m_CacheLock);

            auto it = m_Cache.lower_bound(time - m_Tolerance);
            if (m_Cache.begin() != it)
            {
                --it;
                if (m_Cache.end() != it && m_Cache.begin() != it)
                {
                    TraceLoggingWriteTagged(local, "Cache::CleanUp", TLArg(it->first, "Eraaed"));
                    m_Cache.erase(m_Cache.begin(), it);
                }
            }

            TraceLoggingWriteStop(local, "Cache::CleanUp");
        }

      private:
        std::map<XrTime, Sample> m_Cache{};
        mutable std::mutex m_CacheLock;
        Sample m_Fallback;
        XrTime m_Tolerance{2000000};
        std::string m_SampleType;
    };

    class Mmf
    {
      public:
        Mmf();
        ~Mmf();
        void SetName(const std::string& name);
        bool Open(XrTime time);
        bool Read(void* buffer, size_t size, XrTime time);
        void Close();

      private:
        XrTime m_Check{1000000000}; // reopen mmf once a second by default
        XrTime m_LastRefresh{0};
        std::string m_Name;
        HANDLE m_FileHandle{nullptr};
        void* m_View{nullptr};
        bool m_ConnectionLost{false};
        std::mutex m_MmfLock;
    };

    static inline bool endsWith(const std::string& str, const std::string& substr)
    {
        const auto pos = str.find(substr);
        return pos != std::string::npos && pos == str.size() - substr.size();
    }

    std::string LastErrorMsg();

} // namespace utility