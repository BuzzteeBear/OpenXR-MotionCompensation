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
                                 Cfg::KeyOffForward,
                                 Cfg::KeyOffBack,
                                 Cfg::KeyOffUp,
                                 Cfg::KeyOffDown,
                                 Cfg::KeyOffRight,
                                 Cfg::KeyOffLeft,
                                 Cfg::KeyRotRight,
                                 Cfg::KeyRotLeft,
                                 Cfg::KeyOverlay,
                                 Cfg::KeySaveConfig,
                                 Cfg::KeySaveConfigApp,
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
    Mmf::Mmf()
    {
        float check;
        if (GetConfig()->GetFloat(Cfg::TrackerCheck, check) && check >= 0)
        {
            m_Check = (XrTime)(check * 1000000000.0);
            Log("mmf connection refresh interval is set to %.3f ms\n", m_Check / 1000000.0);
        }
        else
        {
            ErrorLog("%s: defaulting to mmf connection refresh interval of %.3f ms\n",
                     __FUNCTION__,
                     m_Check / 1000000.0);
        }
    }

    Mmf::~Mmf()
    {
        Close();
    }

    void Mmf::SetName(const std::string& name)
    {
        m_Name = name;
    }

    bool Mmf::Open(XrTime time)
    {
        m_FileHandle = OpenFileMapping(FILE_MAP_READ, FALSE, m_Name.c_str());

        if (m_FileHandle)
        {
            m_View = MapViewOfFile(m_FileHandle, FILE_MAP_READ, 0, 0, 0);
            if (m_View != NULL)
            {
                m_LastRefresh = time;
                m_ConnectionLost = false;
            }
            else
            {
                DWORD err = GetLastError();
                ErrorLog("unable to map view of mmf %s: %s\n", m_Name.c_str(), LastErrorMsg().c_str());
                Close();
                return false;
            }
        }
        else
        {
            if (!m_ConnectionLost)
            {
                ErrorLog("could not open file mapping object %s: %s", m_Name.c_str(), LastErrorMsg().c_str());
                m_ConnectionLost = true;
            }
            return false;
        }
        return true;
    }
    bool Mmf::Read(void* buffer, size_t size, XrTime time)
    {
        if (m_Check > 0 && time - m_LastRefresh > m_Check)
        {
            Close();
        }
        if (!m_View)
        {
            Open(time);
        }
        if (m_View)
        {
            try
            {
                memcpy(buffer, m_View, size);
            }
            catch (std::exception e)
            {
                ErrorLog("%s: unable to read from mmf %s: %s\n", __FUNCTION__, m_Name.c_str(), e.what());
                // reset mmf connection
                Close();
                return false;
            }
            return true;
        }
        return false;
    }

    void Mmf::Close()
    {
        if (m_View)
        {
            UnmapViewOfFile(m_View);
        }
        m_View = nullptr;
        if (m_FileHandle)
        {
            CloseHandle(m_FileHandle);
        }
        m_FileHandle = nullptr;
    }

    std::string LastErrorMsg()
    {
        DWORD error = GetLastError();
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
                return std::to_string(error) + " - " + result;
            }
        }
        return "0";
    }
} // namespace utility
