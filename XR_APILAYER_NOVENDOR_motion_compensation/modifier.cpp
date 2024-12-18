// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include <DirectXMath.h>
#include "modifier.h"
#include "config.h"
#include "output.h"
#include <util.h>

using namespace xr::math;
using namespace Pose;
using namespace DirectX;
using namespace openxr_api_layer;
using namespace openxr_api_layer::log;

namespace modifier
{
    void ModifierBase::SetActive(const bool apply)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "ModifierBase::SetActive", TLArg(apply, "Apply"));

        m_ApplyRotation = apply && (m_Roll != 1.f || m_Pitch != 1.f || m_Yaw != 1.f);
        m_ApplyTranslation = apply && (m_Surge != 1.f || m_Sway != 1.f || m_Heave != 1.f);

        TraceLoggingWriteStop(local,
                              "ModifierBase::SetActive",
                              TLArg(m_ApplyRotation, "ApplyRotation"),
                              TLArg(m_ApplyTranslation, "ApplyTranslation"));
    }

    void ModifierBase::SetFwdToStage(const XrPosef& pose)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "ModifierBase::SetFwdToStage", TLArg(xr::ToString(pose).c_str(), "Pose"));

        m_FwdToStage = pose;
        m_StageToFwd = Invert(pose);

        TraceLoggingWriteStop(local,
                              "ModifierBase::SetFwdToStage",
                              TLArg(xr::ToString(m_StageToFwd).c_str(), "Inverted Pose"));
    }

    TrackerModifier::TrackerModifier()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerModifier::TrackerModifier");

        GetConfig()->GetFloat(Cfg::FactorTrackerRoll, m_Roll);
        GetConfig()->GetFloat(Cfg::FactorTrackerPitch, m_Pitch);
        GetConfig()->GetFloat(Cfg::FactorTrackerYaw, m_Yaw);
        GetConfig()->GetFloat(Cfg::FactorTrackerSurge, m_Surge);
        GetConfig()->GetFloat(Cfg::FactorTrackerSway, m_Sway);
        GetConfig()->GetFloat(Cfg::FactorTrackerHeave, m_Heave);

        bool apply{false};
        GetConfig()->GetBool(Cfg::FactorEnabled, apply);
        SetActive(apply);

        Log("pose modifier (experimental) at tracker position is %s",
            m_ApplyRotation ? (m_ApplyTranslation ? "manipulating rotation and translation" : "manipulating rotation")
                            : (m_ApplyTranslation ? "manipulating translation" : "off"));

        TraceLoggingWriteStop(local,
                              "TrackerModifier::TrackerModifier",
                              TLArg(std::to_string(this->m_Roll).c_str(), "Roll"),
                              TLArg(std::to_string(this->m_Pitch).c_str(), "Pitch"),
                              TLArg(std::to_string(this->m_Yaw).c_str(), "Yaw"),
                              TLArg(std::to_string(this->m_Surge).c_str(), "Surge"),
                              TLArg(std::to_string(this->m_Sway).c_str(), "Sway"),
                              TLArg(std::to_string(this->m_Heave).c_str(), "Heave"));
    }

    void TrackerModifier::Apply(XrPosef& target, const XrPosef& reference) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerModifier::Apply",
                               TLArg(xr::ToString(target).c_str(), "Original Target"),
                               TLArg(xr::ToString(reference).c_str(), "Reference"),
                               TLArg(this->m_ApplyRotation, "ApplyRotation"),
                               TLArg(this->m_ApplyTranslation, "ApplyTranslation"));

        if (!m_ApplyTranslation && !m_ApplyRotation)
        {
            TraceLoggingWriteStop(local, "TrackerModifier::Apply");
            return;
        }

        // transfer current and reference tracker pose to forward space
        XrPosef curFwd = Multiply(target, m_StageToFwd);
        const XrPosef refFwd = Multiply(reference, m_StageToFwd);

        if (m_ApplyRotation)
        {
            // apply rotation scaling
            const XrPosef deltaInvFwd = Multiply(Invert(refFwd), curFwd);
            XrVector3f angles = utility::ToEulerAngles(deltaInvFwd.orientation);
            TraceLoggingWriteTagged(local,
                                    "TrackerModifier::Apply",
                                    TLArg(xr::ToString(angles).c_str(), "Original Angles"));

            angles = {angles.x * m_Pitch, angles.y * m_Yaw, angles.z * m_Roll};
            TraceLoggingWriteTagged(local,
                                    "TrackerModifier::Apply",
                                    TLArg(xr::ToString(angles).c_str(), "Modified Angles"));

            const XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(angles.x, angles.y, angles.z);
            StoreXrQuaternion(&curFwd.orientation,
                              XMQuaternionMultiply(LoadXrQuaternion(refFwd.orientation), rotation));
        }
        if (m_ApplyTranslation)
        {
            // apply translation scaling
            XrVector3f translation = curFwd.position - refFwd.position;
            TraceLoggingWriteTagged(local,
                                    "TrackerModifier::Apply",
                                    TLArg(xr::ToString(translation).c_str(), "Original Translation"));

            translation = XrVector3f{translation.x * m_Sway, translation.y * m_Heave, translation.z * m_Surge};
            TraceLoggingWriteTagged(local,
                                    "TrackerModifier::Apply",
                                    TLArg(xr::ToString(translation).c_str(), "Modified Translation"));

            curFwd.position = refFwd.position + translation;
        }
        target = xr::Normalize(Multiply(curFwd, m_FwdToStage));

        TraceLoggingWriteStop(local, "TrackerModifier::Apply", TLArg(xr::ToString(target).c_str(), "Modified Target"));
    }

    HmdModifier::HmdModifier()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "HmdModifier::HmdModifier");

        GetConfig()->GetFloat(Cfg::FactorHmdRoll, m_Roll);
        GetConfig()->GetFloat(Cfg::FactorHmdPitch, m_Pitch);
        GetConfig()->GetFloat(Cfg::FactorHmdYaw, m_Yaw);
        GetConfig()->GetFloat(Cfg::FactorHmdSurge, m_Surge);
        GetConfig()->GetFloat(Cfg::FactorHmdSway, m_Sway);
        GetConfig()->GetFloat(Cfg::FactorHmdHeave, m_Heave);

        bool apply{false};
        GetConfig()->GetBool(Cfg::FactorEnabled, apply);
        bool compensateControllers{false};
        GetConfig()->GetBool(Cfg::CompensateControllers, compensateControllers);
        if (compensateControllers && apply &&
            (m_Roll != 1.f || m_Pitch != 1.f || m_Yaw != 1.f || m_Surge != 1.f || m_Sway != 1.f || m_Heave != 1.f))
        {
            ErrorLog("%s: compensating motion controllers and using pose modifier at hmd position are incompatible",
                     __FUNCTION__);
            Log("pose modifier at hmd position NOT activated");
            output::EventSink::Execute(output::Event::Error);

            // reset values to avoid activation via keyboard shortcut
            m_Pitch = 1.f;
            m_Roll = 1.f;
            m_Yaw = 1.f;
            m_Sway = 1.f;
            m_Heave = 1.f;
            m_Surge = 1.f;

            apply = false;
        }

        SetActive(apply);

        Log("pose modifier (experimental) at hmd position is %s",
            m_ApplyRotation ? (m_ApplyTranslation ? "manipulating rotation and translation" : "manipulating rotation")
                            : (m_ApplyTranslation ? "manipulating translation" : "off"));

        TraceLoggingWriteStop(local,
                              "HmdModifier::HmdModifier",
                              TLArg(std::to_string(this->m_Roll).c_str(), "Roll"),
                              TLArg(std::to_string(this->m_Pitch).c_str(), "Pitch"),
                              TLArg(std::to_string(this->m_Yaw).c_str(), "Yaw"),
                              TLArg(std::to_string(this->m_Surge).c_str(), "Surge"),
                              TLArg(std::to_string(this->m_Sway).c_str(), "Sway"),
                              TLArg(std::to_string(this->m_Heave).c_str(), "Heave"));
    }

    void HmdModifier::Apply(XrPosef& target, const XrPosef& reference) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "HmdModifier::Apply",
                               TLArg(xr::ToString(target).c_str(), "Original Target"),
                               TLArg(xr::ToString(reference).c_str(), "Reference"),
                               TLArg(this->m_ApplyRotation, "ApplyRotation"),
                               TLArg(this->m_ApplyTranslation, "ApplyTranslation"));

        if (!m_ApplyTranslation && !m_ApplyRotation)
        {
            TraceLoggingWriteStop(local, "HmdModifier::Apply");
            return;
        }

        // transfer delta and original pose to forward space
        const XrPosef deltaFwd = Multiply(Multiply(m_FwdToStage, target), m_StageToFwd);
        const XrPosef poseFwd = Multiply(reference, m_StageToFwd);

        // calculate compensated pose
        XrPosef compFwd = Multiply(poseFwd, deltaFwd);

        if (m_ApplyRotation)
        {
            // apply rotation scaling
            XrVector3f angles = utility::ToEulerAngles(Invert(deltaFwd).orientation);
            TraceLoggingWriteTagged(local,
                                    "HmdModifier::Apply",
                                    TLArg(xr::ToString(angles).c_str(), "Original Angles"));

            angles = {angles.x * m_Pitch, angles.y * m_Yaw, angles.z * m_Roll};
            TraceLoggingWriteTagged(local,
                                    "HmdModifier::Apply",
                                    TLArg(xr::ToString(angles).c_str(), "Modified Angles"));

            const XMVECTOR rotation = XMQuaternionRotationRollPitchYaw(angles.x, angles.y, angles.z);
            StoreXrQuaternion(
                &compFwd.orientation,
                XMQuaternionMultiply(LoadXrQuaternion(poseFwd.orientation), XMQuaternionInverse(rotation)));
        }
        if (m_ApplyTranslation)
        {
            // apply translation scaling
            XrVector3f translation = compFwd.position - poseFwd.position;
            TraceLoggingWriteTagged(local,
                                    "HmdModifier::Apply",
                                    TLArg(xr::ToString(translation).c_str(), "Original Translation"));

            translation = XrVector3f{translation.x * m_Sway, translation.y * m_Heave, translation.z * m_Surge};
            TraceLoggingWriteTagged(local,
                                    "HmdModifier::Apply",
                                    TLArg(xr::ToString(translation).c_str(), "Modified Translation"));

            compFwd.position = poseFwd.position + XrVector3f{translation.x, translation.y, translation.z};
        }

        // calculate modified delta
        const XrPosef newDeltaFwd = Multiply(Invert(poseFwd), compFwd);
        target = xr::Normalize(Multiply(Multiply(m_StageToFwd, newDeltaFwd), m_FwdToStage));

        TraceLoggingWriteStop(local, "HmdModifier::Apply", TLArg(xr::ToString(target).c_str(), "Modified Target"));
    }
} // namespace modifier