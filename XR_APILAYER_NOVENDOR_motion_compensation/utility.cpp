// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include <DirectXMath.h>
#include <log.h>
#include "utility.h"
#include "config.h"

using namespace motion_compensation_layer::log;
using namespace xr::math;

namespace utility
{
    Mmf::Mmf()
    {
        float check;
        if (GetConfig()->GetFloat(Cfg::TrackerCheck, check) && check >= 0)
        {
            m_Check = static_cast<XrTime>(check * 1000000000.0);
            Log("mmf connection refresh interval is set to %.3f ms\n", check * 1000.0);
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

    bool Mmf::Open(const XrTime time)
    {
        m_FileHandle = OpenFileMapping(FILE_MAP_READ, FALSE, m_Name.c_str());

        if (m_FileHandle)
        {
            m_View = MapViewOfFile(m_FileHandle, FILE_MAP_READ, 0, 0, 0);
            if (m_View != nullptr)
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
                ErrorLog("could not open file mapping object %s: %s\n", m_Name.c_str(), LastErrorMsg().c_str());
                m_ConnectionLost = true;
            }
            return false;
        }
        return true;
    }
    bool Mmf::Read(void* buffer, const size_t size, const XrTime time)
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
        if (const DWORD error = GetLastError())
        {
            LPVOID buffer;
            if (const DWORD bufLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                                   nullptr,
                                                   error,
                                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                                   reinterpret_cast<LPTSTR>(&buffer),
                                                   0,
                                                   nullptr))
            {
                const auto lpStr = static_cast<LPCSTR>(buffer);
                const std::string result(lpStr, lpStr + bufLen);
                LocalFree(buffer);
                return std::to_string(error) + " - " + result;
            }
        }
        return "0";
    }
} // namespace utility
