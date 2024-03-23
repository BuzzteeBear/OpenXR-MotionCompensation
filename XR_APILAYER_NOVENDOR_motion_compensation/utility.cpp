// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include <DirectXMath.h>
#include <log.h>
#include <util.h>
#include "layer.h"
#include "config.h"
#include "output.h"

using namespace openxr_api_layer;
using namespace log;
using namespace xr::math;
using namespace Pose;
using namespace DirectX;

namespace utility
{
    XrVector3f ToEulerAngles(XrQuaternionf q)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "ModifierBase::ToEulerAngles", TLArg(xr::ToString(q).c_str(), "Quaternion"));

        XrVector3f angles;

        // pitch (x-axis rotation)
        const double sinP = std::sqrt(1 + 2.0 * (q.w * q.x - q.z * q.y));
        const double cosP = std::sqrt(1 - 2.0 * (q.w * q.x - q.z * q.y));
        angles.x = static_cast<float>(2 * std::atan2(sinP, cosP) - M_PI / 2);

        // yaw (y-axis rotation)
        const double sinYCosP = 2.0 * (q.w * q.y + q.z * q.x);
        const double cosYCosP = 1 - 2.0 * (q.x * q.x + q.y * q.y);
        angles.y = static_cast<float>(std::atan2(sinYCosP, cosYCosP));

        // roll (z-axis rotation)
        const double sinRCosP = 2.0 * (q.w * q.z + q.x * q.y);
        const double cosRCosP = 1 - 2.0 * (q.z * q.z + q.x * q.x);
        angles.z = static_cast<float>(std::atan2(sinRCosP, cosRCosP));

        TraceLoggingWriteStop(local, "ModifierBase::ToEulerAngles", TLArg(xr::ToString(angles).c_str(), "Angles"));

        return angles;
    }

    AutoActivator::AutoActivator(const std::shared_ptr<input::InputHandler>& input)
    {
        m_Input = input;
        GetConfig()->GetBool(Cfg::AutoActive, m_Activate);
        GetConfig()->GetInt(Cfg::AutoActiveDelay, m_SecondsLeft);
        GetConfig()->GetBool(Cfg::AutoActiveCountdown, m_Countdown);

        Log("auto activation %s, delay: %d seconds, countdown %s",
            m_Activate ? "on" : "off",
            m_SecondsLeft,
            m_Countdown ? "on" : "off");
    }
    void AutoActivator::ActivateIfNecessary(const XrTime time)
    {
        if (m_Activate)
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "AutoActivator::ActivateIfNecessary", TLArg(time, "Time"));

            if (m_SecondsLeft <= 0)
            {
                m_Input->ToggleActive(time);
                m_Activate = false;
                TraceLoggingWriteStop(local,
                                      "AutoActivator::ActivateIfNecessary",
                                      TLArg(m_SecondsLeft, "No_Seconds_Left"));
                return;
            }
            if (0 == m_ActivationTime)
            {
                m_ActivationTime = time + ++m_SecondsLeft * 1000000000ll;
            }
            const int currentlyLeft = static_cast<int>((m_ActivationTime - time) / 1000000000ll);

            if (m_Countdown && currentlyLeft < m_SecondsLeft)
            {
                output::AudioOut::CountDown(currentlyLeft);
            }
            m_SecondsLeft = currentlyLeft;

            TraceLoggingWriteStop(local, "AutoActivator::ActivateIfNecessary", TLArg(m_SecondsLeft, "Seconds_Left"));
        }
    }

    Mmf::Mmf()
    {
        float check;
        if (GetConfig()->GetFloat(Cfg::TrackerCheck, check) && check >= 0)
        {
            m_Check = static_cast<XrTime>(check * 1000000000.0);
            Log("mmf connection refresh interval is set to %.3f ms", check * 1000.0);
        }
        else
        {
            ErrorLog("%s: defaulting to mmf connection refresh interval of %.3f ms",
                     __FUNCTION__,
                     static_cast<double>(m_Check) / 1000000.0);
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

    bool Mmf::Open(const int64_t time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Mmf::Open", TLArg(time, "Time"));

        std::unique_lock lock(m_MmfLock);
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
                ErrorLog("%s: unable to map view to mmf '%s': %s",__FUNCTION__, m_Name.c_str(), LastErrorMsg().c_str());
                lock.unlock();
                Close();
                TraceLoggingWriteStop(local, "Mmf::Open", TLArg(false, "Success"));
                return false;
            }
        }
        else
        {
            if (!m_ConnectionLost)
            {
                ErrorLog("%s: could not open mmf '%s': %s", __FUNCTION__,  m_Name.c_str(), LastErrorMsg().c_str());
                m_ConnectionLost = true;
            }
            TraceLoggingWriteStop(local, "Mmf::Open", TLArg(false, "Success"));
            return false;
        }
        TraceLoggingWriteStop(local, "Mmf::Open", TLArg(true, "Success"));
        return true;
    }

    bool Mmf::Read(void* buffer, const size_t size, const int64_t time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Mmf::Read", TLArg(time, "Time"));

        if (m_Check > 0 && time - m_LastRefresh > m_Check)
        {
            Close();
        }
        if (!m_View)
        {
            Open(time);
        }
        std::unique_lock lock(m_MmfLock);
        if (m_View)
        {
            try
            {
                memcpy(buffer, m_View, size);
            }
            catch (std::exception& e)
            {
                ErrorLog("%s: unable to read from mmf %s: %s", __FUNCTION__, m_Name.c_str(), e.what());
                // reset mmf connection
                Close();
                TraceLoggingWriteStop(local, "Mmf::Read", TLArg(false, "Memcpy"));
                return false;
            }
            TraceLoggingWriteStop(local, "Mmf::Read", TLArg(true, "Success"));
            return true;
        }
        TraceLoggingWriteStop(local, "Mmf::Read", TLArg(false, "View"));
        return false;
    }

    void Mmf::Close()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Mmf::Close");

        std::unique_lock lock(m_MmfLock);
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

        TraceLoggingWriteStop(local, "Mmf::Close");
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
