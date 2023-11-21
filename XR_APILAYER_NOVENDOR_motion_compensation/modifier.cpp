// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include <DirectXMath.h>
#include "modifier.h"
#include "config.h"
#include "utility.h"


using namespace xr::math;
using namespace Pose;
using namespace DirectX;

namespace Modifier
{
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
        angles.x = 2 * std::atan2f(sinp, cosp) - utility::floatPi / 2;

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
        GetConfig()->GetBool(Cfg::FactorEnabled, apply);
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
        GetConfig()->GetBool(Cfg::FactorEnabled, apply);
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
                poseFwd.position + XrVector3f{translation.x * m_Sway, translation.y * m_Heave, translation.z * m_Surge};
        }

        // calculate modified delta
        const XrPosef newDeltaFwd = Multiply(Invert(poseFwd), compFwd);
        target = Multiply(Multiply(m_StageToFwd, newDeltaFwd), m_FwdToStage);
    }
} // namespace Modifier