// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include <DirectXMath.h>
#include <log.h>
#include <util.h>
#include "utility.h"

using namespace motion_compensation_layer::log;
using namespace xr::math;

namespace utility
{
    bool KeyboardInput::Init()
    {
        bool success = true;
        std::set<Cfg> activities{Cfg::KeyActivate,
                                 Cfg::KeyCenter,
                                 Cfg::KeyTransInc,
                                 Cfg::KeyTransDec,
                                 Cfg::KeyRotInc,
                                 Cfg::KeyRotDec,
                                 Cfg::KeySaveConfig,
                                 Cfg::KeyReloadConfig,
                                 Cfg::KeyDebugCor};
        std::string errors;
        for (const Cfg& activity : activities)
        {
            std::set<int> shortcut;
            if (GetConfig()->GetShortcut(activity, shortcut))
            {
                m_ShortCuts[activity] = shortcut;
            }
            else
            {
                success = false;
            }
        }
        return success;
    }

    bool KeyboardInput::GetKeyState(Cfg key, bool& isRepeat)
    {
        auto it = m_ShortCuts.find(key);
        if (it == m_ShortCuts.end())
        {
            ErrorLog("%s(%d): unable to find key\n", __FUNCTION__, key);
            return false;
        }
        return UpdateKeyState(it->second, isRepeat);
    }

    bool KeyboardInput::UpdateKeyState(const std::set<int>& vkKeySet, bool& isRepeat)
    {
        const auto isPressed = vkKeySet.size() > 0 && std::all_of(vkKeySet.begin(), vkKeySet.end(), [](int vk) {
                                   return GetAsyncKeyState(vk) < 0;
                               });
        auto keyState = m_KeyStates.find(vkKeySet);
        const auto now = std::chrono::steady_clock::now();
        if (m_KeyStates.end() == keyState)
        {
            // remember keyState for next call
            keyState = m_KeyStates.insert({vkKeySet, {false, now}}).first;
        }
        const auto lastToggleTime = isPressed != keyState->second.first ? now : keyState->second.second;
        const auto prevState = std::exchange(keyState->second, {isPressed, lastToggleTime});
        isRepeat = isPressed && prevState.first && (now - prevState.second) > m_KeyRepeatDelay;
        if (isRepeat)
        {
            // reset toggle time for next repetition
            keyState->second.second = now;
        }
        return isPressed && (!prevState.first || isRepeat);
    }
  
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