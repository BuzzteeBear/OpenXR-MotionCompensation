// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "layer.h"
#include "feedback.h"
#include <log.h>
#include <util.h>

using namespace openxr_api_layer;
using namespace log;
using namespace Feedback;
using namespace xr::math;
using namespace utility;

namespace Tracker
{
    bool ControllerBase::Init()
    {
        GetConfig()->GetBool(Cfg::PhysicalEnabled, m_PhysicalEnabled);
        return true;
    }

    bool ControllerBase::GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time)
    {
        // pose already calculated for requested time
        if (time == m_LastPoseTime)
        {
            // already calculated for requested time;
            poseDelta = m_LastPoseDelta;
            TraceLoggingWrite(g_traceProvider,
                              "GetPoseDelta",
                              TLArg(xr::ToString(m_LastPoseDelta).c_str(), "LastDelta"));
            DebugLog("delta(%u) reused", time);
            return true;
        }
        if (XrPosef curPose{Pose::Identity()}; GetPose(curPose, session, time))
        {
            ApplyFilters(curPose);
            ApplyModifier(curPose);

            // calculate difference toward reference pose
            poseDelta = Pose::Multiply(Pose::Invert(curPose), m_ReferencePose);
            TraceLoggingWrite(g_traceProvider, "GetPoseDelta", TLArg(xr::ToString(poseDelta).c_str(), "Delta"));
            DebugLog("delta(%u): %s", time, xr::ToString(m_LastPoseDelta).c_str());
            m_LastPose = curPose;
            m_LastPoseDelta = poseDelta;
            m_LastPoseTime = time;
            return true;
        }
        return false;
    }

    bool ControllerBase::ResetReferencePose(XrSession session, XrTime time)
    {
        if (XrPosef curPose; GetPose(curPose, session, time))
        {
            SetReferencePose(curPose);
            return true;
        }
        else
        {
            ErrorLog("%s: unable to get current pose", __FUNCTION__);
            return false;
        }
    }

    void ControllerBase::SetReferencePose(const XrPosef& pose)
    {
        m_ReferencePose = pose;
        TraceLoggingWrite(g_traceProvider, "SetReferencePose", TLArg(xr::ToString(pose).c_str(), "ReferencePose"));
        Log("tracker reference pose set");
    }

    bool ControllerBase::GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        if (!m_PhysicalEnabled)
        {
            ErrorLog("physical tracker disabled in config file");
            return false;
        }
        if (auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance()))
        {
            // Query the latest tracker pose.
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
            if (!m_XrSyncCalled)
            {
                constexpr XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr, 0, nullptr};

                TraceLoggingWrite(g_traceProvider,
                                  "GetControllerPose",
                                  TLPArg(layer->m_ActionSet, "xrSyncActions"),
                                  TLArg(time, "Time"));
                if (const XrResult result = GetInstance()->xrSyncActions(session, &syncInfo); XR_FAILED(result))
                {
                    ErrorLog("%s: xrSyncActions failed: %s", __FUNCTION__, xr::ToCString(result));
                    return false;
                }
            }
            {
                XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
                XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
                getActionStateInfo.action = layer->m_PoseAction;

                TraceLoggingWrite(g_traceProvider,
                                  "GetControllerPose",
                                  TLPArg(layer->m_PoseAction, "xrGetActionStatePose"),
                                  TLArg(time, "Time"));
                if (const XrResult result =
                        GetInstance()->xrGetActionStatePose(session, &getActionStateInfo, &actionStatePose);
                    XR_FAILED(result))
                {
                    ErrorLog("%s: xrGetActionStatePose failed: %s", __FUNCTION__, xr::ToCString(result));
                    return false;
                }

                if (!actionStatePose.isActive)
                {
                    if (!m_ConnectionLost)
                    {
                        ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active", __FUNCTION__);
                        m_ConnectionLost = true;
                    }
                    return false;
                }
            }

            if (const XrResult result =
                    GetInstance()->xrLocateSpace(layer->m_TrackerSpace, layer->m_StageSpace, time, &location);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrLocateSpace failed: %s", __FUNCTION__, xr::ToCString(result));
                return false;
            }

            if (!Pose::IsPoseValid(location.locationFlags))
            {
                if (!m_ConnectionLost)
                {
                    ErrorLog("%s: unable to determine tracker pose - XrSpaceLocation not valid", __FUNCTION__);
                    m_ConnectionLost = true;
                }
                return false;
            }
            TraceLoggingWrite(g_traceProvider,
                              "GetControllerPose",
                              TLArg(xr::ToString(location.pose).c_str(), "Location"),
                              TLArg(time, "Time"));
            m_ConnectionLost = false;
            trackerPose = location.pose;
            return true;
        }
        else
        {
            ErrorLog("%s: unable to cast instance to OpenXrLayer", __FUNCTION__);
            return false;
        }
    }

    XrVector3f ControllerBase::GetForwardVector(const XrQuaternionf& quaternion, bool inverted)
    {
        XrVector3f forward;
        StoreXrVector3(
            &forward,
            DirectX::XMVector3Rotate(LoadXrVector3(XrVector3f{0.0f, 0.0f, inverted ? 1.0f : -1.0f}), LoadXrQuaternion(quaternion)));
        forward.y = 0;
        return Normalize(forward);
    }

    XrQuaternionf ControllerBase::GetYawRotation(const XrVector3f& forward, float yawAdjustment)
    {
        XrQuaternionf rotation{};
        StoreXrQuaternion(&rotation,
                          DirectX::XMQuaternionRotationRollPitchYaw(0.0f, GetYawAngle(forward) + yawAdjustment, 0.0f));
        return rotation;
    }

    float ControllerBase::GetYawAngle(const XrVector3f& forward)
    {
        return atan2f(forward.x, forward.z);
    }

    TrackerBase::~TrackerBase()
    {
        delete m_TransFilter;
        delete m_RotFilter;
    }

    bool TrackerBase::Init()
    {
        m_TrackerModifier = std::make_unique<Modifier::TrackerModifier>();
        ControllerBase::Init();
        return LoadFilters();
    }

    bool TrackerBase::LazyInit(XrTime time)
    {
        m_SkipLazyInit = true;
        return true;
    }

    bool TrackerBase::LoadFilters()
    {
        // set up filters
        int orderTrans = 2, orderRot = 2;
        float strengthTrans = 0.0f, strengthRot = 0.0f;
        if (!GetConfig()->GetInt(Cfg::TransOrder, orderTrans) || !GetConfig()->GetInt(Cfg::RotOrder, orderRot) ||
            !GetConfig()->GetFloat(Cfg::TransStrength, strengthTrans) ||
            !GetConfig()->GetFloat(Cfg::RotStrength, strengthRot))
        {
            ErrorLog("%s: error reading configured values for filters", __FUNCTION__);
        }
        if (1 > orderTrans || 3 < orderTrans)
        {
            ErrorLog("%s: invalid order for translational filter: %d", __FUNCTION__, orderTrans);
            return false;
        }
        if (1 > orderRot || 3 < orderRot)
        {
            ErrorLog("%s: invalid order for rotational filter: %d", __FUNCTION__, orderRot);
            return false;
        }
        // remove previous filter objects

        delete m_TransFilter;
        delete m_RotFilter;

        m_TransStrength = strengthTrans;
        m_RotStrength = strengthRot;

        Log("translational filter stages: %d", orderTrans);
        Log("translational filter strength: %f", m_TransStrength);
        m_TransFilter = 1 == orderTrans   ? new Filter::SingleEmaFilter(m_TransStrength)
                        : 2 == orderTrans ? new Filter::DoubleEmaFilter(m_TransStrength)
                                          : new Filter::TripleEmaFilter(m_TransStrength);

        Log("rotational filter stages: %d", orderRot);
        Log("rotational filter strength: %f", m_TransStrength);
        m_RotFilter = 1 == orderRot   ? new Filter::SingleSlerpFilter(m_RotStrength)
                      : 2 == orderRot ? new Filter::DoubleSlerpFilter(m_RotStrength)
                                      : new Filter::TripleSlerpFilter(m_RotStrength);

        return true;
    }

    void TrackerBase::ModifyFilterStrength(const bool trans, const bool increase, const bool fast)
    {
        float* currentValue = trans ? &m_TransStrength : &m_RotStrength;
        const float prevValue = *currentValue;
        const float amount = (1.1f - *currentValue) * (fast ? 0.25f : 0.05f);
        const float newValue = *currentValue + (increase ? amount : -amount);
        if (trans)
        {
            *currentValue = m_TransFilter->SetStrength(newValue);
            GetConfig()->SetValue(Cfg::TransStrength, *currentValue);
            Log("translational filter strength %screased to %f", increase ? "in" : "de", *currentValue);
        }
        else
        {
            *currentValue = m_RotFilter->SetStrength(newValue);
            GetConfig()->SetValue(Cfg::RotStrength, *currentValue);
            Log("rotational filter strength %screased to %f", increase ? "in" : "de", *currentValue);
        }
        GetAudioOut()->Execute(*currentValue == prevValue ? increase ? Event::Max : Event::Min
                               : increase                 ? Event::Plus
                                                          : Event::Minus);
    }

    void TrackerBase::AdjustReferencePose(const XrPosef& pose)
    {
        SetReferencePose(Pose::Multiply(m_ReferencePose, pose));
    }

    XrPosef TrackerBase::GetReferencePose() const
    {
        return m_ReferencePose;
    }

    void TrackerBase::LogCurrentTrackerPoses(XrSession session, XrTime time, bool activated)
    {
        if (!m_Calibrated)
        {
            ErrorLog("%s: tracker not calibrated", __FUNCTION__);
            return;
        }
        Log("current reference pose in stage space: %s", xr::ToString(m_ReferencePose).c_str());
        XrPosef currentPose{Pose::Identity()};
        if (activated && GetPose(currentPose, session, time))
        {
            Log("current tracker pose in stage space: %s", xr::ToString(currentPose).c_str());
        }
    }

    void TrackerBase::SetReferencePose(const XrPosef& pose)
    {
        m_TransFilter->Reset(pose.position);
        m_RotFilter->Reset(pose.orientation);
        m_Calibrated = true;
        ControllerBase::SetReferencePose(pose); 
    }

    void TrackerBase::SetStageToLocal(const XrPosef& pose) const
    {
        m_TrackerModifier->SetStageToLocal(pose);
    }

    void TrackerBase::SetModifierActive(const bool active) const
    {
        m_TrackerModifier->SetActive(active);
    }

    void TrackerBase::ApplyFilters(XrPosef& pose)
    {
        // apply translational filter
        m_TransFilter->Filter(pose.position);

        // apply rotational filter
        m_RotFilter->Filter(pose.orientation);

        TraceLoggingWrite(g_traceProvider,
                          "ApplyFilters",
                          TLArg(xr::ToString(pose).c_str(), "LocationAfterFilter"));
    }

    void TrackerBase::ApplyModifier(XrPosef& pose)
    {
        m_TrackerModifier->Apply(pose, m_ReferencePose);
    }

    bool TrackerBase::CalibrateForward(XrSession session, XrTime time, float yawOffset)
    {
        if (auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance()); !layer)
        {
            ErrorLog("%s: cast of layer failed", __FUNCTION__);
            return false;
        }
        else
        {
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
            if (const XrResult result =
                    layer->OpenXrApi::xrLocateSpace(layer->m_ViewSpace, layer->m_StageSpace, time, &location);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrLocateSpace(view) failed: %s", __FUNCTION__, xr::ToCString(result));
                return false;
            }
            if (!Pose::IsPoseValid(location.locationFlags))
            {
                ErrorLog("%s: view space location invalid", __FUNCTION__);
                return false;
            }
            // project forward and right vector of view onto 'floor plane'
            m_Forward = GetForwardVector(location.pose.orientation);
            m_Right = XrVector3f{-m_Forward.z, 0.0f, m_Forward.x};

            // calculate orientation parallel to floor
            location.pose.orientation = GetYawRotation(m_Forward, yawOffset);

            m_ForwardPose = location.pose;
        }
        return true;
    }

    void TrackerBase::SetForwardRotation(const XrPosef& pose) const
    {
        // update forward rotation
        const XrPosef fwdRotation = {pose.orientation, {0.f, 0.f, 0.f}};
        m_TrackerModifier->SetFwdToStage(fwdRotation);
        const auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if ( !layer)
        {
            ErrorLog("%s: cast of layer failed", __FUNCTION__);
        }
        layer->SetForwardRotation(fwdRotation);
    }

    bool OpenXrTracker::ResetReferencePose(XrSession session, XrTime time)
    {
        CalibrateForward(session, time, 0.f);
        SetForwardRotation(m_ForwardPose);
        if (!ControllerBase::ResetReferencePose(session, time))
        {
            m_Calibrated = false;
            return false;
        }
        return true;
    }

    bool OpenXrTracker::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        return GetControllerPose(trackerPose, session, time);
    }

    bool VirtualTracker::Init()
    {
        m_Manipulator = std::make_unique<CorManipulator>(CorManipulator(this));
        m_Manipulator->Init();
        bool success{true};
        if(!GetConfig()->GetBool(Cfg::UpsideDown, m_UpsideDown))
        {
            success = false; 
        }
        Log("Virtual tracker is%s upside down", m_UpsideDown ? "" : " not");
        float value;
        if (GetConfig()->GetFloat(Cfg::TrackerOffsetForward, value))
        {
            m_OffsetForward = value / 100.0f;
        }
        else
        {
            success = false;
        }
        if (GetConfig()->GetFloat(Cfg::TrackerOffsetDown, value))
        {
            m_OffsetDown = value / 100.0f;
        }
        else
        {
            success = false;
        }
        if (GetConfig()->GetFloat(Cfg::TrackerOffsetRight, value))
        {
            m_OffsetRight = value / 100.0f;
        }
        else
        {
            success = false;
        }
        if (GetConfig()->GetFloat(Cfg::TrackerOffsetYaw, value))
        {
            m_OffsetYaw = fmod(value * angleToRadian + floatPi, 2.0f * floatPi) - floatPi;
        }
        else
        {
            success = false;
        }
        LogOffsetValues();

        if (GetConfig()->GetBool(Cfg::UseCorPos, m_LoadPoseFromFile))
        {
            Log("center of rotation is %s read from config file", m_LoadPoseFromFile ? "" : "not");
        }
        else
        {
            success = false;
        }
        if (!TrackerBase::Init())
        {
            success = false;
        }
        return success;
    }

    bool VirtualTracker::LazyInit(const XrTime time)
    {
        bool success = true;
        if (!m_SkipLazyInit)
        {
            m_Mmf.SetName(m_Filename);

            if (!m_Mmf.Open(time))
            {
                ErrorLog("unable to open mmf '%s'. Check if motion software is running and data output is activated!",
                         m_Filename.c_str());
                success = false;
            }
        }
        m_SkipLazyInit = success;
        return success;
    }

    bool VirtualTracker::ResetReferencePose(const XrSession session, const XrTime time)
    {
        bool success = true;
        if (m_LoadPoseFromFile)
        {
            success = LoadReferencePose(session, time);
        }
        else
        {
            if (!CalibrateForward(session, time, m_OffsetYaw))
            {
                success = false;
            }
            else
            {
                XrPosef refPose = m_ForwardPose;
                const float offsetRight = m_UpsideDown ? -m_OffsetRight : m_OffsetRight;
                const float offsetDown = m_UpsideDown ? -m_OffsetDown : m_OffsetDown;

                // calculate and apply translational offset
                const XrVector3f offset =
                    m_OffsetForward * m_Forward + offsetRight * m_Right + XrVector3f{0.0f, -offsetDown, 0.0f};
                refPose.position = refPose.position + offset;
                TrackerBase::SetReferencePose(refPose);
            }
        }

        m_Calibrated = success;
        return success;
    }

    bool VirtualTracker::ChangeOffset(XrVector3f modification)
    {
        if (!m_Calibrated)
        {
            ErrorLog("please calibrate cor position before trying to move it");
            return false;
        }
        TraceLoggingWrite(g_traceProvider,
                          "ChangeOffset",
                          TLArg(modification.z, "Fwd Modification"),
                          TLArg(-modification.y, "Down Modification"),
                          TLArg(-modification.x, "Right Modification"));
        const XrVector3f relativeToHmd{cosf(m_OffsetYaw) * modification.x + sinf(m_OffsetYaw) * modification.z,
                                       modification.y,
                                       cosf(m_OffsetYaw) * modification.z - sinf(m_OffsetYaw) * modification.x};
        TraceLoggingWrite(g_traceProvider,
                          "ChangeOffset",
                          TLArg(m_OffsetYaw, "Yaw Radian"),
                          TLArg(relativeToHmd.z, "Fwd Relative"),
                          TLArg(-relativeToHmd.x, "Right Relative"));
        m_OffsetForward += relativeToHmd.z;
        m_OffsetDown -= relativeToHmd.y;
        m_OffsetRight -= relativeToHmd.x;
        GetConfig()->SetValue(Cfg::TrackerOffsetForward, m_OffsetForward * 100.0f);
        GetConfig()->SetValue(Cfg::TrackerOffsetDown, m_OffsetDown * 100.0f);
        GetConfig()->SetValue(Cfg::TrackerOffsetRight, m_OffsetRight * 100.0f);

        TraceLoggingWrite(g_traceProvider,
                          "ChangeOffset",
                          TLArg(m_OffsetForward, "OffsetForward"),
                          TLArg(m_OffsetDown, "OffsetDown"),
                          TLArg(m_OffsetRight, "OffsetForward"));

        if (m_UpsideDown)
        {
            modification.y = -modification.y;
            modification.x = -modification.x;
        }

        const XrPosef adjustment{{Quaternion::Identity()}, modification};
        m_ReferencePose = Pose::Multiply(adjustment, m_ReferencePose);
        TraceLoggingWrite(g_traceProvider,
                          "ChangeOffset",
                          TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        return true;
    }

    bool VirtualTracker::ChangeRotation(float radian)
    {
        if (!m_Calibrated)
        {
            ErrorLog("please calibrate cor position before trying to rotate it");
            return false;
        }
        TraceLoggingWrite(g_traceProvider, "ChangeRotation", TLArg(m_OffsetYaw, "Old Angle"), TLArg(radian, "Radian"));
        XrPosef adjustment{Pose::Identity()};
        const float direction = radian * (m_UpsideDown ? -1.0f : 1.0f);
        m_OffsetYaw += direction;
        // restrict to +- pi
        m_OffsetYaw = fmod(m_OffsetYaw + floatPi, 2.0f * floatPi) - floatPi;
        const float yawAngle = m_OffsetYaw / angleToRadian;
        GetConfig()->SetValue(Cfg::TrackerOffsetYaw, yawAngle);
        StoreXrQuaternion(&adjustment.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(0.0f, direction , 0.0f));
        TraceLoggingWrite(g_traceProvider,
                          "ChangeRotation",
                          TLArg(m_OffsetYaw, "New Angle"),
                          TLArg(direction, "Direction"),
                          TLArg(yawAngle, "Angle"));
        SetReferencePose(Pose::Multiply(adjustment, m_ReferencePose));
        return true;
    }

    void VirtualTracker::SaveReferencePose(const XrTime time) const
    {
        if (m_Calibrated)
        {
            GetConfig()->SetValue(Cfg::CorX, m_ReferencePose.position.x);
            GetConfig()->SetValue(Cfg::CorY, m_ReferencePose.position.y);
            GetConfig()->SetValue(Cfg::CorZ, m_ReferencePose.position.z);
            GetConfig()->SetValue(Cfg::CorA, m_ReferencePose.orientation.w);
            GetConfig()->SetValue(Cfg::CorB, m_ReferencePose.orientation.x);
            GetConfig()->SetValue(Cfg::CorC, m_ReferencePose.orientation.y);
            GetConfig()->SetValue(Cfg::CorD, m_ReferencePose.orientation.z);
        }
    }

    
    bool VirtualTracker::LoadReferencePose(const XrSession session, const XrTime time)
    {
        bool success = true;
        XrPosef refPose;
        auto ReadValue = [&success](Cfg key, float& value) {
            if (!GetConfig()->GetFloat(key, value))
            {
                success = false;
            }
        };
        ReadValue(Cfg::CorX, refPose.position.x);
        ReadValue(Cfg::CorY, refPose.position.y);
        ReadValue(Cfg::CorZ, refPose.position.z);
        ReadValue(Cfg::CorA, refPose.orientation.w);
        ReadValue(Cfg::CorB, refPose.orientation.x);
        ReadValue(Cfg::CorC, refPose.orientation.y);
        ReadValue(Cfg::CorD, refPose.orientation.z);
        if (success && !Quaternion::IsNormalized(refPose.orientation))
        {
            ErrorLog("%s: rotation values are invalid in config file", __FUNCTION__);
            Log("you may need to save cor position separately for native OpenXR and OpenComposite");
            return false;
        }
        if (success)
        {
            Log("reference pose successfully loaded from config file");
            SetReferencePose(refPose);    
        }
        return success;
    }

    void VirtualTracker::LogOffsetValues() const
    {
        Log("offset values: forward = %.3f m, down = %.3f m, right = %.3f m, yaw = %.3f deg,   ", m_OffsetForward, m_OffsetDown, m_OffsetRight, m_OffsetYaw / angleToRadian);
    }

    void VirtualTracker::ApplyCorManipulation(XrSession session, XrTime time)
    {
        if (m_Manipulator)
        {
            m_Manipulator->ApplyManipulation(session, time);
        }
    }

    void VirtualTracker::SetReferencePose(const ::XrPosef& pose)
    {
        SetForwardRotation(pose);
        TrackerBase::SetReferencePose(pose);
    }

    bool VirtualTracker::GetPose(XrPosef& trackerPose, const XrSession session, const XrTime time)
    {
        return GetVirtualPose(trackerPose, session, time);
    }
    
    bool YawTracker::ResetReferencePose(XrSession session, XrTime time)
    {
        bool useGameEngineValues;
        if (GetConfig()->GetBool(Cfg::UseYawGeOffset, useGameEngineValues) && useGameEngineValues)
        {
            // calculate difference between floor and headset and set offset according to file values
            if (const auto layer = reinterpret_cast<OpenXrLayer*>(GetInstance()))
            {
                XrSpaceLocation hmdLocation{XR_TYPE_SPACE_LOCATION, nullptr};
                const XrResult result =
                    layer->OpenXrApi::xrLocateSpace(layer->m_ViewSpace, layer->m_StageSpace, time, &hmdLocation);
                if (XR_SUCCEEDED(result))
                {
                    if (Pose::IsPoseValid(hmdLocation.locationFlags))
                    {
                        YawData data{};
                        if (m_Mmf.Read(&data, sizeof(data), time))
                        {
                            Log("Yaw Game Engine values: rotationHeight = %f, rotationForwardHead = %f",
                                data.rotationHeight,
                                data.rotationForwardHead);

                            m_OffsetRight = 0.0;
                            m_OffsetForward = data.rotationForwardHead / 100.0f;
                            m_OffsetDown = hmdLocation.pose.position.y - data.rotationHeight / 100.0f;

                            Log("offset down value based on Yaw Game Engine value: %f", m_OffsetDown);
                        }
                        else
                        {
                            ErrorLog("%s: unable to use Yaw GE offset values: could not read mmf file", __FUNCTION__);
                        }
                    }
                    else
                    {
                        ErrorLog("%s: unable to use Yaw GE offset values: pose invalid", __FUNCTION__);
                    }
                }
                else
                {
                    ErrorLog("%s: unable to use Yaw GE offset values: xrLocateSpace(view) failed: %s",
                             __FUNCTION__,
                             xr::ToCString(result));
                }
            }
            else
            {
                ErrorLog("%s: unable to use Yaw GE offset values: cast of layer failed", __FUNCTION__);
            }
        }
        return VirtualTracker::ResetReferencePose(session, time);
    }

    bool YawTracker::GetVirtualPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        YawData data{};
        XrPosef rotation{Pose::Identity()};
        if (!m_Mmf.Read(&data, sizeof(data), time))
        {
            return false;
        }

        DebugLog("YawData:\n\tyaw: %f, pitch: %f, roll: %f\n\tbattery: %f, rotationHeight: %f, "
                 "rotationForwardHead: %f\n\tsixDof: %d, usePos: %d, autoX: %f, autoY: %f",
                 data.yaw,
                 data.pitch,
                 data.roll,
                 data.battery,
                 data.rotationHeight,
                 data.rotationForwardHead,
                 data.sixDof,
                 data.usePos,
                 data.autoX,
                 data.autoY);

        TraceLoggingWrite(g_traceProvider,
                          "YawTracker::GetVirtualPose",
                          TLArg(data.yaw, "Yaw"),
                          TLArg(data.pitch, "Pitch"),
                          TLArg(data.roll, "Yaw"),
                          TLArg(data.battery, "Battery"),
                          TLArg(data.rotationHeight, "RotationHeight"),
                          TLArg(data.rotationForwardHead, "RotationForwardHead"),
                          TLArg(data.sixDof, "SixDof"),
                          TLArg(data.usePos, "UsePos"),
                          TLArg(data.autoX, "AutoX"),
                          TLArg(data.autoY, "AutoY"));

        if (m_UpsideDown)
        {
            data.pitch = -data.pitch;
            data.yaw = -data.yaw;
        }

        StoreXrQuaternion(&rotation.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(data.pitch * angleToRadian,
                                                                    -data.yaw * angleToRadian,
                                                                    -data.roll * angleToRadian));

        trackerPose = Pose::Multiply(rotation, m_ReferencePose);
        return true;
    }

    bool SixDofTracker::GetVirtualPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        SixDofData data{};
        XrPosef rigPose{Pose::Identity()};
        if (!m_Mmf.Read(&data, sizeof(data), time))
        {
            return false;
        }

        DebugLog("MotionData:\n\tyaw: %f, pitch: %f, roll: %f\n\tsway: %f, surge: %f, heave: %f",
                 data.yaw,
                 data.pitch,
                 data.roll,
                 data.sway,
                 data.surge,
                 data.heave);

        TraceLoggingWrite(g_traceProvider,
                          "SixDofTracker::GetVirtualPose",
                          TLArg(data.yaw, "Yaw"),
                          TLArg(data.pitch, "Pitch"),
                          TLArg(data.roll, "Roll"),
                          TLArg(data.sway, "Sway"),
                          TLArg(data.surge, "Surge"),
                          TLArg(data.heave, "Heave"));

        if (m_UpsideDown)
        {
            data.pitch = -data.pitch;
            data.yaw = -data.yaw;
            data.sway = -data.sway;
            data.heave = -data.heave;
        }

        StoreXrQuaternion(&rigPose.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(
                              static_cast<float>(data.pitch) * (m_IsSrs ? angleToRadian : -angleToRadian),
                              static_cast<float>(data.yaw) * angleToRadian,
                              static_cast<float>(data.roll) * (m_IsSrs ? -angleToRadian : angleToRadian)));
        rigPose.position = XrVector3f{static_cast<float>(data.sway) / -1000.0f,
                                      static_cast<float>(data.heave) / 1000.0f,
                                      static_cast<float>(data.surge) / 1000.0f};

        trackerPose = Pose::Multiply(rigPose, m_ReferencePose);
        return true;
    }
    
    void CorManipulator::ApplyManipulation(XrSession session, XrTime time)
    {
        if (!m_Initialized)
        {
            return;
        }

        auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (!layer)
        {
            m_Initialized = false;
            ErrorLog("%s: unable to cast layer to OpenXrLayer", __FUNCTION__);
            return;
        }
        
        bool movePressed{false}, positionPressed{false};
        GetButtonState(session, movePressed, positionPressed);

        // apply vibration to acknowledge button state change
        if ((positionPressed && !m_PositionActive) ||  movePressed != m_MoveActive)
        {
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude = 0.75;
            vibration.duration = XR_MIN_HAPTIC_DURATION;
            vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

            TraceLoggingWrite(g_traceProvider,
                              "ApplyManipulation",
                              TLPArg(layer->m_HapticAction, "xrApplyHapticFeedback"));
            XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = layer->m_HapticAction;
            if (const XrResult result = layer->xrApplyHapticFeedback(session,
                                                                     &hapticActionInfo,
                                                                     reinterpret_cast<XrHapticBaseHeader*>(&vibration));
                XR_FAILED(result))
            {
                ErrorLog("%s: xrApplyHapticFeedback failed: %s", __FUNCTION__, xr::ToCString(result));
            }
        }

        
        if (!positionPressed) 
        {
            // reset reference pose on move activation
            if (movePressed && !m_MoveActive)
            {
                ResetReferencePose(session, time);
            }
            // log new offset values on move deactivation
            if (!movePressed && m_MoveActive)
            {
                m_Tracker->LogOffsetValues();
            }
        }
        // apply actual manipulation
        if (movePressed || positionPressed)
        {
            if (XrPosef poseDelta; GetPoseDelta(poseDelta, session, time))
            {
                if (positionPressed)
                {
                    if (!m_PositionActive)
                    {
                        ApplyPosition();
                        m_Tracker->LogOffsetValues();
                    }
                }
                else
                {
                    ApplyTranslation();
                    ApplyRotation(poseDelta);
                    ResetReferencePose(session, time);
                }
            }
            
        }
        m_PositionActive = positionPressed;
        m_MoveActive = !positionPressed && movePressed;
    }

    bool CorManipulator::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        return GetControllerPose(trackerPose, session, time);
    }

    void CorManipulator::GetButtonState(XrSession session, bool& moveButton, bool& positionButton)
    {
        auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (!layer)
        {
            m_Initialized = false;
            ErrorLog("%s: unable to cast layer to OpenXrLayer", __FUNCTION__);
            return;
        }
        if (!m_Tracker->m_XrSyncCalled)
        {
            // sync actions
            TraceLoggingWrite(g_traceProvider,
                              "GetTriggerState",
                              TLPArg(layer->m_ActionSet, "xrSyncActions"));
            constexpr XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr, 0, nullptr};
            if (const XrResult result = GetInstance()->xrSyncActions(session, &syncInfo); XR_FAILED(result))
            {
                ErrorLog("%s: xrSyncActions failed: %s", __FUNCTION__, xr::ToCString(result));
                return;
            }
        }
        {
            // obtain current action state
            TraceLoggingWrite(g_traceProvider,
                              "GetTriggerState",
                              TLPArg(layer->m_MoveAction, "xrGetActionStateBoolean"));
            XrActionStateGetInfo moveInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            moveInfo.action = layer->m_MoveAction;

            XrActionStateBoolean moveButtonState{XR_TYPE_ACTION_STATE_BOOLEAN};
            if (const XrResult result = layer->xrGetActionStateBoolean(session, &moveInfo, &moveButtonState); XR_FAILED(result))
            {
                ErrorLog("%s: xrGetActionStateBoolean failed: %s", __FUNCTION__, xr::ToCString(result));
                return;
            }
            if (moveButtonState.isActive == XR_TRUE)
            {
                moveButton = XR_TRUE == moveButtonState.currentState;
                TraceLoggingWrite(g_traceProvider, "GetButtonState", TLArg(moveButton, "MoveState"));
            }

            TraceLoggingWrite(g_traceProvider,
                              "GetTriggerState",
                              TLPArg(layer->m_PositionAction, "xrGetActionStateBoolean"));
            XrActionStateGetInfo positionInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            positionInfo.action = layer->m_PositionAction;

            XrActionStateBoolean positionButtonState{XR_TYPE_ACTION_STATE_BOOLEAN};
            if (const XrResult result = layer->xrGetActionStateBoolean(session, &positionInfo, &positionButtonState);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrGetActionStateBoolean failed: %s", __FUNCTION__, xr::ToCString(result));
                return;
            }
            if (positionButtonState.isActive == XR_TRUE)
            {
                positionButton = XR_TRUE == positionButtonState.currentState;
                TraceLoggingWrite(g_traceProvider, "GetButtonState", TLArg(positionButton, "PositionState"));
            }
        }
    }

    void CorManipulator::ApplyPosition() const
    {
        const XrPosef corPose = m_Tracker->GetReferencePose();
        TraceLoggingWrite(g_traceProvider,
                          "ApplyPosition",
                          TLArg(xr::ToString(corPose).c_str(), "Before"),
                          TLArg(xr::ToString(this->m_LastPose).c_str(), "Tracker"));

        const auto [relativeOrientation, relativePosition] = Pose::Multiply(m_LastPose, Pose::Invert(corPose));
        m_Tracker->ChangeOffset(relativePosition);
        const float angleDelta = GetYawAngle(GetForwardVector(m_LastPose.orientation, false)) -
                           GetYawAngle(GetForwardVector(corPose.orientation, true));
        m_Tracker->ChangeRotation(angleDelta);

        TraceLoggingWrite(g_traceProvider,
                          "ApplyPosition",
                          TLArg(xr::ToString(this->m_Tracker->GetReferencePose()).c_str(), "After"));
    }

    void CorManipulator::ApplyTranslation() const
    {
        const XrPosef corPose = m_Tracker->GetReferencePose();
        TraceLoggingWrite(g_traceProvider,
                          "ApplyTranslation",
                          TLArg(xr::ToString(corPose).c_str(), "Before"),
                          TLArg(xr::ToString(this->m_ReferencePose).c_str(), "Reference"),
                          TLArg(xr::ToString(this->m_LastPose).c_str(), "Tracker"));

        const DirectX::XMVECTOR rot = LoadXrQuaternion(corPose.orientation);
        const DirectX::XMVECTOR ref = LoadXrVector3(m_ReferencePose.position);
        const DirectX::XMVECTOR current = LoadXrVector3(m_LastPose.position);
        XrVector3f relativeTranslation;
        StoreXrVector3(
            &relativeTranslation,
            DirectX::XMVector3InverseRotate(DirectX::XMVectorAdd(current, DirectX::XMVectorNegate(ref)), rot));
        m_Tracker->ChangeOffset(relativeTranslation);

        TraceLoggingWrite(g_traceProvider,
                          "ApplyTranslation",
                          TLArg(xr::ToString(m_Tracker->GetReferencePose()).c_str(), "After"));
    }

    void CorManipulator::ApplyRotation(const XrPosef& poseDelta) const
    {
        TraceLoggingWrite(g_traceProvider,
                          "ApplyRotation",
                          TLArg(xr::ToString(m_Tracker->GetReferencePose()).c_str(), "Before"),
                          TLArg(xr::ToString(poseDelta).c_str(), "Delta"));

        const float yawAngle = -GetYawAngle(GetForwardVector(poseDelta.orientation, true));
        m_Tracker->ChangeRotation(yawAngle);
       
        TraceLoggingWrite(g_traceProvider,
                          "ApplyRotation",
                          TLArg(xr::ToString(m_Tracker->GetReferencePose()).c_str(), "After"),
                          TLArg(yawAngle, "Angle Delta"));
    }

    bool ViveTrackerInfo::Init()
    {
        std::string trackerType;
        if (!GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            return false;
        }
        if ("vive" == trackerType)
        {
            if (!GetInstance()->IsExtensionGranted(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME))
            {
                ErrorLog("%s: runtime does not support Vive tracker OpenXR extension: %s",
                         __FUNCTION__,
                         XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);
                return false;
            }
            std::string side;
            if (!GetConfig()->GetString(Cfg::TrackerSide, side))
            {
                return false;
            }
            if (!validRoles.contains(side))
            {
                ErrorLog("invalid role for vive tracker (= parameter 'side'): %s", side.c_str());
                std::string validOptions;
                for (const auto& option : validRoles)
                {
                    validOptions += (validOptions.empty() ? "" : ", ") + option;
                }
                Log("try one of the following instead: %s", validOptions.c_str());
                return false;
            }
            role = "/user/vive_tracker_htcx/role/" + side;
            Log("vive tracker is using role %s", role.c_str());
            active = true;
        }
        return true;
    }

    void GetTracker(TrackerBase** tracker)
    {
        const TrackerBase* previousTracker = *tracker;
        std::string trackerType;
        if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            if ("yaw" == trackerType)
            {
                Log("using Yaw Game Engine memory mapped file as tracker");

                delete previousTracker;

                *tracker = new YawTracker();
                return;
            }
            if ("srs" == trackerType)
            {
                Log("using SRS memory mapped file as tracker");

                delete previousTracker;

                *tracker = new SrsTracker();
                return;
            }
            if ("flypt" == trackerType)
            {
                Log("using FlyPT Mover memory mapped file as tracker");

                delete previousTracker;

                *tracker = new FlyPtTracker();
                return;
            }
            if ("controller" == trackerType)
            {
                Log("using motion controller as tracker");

                delete previousTracker;

                *tracker = new OpenXrTracker();
                return;
            }
            if ("vive" == trackerType)
            {
                Log("using vive tracker as tracker");

                delete previousTracker;

                *tracker = new OpenXrTracker();
                return;
            }
            else
            {
                ErrorLog("unknown tracker type: %s", trackerType.c_str());
            }
        }
        else
        {
            ErrorLog("unable to determine tracker type");
        }
        if (previousTracker)
        {
            ErrorLog("retaining previous tracker type");
            return;
        }
        ErrorLog("defaulting to 'controller'");
        *tracker = new OpenXrTracker();
    }
} // namespace Tracker