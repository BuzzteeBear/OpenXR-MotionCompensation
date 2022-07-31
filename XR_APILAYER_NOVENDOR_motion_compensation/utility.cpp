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

    void Mmf::SetName(const std::string& name)
    {
        m_Name = name;
    }

    bool Mmf::Exists()
    {
        m_FileHandle = OpenFileMapping(FILE_MAP_READ, FALSE, m_Name.c_str());

        if (m_FileHandle == NULL)
        {
            DWORD err = GetLastError();
            ErrorLog("could not open file mapping object %s: %d - %s\n",
                     m_Name.c_str(),
                     err,
                     LastErrorMsg(err).c_str());
            return false;
        }
        CloseHandle(m_FileHandle);

        return true;
    }
    bool Mmf::Read(void* buffer, size_t size)
    {
        LPVOID localBuffer;
        m_FileHandle = OpenFileMapping(FILE_MAP_READ,
                                   FALSE,
                                   m_Name.c_str());

        if (m_FileHandle == NULL)
        {
            DWORD err = GetLastError();
            ErrorLog("Could not open mmf %s: %d - %s\n",
                     m_Name.c_str(),
                     err,
                     LastErrorMsg(err).c_str());
            return false;
        }

        localBuffer = MapViewOfFile(m_FileHandle, FILE_MAP_READ, 0, 0, 0);
        if (localBuffer == NULL)
        {
            DWORD err = GetLastError();
            ErrorLog("Could not map view of mmf %s: %d - %s\n", m_Name.c_str(), err, LastErrorMsg(err).c_str());
            CloseHandle(m_FileHandle);
            return false;
        }

        try
        {
            memcpy(buffer, localBuffer, size);
        }
        catch (std::exception e)
        {
            ErrorLog("Error reading from mmf %s: %s\n", m_Name.c_str(), e.what());
            UnmapViewOfFile(localBuffer);
            CloseHandle(m_FileHandle);
            return false;
        }
        UnmapViewOfFile(localBuffer);
        CloseHandle(m_FileHandle);

        return true;
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