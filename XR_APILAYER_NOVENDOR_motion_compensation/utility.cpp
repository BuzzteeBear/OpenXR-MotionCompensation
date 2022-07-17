#include "pch.h"

#include <DirectXMath.h>
#include <log.h>
#include <util.h>
#include "utility.h"

using namespace motion_compensation_layer::log;
using namespace xr::math;

namespace utilities
{
    bool KeyboardInput::UpdateKeyState(const std::set<int>& vkKeySet, bool& isRepeat)
    {
        const auto isPressed =
            std::all_of(vkKeySet.begin(), vkKeySet.end(), [](int vk) { return GetAsyncKeyState(vk) < 0; });
        auto keyState = m_KeyStates.find(vkKeySet);
        const auto now = std::chrono::steady_clock::now();
        if (m_KeyStates.end() == keyState)
        {
            // remember keyState for next call
            keyState = m_KeyStates.insert({vkKeySet, {false, now}}).first;
        }
        const auto prevState = std::exchange(keyState->second, {isPressed, now});
        isRepeat = isPressed && prevState.first && (now - keyState->second.second) > m_KeyRepeatDelay;
        return isPressed && (!prevState.first || isRepeat);
    }

    void PoseCache::AddPose(XrTime time, XrPosef pose)
    {
        DebugLog("AddPose: %d\n", time);

        m_Cache.insert({time, pose});
    }

    XrPosef PoseCache::GetPose(XrTime time) const
    {
        TraceLoggingWrite(g_traceProvider, "GetPose", TLArg(time, "Time"));

        DebugLog("GetPose: %d\n", time);

        auto it = m_Cache.lower_bound(time);
        bool itIsEnd = m_Cache.end() == it;
        if (!itIsEnd)
        {
            if (it->first <= time + m_Tolerance)
            {
                // exact or succeeding entry is within tolerance
                TraceLoggingWrite(g_traceProvider,
                                  "GetPose_Found",
                                  TLArg("LaterOrEqual", "Type"),
                                  TLArg(it->first, "Time"),
                                  TLArg(xr::ToString(it->second).c_str(), "Pose"));

                return it->second;
            }
        }
        auto lowerIt = it;
        lowerIt--;
        bool lowerItIsBegin = m_Cache.begin() == lowerIt;
        if (!lowerItIsBegin)
        {
            if (lowerIt->first >= time - m_Tolerance)
            {
                // preceding entry is within tolerance
                TraceLoggingWrite(g_traceProvider,
                                  "GetPose_Found",
                                  TLArg("Earlier", "Type"),
                                  TLArg(lowerIt->first, "Time"),
                                  TLArg(xr::ToString(lowerIt->second).c_str(), "Pose"));

                return lowerIt->second;
            }
        }
        ErrorLog("UnmodifiedEyePoseCache::GetPose(%d) unable to find eye pose +-%dms\n", time, m_Tolerance);
        if (!itIsEnd)
        {
            if (!lowerItIsBegin)
            {
                // both etries are valid -> select better match
                auto returnIt = (time - lowerIt->first < it->first - time ? lowerIt : it);
                TraceLoggingWrite(g_traceProvider,
                                  "GetPose_Found",
                                  TLArg("Estimated Both", "Type"),
                                  TLArg(it->first, "Time"),
                                  TLArg(xr::ToString(it->second).c_str(), "Pose"));
                ErrorLog("Using best match: t = %d \n", returnIt->first);
                return returnIt->second;
            }
            // higher entry is first in cache -> use it
            ErrorLog("Using best match: t = %d \n", it->first);
            TraceLoggingWrite(g_traceProvider,
                              "GetPose_Found",
                              TLArg("Estimated Earlier", "Type"),
                              TLArg(it->first, "Time"),
                              TLArg(xr::ToString(it->second).c_str(), "Pose"));
            return it->second;
        }
        if (!lowerItIsBegin)
        {
            // lower entry is last in cache-> use it
            ErrorLog("Using best match: t = %d \n", lowerIt->first);
            TraceLoggingWrite(g_traceProvider,
                              "GetPose_Found",
                              TLArg("Estimated Earlier", "Type"),
                              TLArg(lowerIt->first, "Time"),
                              TLArg(xr::ToString(lowerIt->second).c_str(), "Pose"));
            return lowerIt->second;
        }
        // cache is emtpy -> return fallback
        ErrorLog("Using fallback!!!\n", time);
        TraceLoggingWrite(g_traceProvider, "GetPose_Found", TLArg("Fallback", "Type"));
        return xr::math::Pose::Identity();
    }

        // remove outdated entries
    void PoseCache::CleanUp(XrTime time)
    {
        auto it = m_Cache.lower_bound(time - m_Tolerance);
        if (m_Cache.end() != it && m_Cache.begin() != it)
        {
            m_Cache.erase(m_Cache.begin(), it);
        }
    }
  
    XrVector3f SingleEmaFilter::EmaFunction(XrVector3f current, XrVector3f stored) const
    {
        return m_Alpha * current + m_OneMinusAlpha * stored;
    }

    void SingleEmaFilter::SetStrength(float strength)
    {
        FilterBase::SetStrength(strength);
        m_Alpha = {1.0f - m_Strength, 1.0f - m_Strength, 1.0f - m_Strength};
        m_OneMinusAlpha = {m_Strength, m_Strength, m_Strength};
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
        location = three* m_Ema - three* m_EmaEma + m_EmaEmaEma;    
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
        
    std::string LastErrorMsg(DWORD error)
    {
        if (error)
        {
            LPVOID buffer;
            DWORD bufLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                             FORMAT_MESSAGE_IGNORE_INSERTS,
                                         NULL,
                                         error,
                                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                         (LPTSTR)&buffer,
                                         0,
                                         NULL);
            if (bufLen)
            {
                LPCSTR lpStr = (LPCSTR)buffer;
                std::string result(lpStr, lpStr + bufLen);
                LocalFree(buffer);
                return result;
            }
        }
        return std::string();
    }
} // namespace utilities