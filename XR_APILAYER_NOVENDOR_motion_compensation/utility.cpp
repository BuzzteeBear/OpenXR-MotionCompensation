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
using namespace Pose;
using namespace DirectX;

namespace utility
{
    AutoActivator::AutoActivator(const std::shared_ptr<Input::InputHandler>& input)
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

    void ModifierBase::SetActive(const bool apply)
    {
        m_ApplyRotation = apply && (m_Roll != 1.f || m_Pitch != 1.f || m_Yaw != 1.f);
        m_ApplyTranslation = apply && (m_Surge != 1.f || m_Sway != 1.f || m_Heave != 1.f);
    }

    void ModifierBase::SetStageToLocal(const XrPosef& pose)
    {
        m_StageToLocal = pose;
        m_LocalToStage = Invert(pose);
    }

    void ModifierBase::SetFwdToStage(const XrPosef& pose)
    {
        m_FwdToStage = pose;
        m_StageToFwd = Invert(pose);
    }

    XrVector3f ModifierBase::ToEulerAngles(XrQuaternionf q)
    {
        XrVector3f angles;

        // pitch (x-axis rotation)
        const float sinp = std::sqrt(1 + 2 * (q.w * q.x - q.z * q.y));
        const float cosp = std::sqrt(1 - 2 * (q.w * q.x - q.z * q.y));
        angles.x = 2 * std::atan2f(sinp, cosp) - floatPi / 2;

        // yaw (y-axis rotation)
        const float siny_cosp = 2 * (q.w * q.y + q.z * q.x);
        const float cosy_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
        angles.y = std::atan2f(siny_cosp, cosy_cosp);

        // roll (z-axis rotation)
        const float sinr_cosp = 2 * (q.w * q.z + q.x * q.y);
        const float cosr_cosp = 1 - 2 * (q.z * q.z + q.x * q.x);
        angles.z = std::atan2f(sinr_cosp, cosr_cosp);

        return angles;
    }

    TrackerModifier::TrackerModifier()
    {
        GetConfig()->GetFloat(Cfg::FactorTrackerRoll, m_Roll);
        GetConfig()->GetFloat(Cfg::FactorTrackerPitch, m_Pitch);
        GetConfig()->GetFloat(Cfg::FactorTrackerYaw, m_Yaw);
        GetConfig()->GetFloat(Cfg::FactorTrackerSurge, m_Surge);
        GetConfig()->GetFloat(Cfg::FactorTrackerSway, m_Sway);
        GetConfig()->GetFloat(Cfg::FactorTrackerHeave, m_Heave);

        bool apply{false};
        GetConfig()->GetBool(Cfg::FactorApply, apply);
        SetActive(apply);
    }

    void TrackerModifier::Apply(XrPosef& target, const XrPosef& reference) const
    {
        if (!m_ApplyTranslation && !m_ApplyRotation)
        {
            return;
        }

        // transfer current and reference tracker pose to forward space
        XrPosef curFwd = Multiply(target, m_StageToFwd);
        const XrPosef refFwd = Multiply(reference, m_StageToFwd);

        if (m_ApplyRotation)
        {
            // apply rotation scaling
            const XrPosef deltaInvFwd = Multiply(Invert(refFwd), curFwd);
            const XrVector3f angles = ToEulerAngles(deltaInvFwd.orientation);
            const XMVECTOR rotation =
                XMQuaternionRotationRollPitchYaw(angles.x * m_Pitch, angles.y * m_Yaw, angles.z * m_Roll);
            StoreXrQuaternion(&curFwd.orientation,
                              XMQuaternionMultiply(LoadXrQuaternion(refFwd.orientation), rotation));
        }
        if (m_ApplyTranslation)
        {
            // apply translation scaling
            const XrVector3f translation = curFwd.position - refFwd.position;
            curFwd.position =
                refFwd.position + XrVector3f{translation.x * m_Sway, translation.y * m_Heave, translation.z * m_Surge};
        }
        target = Multiply(curFwd, m_FwdToStage);
    }

    HmdModifier::HmdModifier()
    {
        GetConfig()->GetFloat(Cfg::FactorHmdRoll, m_Roll);
        GetConfig()->GetFloat(Cfg::FactorHmdPitch, m_Pitch);
        GetConfig()->GetFloat(Cfg::FactorHmdYaw, m_Yaw);
        GetConfig()->GetFloat(Cfg::FactorHmdSurge, m_Surge);
        GetConfig()->GetFloat(Cfg::FactorHmdSway, m_Sway);
        GetConfig()->GetFloat(Cfg::FactorHmdHeave, m_Heave);

        bool apply{false};
        GetConfig()->GetBool(Cfg::FactorApply, apply);
        SetActive(apply);
    }

    void HmdModifier::Apply(XrPosef& target, const XrPosef& reference) const
    {
        if (!m_ApplyTranslation && !m_ApplyRotation)
        {
            return;
        }

        // transfer delta and original pose to forward space
        const XrPosef deltaFwd = Multiply(Multiply(m_FwdToStage, target), m_StageToFwd);
        const XrPosef poseStage = Multiply(reference, m_LocalToStage);
        const XrPosef poseFwd = Multiply(poseStage, m_StageToFwd);

        // calculate compensated pose
        XrPosef compFwd = Multiply(poseFwd, deltaFwd);

        if (m_ApplyRotation)
        {
            // apply rotation scaling
            const XrVector3f angles = ToEulerAngles(Invert(deltaFwd).orientation);
            const XMVECTOR rotation =
                XMQuaternionRotationRollPitchYaw(angles.x * m_Pitch, angles.y * m_Yaw, angles.z * m_Roll);
            StoreXrQuaternion(
                &compFwd.orientation,
                XMQuaternionMultiply(LoadXrQuaternion(poseFwd.orientation), XMQuaternionInverse(rotation)));
        }
        if (m_ApplyTranslation)
        {
            // apply translation scaling
            const XrVector3f translation = compFwd.position - poseFwd.position;
            compFwd.position =
                poseFwd.position +
                XrVector3f{translation.x * m_Sway, translation.y * m_Heave, translation.z * m_Surge};
        }

        // calculate modified delta
        const XrPosef newDeltaFwd = Multiply(Invert(poseFwd), compFwd);
        target = Multiply(Multiply(m_StageToFwd, newDeltaFwd), m_FwdToStage);
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
