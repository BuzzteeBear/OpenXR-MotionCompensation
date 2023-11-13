// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include <DirectXMath.h>
#include <log.h>
#include "utility.h"
#include "layer.h"
#include "config.h"
#include "feedback.h"

using namespace openxr_api_layer;
using namespace log;
using namespace xr::math;

namespace utility
{
    AutoActivator::AutoActivator(std::shared_ptr<Input::InputHandler> input)
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
            if (m_SecondsLeft <= 0)
            {
                m_Input->ToggleActive(time);
                m_Activate = false;
                return;
            }
            if (0 == m_ActivationTime)
            {
                m_ActivationTime = time + ++m_SecondsLeft * 1000000000ll;
            }
            const int currentlyLeft = static_cast<int>((m_ActivationTime - time) / 1000000000ll);

            if (m_Countdown && currentlyLeft < m_SecondsLeft)
            {
                Feedback::AudioOut::CountDown(currentlyLeft);
            }
            m_SecondsLeft = currentlyLeft;
        }
    }

    DeltaMultiplier::DeltaMultiplier()
    {
        const auto cfg = GetConfig();
        bool apply{false};
        cfg->GetBool(Cfg::FactorApply, apply);
        cfg->GetFloat(Cfg::FactorHmdRoll, m_FactorRoll);
        cfg->GetFloat(Cfg::FactorHmdPitch, m_FactorPitch);
        cfg->GetFloat(Cfg::FactorHmdYaw, m_FactorYaw);
        cfg->GetFloat(Cfg::FactorHmdSurge, m_FactorSurge);
        cfg->GetFloat(Cfg::FactorHmdSway, m_FactorSway);
        cfg->GetFloat(Cfg::FactorHmdHeave, m_FactorHeave);

        SetApply(apply);
    }

    void DeltaMultiplier::SetApply(bool apply)
    {
        m_ApplyRotation = apply && (m_FactorRoll != 1.f || m_FactorPitch != 1.f || m_FactorYaw != 1.f);
        m_ApplyTranslation = apply && (m_FactorSurge != 1.f || m_FactorSway != 1.f || m_FactorHeave != 1.f);
    }

    void DeltaMultiplier::SetStageToLocal(const XrPosef& pose)
    {
        m_StageToLocal = pose;
        m_LocalToStage = Pose::Invert(pose);
    }

    void DeltaMultiplier::SetFwdToStage(const XrPosef& pose)
    {
        m_FwdToStage = pose;
        m_StageToFwd = Pose::Invert(pose);
    }
    void DeltaMultiplier::Apply(XrPosef& delta, const XrPosef& pose) const
    {
        if (!m_ApplyTranslation && !m_ApplyRotation)
        {
            return;
        }
        using namespace Pose;

        // transfer delta and original pose to forward space
        XrPosef deltaFwd = Multiply(Multiply(m_FwdToStage, delta), m_StageToFwd);
        const XrPosef poseStage = Multiply(pose, m_LocalToStage);
        const XrPosef poseFwd = Multiply(poseStage, m_StageToFwd);

        // calculate compensated pose
        XrPosef compensatedFwd = Multiply(poseFwd, deltaFwd);
        if (m_ApplyTranslation)
        {
            // apply translation scaling
            const XrVector3f translation = compensatedFwd.position - poseFwd.position;
            compensatedFwd.position =
                poseFwd.position +
                XrVector3f{translation.x * m_FactorSway, translation.y * m_FactorHeave, translation.z * m_FactorSurge};
        }
        if (m_ApplyRotation)
        {
            // apply rotation scaling
            XrVector3f angles = ToEulerAngles(deltaFwd.orientation);
            DirectX::XMVECTOR rotation = DirectX::XMQuaternionRotationRollPitchYaw(angles.x * m_FactorPitch,
                                                                                   angles.y * m_FactorYaw,
                                                                                   angles.z * m_FactorRoll);
            StoreXrQuaternion(&compensatedFwd.orientation,
                              DirectX::XMQuaternionMultiply(LoadXrQuaternion(poseFwd.orientation), rotation));
        }
        // calculate modified delta
        deltaFwd = Multiply(Invert(poseFwd), compensatedFwd);
        delta = Multiply(Multiply(m_StageToFwd, deltaFwd), m_FwdToStage);
    }

    XrVector3f DeltaMultiplier::ToEulerAngles(XrQuaternionf q)
    {
        XrVector3f angles;

        // pitch (x-axis rotation)
        float sinp = std::sqrt(1 + 2 * (q.w * q.x - q.z * q.y));
        float cosp = std::sqrt(1 - 2 * (q.w * q.x - q.z * q.y));
        angles.x = 2 * std::atan2f(sinp, cosp) - floatPi / 2;

        // yaw (y-axis rotation)
        float siny_cosp = 2 * (q.w * q.y + q.z * q.x);
        float cosy_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
        angles.y = std::atan2f(siny_cosp, cosy_cosp);

        // roll (z-axis rotation)
        float sinr_cosp = 2 * (q.w * q.z + q.x * q.y);
        float cosr_cosp = 1 - 2 * (q.z * q.z + q.x * q.x);
        angles.z = std::atan2f(sinr_cosp, cosr_cosp);

        return angles;
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

    bool Mmf::Open(const XrTime time)
    {
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
                ErrorLog("unable to map view of mmf %s: %s", m_Name.c_str(), LastErrorMsg().c_str());
                lock.unlock();
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
                return false;
            }
            return true;
        }
        return false;
    }

    void Mmf::Close()
    {
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
