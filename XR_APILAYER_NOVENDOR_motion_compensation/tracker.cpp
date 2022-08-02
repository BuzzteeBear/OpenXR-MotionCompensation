// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "layer.h"
#include <log.h>
#include <util.h>

using namespace motion_compensation_layer;
using namespace motion_compensation_layer::log;
using namespace xr::math;

namespace Tracker
{
    TrackerBase::~TrackerBase()
    {
        if (m_TransFilter)
        {
            delete m_TransFilter;
        }
        if (m_RotFilter)
        {
            delete m_RotFilter;
        }
    }

    bool TrackerBase::Init()
    {
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
            ErrorLog("%s: error reading configured values for filters\n", __FUNCTION__);
        }
        if (1 > orderTrans || 3 < orderTrans)
        {
            ErrorLog("%s: invalid order for translational filter: %d\n", __FUNCTION__, orderTrans);
            return false;
        }
        if (1 > orderRot || 3 < orderRot)
        {
            ErrorLog("%s: invalid order for rotational filter: %d\n", __FUNCTION__, orderRot);
            return false;
        }
        // remove previous filter objects
        if (m_TransFilter)
        {
            delete m_TransFilter;
        }
        if (m_RotFilter)
        {
            delete m_RotFilter;
        }

        m_TransStrength = strengthTrans;
        m_RotStrength = strengthRot;

        Log("translational filter stages: %d\n", orderTrans);
        Log("translational filter strength: %f\n", m_TransStrength);
        m_TransFilter = 1 == orderTrans   ? new Filter::SingleEmaFilter(m_TransStrength)
                        : 2 == orderTrans ? new Filter::DoubleEmaFilter(m_TransStrength)
                                          : new Filter::TripleEmaFilter(m_TransStrength);

        Log("rotational filter stages: %d\n", orderRot);
        Log("rotational filter strength: %f\n", m_TransStrength);
        m_RotFilter = 1 == orderRot   ? new Filter::SingleSlerpFilter(m_RotStrength)
                      : 2 == orderRot ? new Filter::DoubleSlerpFilter(m_RotStrength)
                                      : new Filter::TripleSlerpFilter(m_RotStrength);

        return true;
    }

    void TrackerBase::ModifyFilterStrength(bool trans, bool increase)
    {
        float* currentValue = trans ? &m_TransStrength : &m_RotStrength;
        float prevValue = *currentValue;
        float amount = (1.1f - *currentValue) * 0.05f;
        float newValue = *currentValue + (increase ? amount : -amount);
        if (trans)
        {
            *currentValue = m_TransFilter->SetStrength(newValue);
            GetConfig()->SetValue(Cfg::TransStrength, *currentValue);
            Log("translational filter strength %screased to %f\n", increase ? "in" : "de", *currentValue);
        }
        else
        {
            *currentValue = m_RotFilter->SetStrength(newValue);
            GetConfig()->SetValue(Cfg::RotStrength, *currentValue);
            Log("rotational filter strength %screased to %f\n", increase ? "in" : "de", *currentValue);
        }
        MessageBeep(*currentValue != prevValue ? MB_OK : MB_ICONERROR);
    }

    void TrackerBase::SetReferencePose(XrPosef pose)
    {
        m_TransFilter->Reset(pose.position);
        m_RotFilter->Reset(pose.orientation);
        m_ReferencePose = pose;
        m_Calibrated = true;
    }

    bool TrackerBase::GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time)
    {
        // pose already calulated for requested time or unable to calculate
        if (time == m_LastPoseTime)
        {
            // already calulated for requested time;
            poseDelta = m_LastPoseDelta;
            TraceLoggingWrite(g_traceProvider,
                              "GetPoseDelta",
                              TLArg(xr::ToString(m_LastPoseDelta).c_str(), "Last_Delta"));
            return true;
        }
        if (m_ResetReferencePose)
        {
            m_ResetReferencePose = !ResetReferencePose(session, time);
        }
        XrPosef curPose{Pose::Identity()};
        if (GetPose(curPose, session, time))
        {
            // apply translational filter
            m_TransFilter->Filter(curPose.position);

            // apply rotational filter
            m_RotFilter->Filter(curPose.orientation);

            TraceLoggingWrite(g_traceProvider,
                              "GetPoseDelta",
                              TLArg(xr::ToString(curPose).c_str(), "Location_After_Filter"),
                              TLArg(time, "Time"));

            // calculate difference toward reference pose
            poseDelta = Pose::Multiply(Pose::Invert(curPose), m_ReferencePose);

            TraceLoggingWrite(g_traceProvider, "GetPoseDelta", TLArg(xr::ToString(poseDelta).c_str(), "Delta"));

            m_LastPoseTime = time;
            m_LastPoseDelta = poseDelta;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool TrackerBase::GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            // Query the latest tracker pose.
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
            {
                // TODO: skip xrSyncActions if other actions are active as well?
                XrActiveActionSet activeActionSets;
                activeActionSets.actionSet = layer->m_ActionSet;
                activeActionSets.subactionPath = XR_NULL_PATH;

                XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr};
                syncInfo.activeActionSets = &activeActionSets;
                syncInfo.countActiveActionSets = 1;

                TraceLoggingWrite(g_traceProvider,
                                  "GetPose",
                                  TLPArg(layer->m_ActionSet, "xrSyncActions"),
                                  TLArg(time, "Time"));
                CHECK_XRCMD(GetInstance()->xrSyncActions(session, &syncInfo));
            }
            {
                XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
                XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
                getActionStateInfo.action = layer->m_TrackerPoseAction;

                TraceLoggingWrite(g_traceProvider,
                                  "GetPose",
                                  TLPArg(layer->m_TrackerPoseAction, "xrGetActionStatePose"),
                                  TLArg(time, "Time"));
                CHECK_XRCMD(GetInstance()->xrGetActionStatePose(session, &getActionStateInfo, &actionStatePose));

                if (!actionStatePose.isActive)
                {
                    ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active\n", __FUNCTION__);
                    return false;
                }
            }

            CHECK_XRCMD(GetInstance()->xrLocateSpace(layer->m_TrackerSpace, layer->m_ReferenceSpace, time, &location));

            if (!Pose::IsPoseValid(location.locationFlags))
            {
                ErrorLog("%s: unable to determine tracker pose - XrSpaceLocation not valid\n", __FUNCTION__);
                return false;
            }
            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLArg(xr::ToString(location.pose).c_str(), "Location"),
                              TLArg(time, "Time"));

            trackerPose = location.pose;
            return true;
        }
        else
        {
            ErrorLog("unable to cast instance to OpenXrLayer\n");
            return false;
        }
    }

    bool OpenXrTracker::ResetReferencePose(XrSession session, XrTime time)
    {
        XrPosef curPose;
        if (GetPose(curPose, session, time))
        {
            SetReferencePose(curPose);
            return true;
        }
        else
        {
            ErrorLog("%s: unable to get current pose\n", __FUNCTION__);
            m_Calibrated = false;
            return false;
        }
    }

    bool OpenXrTracker::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        return GetControllerPose(trackerPose, session, time);
    }

    bool VirtualTracker::Init()
    {
        bool success{true};
        float value;
        if (GetConfig()->GetFloat(Cfg::TrackerOffsetForward, value))
        {
            m_OffsetForward = value / 100.0f;
            Log("offset forward = %f cm\n", value);
        }
        else
        {
            success = false;
        }
        if (GetConfig()->GetFloat(Cfg::TrackerOffsetDown, value))
        {
            m_OffsetDown = value / 100.0f;
            Log("offset down = %f cm\n", value);
        }
        else
        {
            success = false;
        }
        if (GetConfig()->GetFloat(Cfg::TrackerOffsetRight, value))
        {
            m_OffsetRight = value / 100.0f;
            Log("offset right = %f cm\n", value);
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

    bool VirtualTracker::ResetReferencePose(XrSession session, XrTime time)
    {
        // TODO: determine reference pose using saved pose in stage
        bool success = true;
        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
            if (XR_SUCCEEDED(
                    layer->OpenXrApi::xrLocateSpace(layer->m_ViewSpace, layer->m_ReferenceSpace, time, &location)) &&
                Pose::IsPoseValid(location.locationFlags))
            {
                // project forward and right vector of view onto 'floor plane'
                XrVector3f forward;
                StoreXrVector3(&forward,
                               DirectX::XMVector3Rotate(LoadXrVector3(XrVector3f{0.0f, 0.0f, -1.0f}),
                                                        LoadXrQuaternion(location.pose.orientation)));
                forward.y = 0;
                forward = Normalize(forward);
                XrVector3f right{-forward.z, 0.0f, forward.x};

                // calculate and apply translational offset
                XrVector3f offset =
                    m_OffsetForward * forward + m_OffsetRight * right + XrVector3f{0.0f, -m_OffsetDown, 0.0f};
                location.pose.position = location.pose.position + offset;

                // calculate orientation parallel to floor
                float yawAngle = atan2f(forward.x, forward.z);
                StoreXrQuaternion(&location.pose.orientation,
                                  DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yawAngle, 0.0f));

                XrPosef refPose = location.pose;

                if (m_DebugMode)
                {
                    // manipulate ref pose orientation to match motion controller
                    m_OriginalRefPose = m_ReferencePose;
                    XrPosef controllerPose;
                    if (GetControllerPose(controllerPose, session, time))
                    {
                        refPose.orientation = controllerPose.orientation;
                    }
                }
                TrackerBase::SetReferencePose(refPose);
            }
            else
            {
                ErrorLog("%s: xrLocateSpace(view) failed\n", __FUNCTION__);
                success = false;
            }
        }
        else
        {
            ErrorLog("%s: cast of layer failed\n", __FUNCTION__);
            success = false;
        }

        m_Calibrated = success;
        return success;
    }

    bool VirtualTracker::ChangeOffset(XrVector3f modification)
    {
        if (m_DebugMode)
        {
            ErrorLog("%s: unable to change offset while cor debug mode is active");
            return false;
        }

        m_OffsetForward += modification.z;
        GetConfig()->SetValue(Cfg::TrackerOffsetForward, m_OffsetForward * 100.0f);

        m_OffsetDown -= modification.y;
        GetConfig()->SetValue(Cfg::TrackerOffsetDown, m_OffsetDown * 100.0f);

        m_OffsetRight -= modification.x;
        GetConfig()->SetValue(Cfg::TrackerOffsetRight, m_OffsetRight * 100.0f);

        XrPosef adjustment{{Quaternion::Identity()}, modification};
        m_ReferencePose = Pose::Multiply(adjustment, m_ReferencePose);
        // SetReferencePose(Pose::Multiply(adjustment, m_ReferencePose));
        return true;
    }

    bool VirtualTracker::ChangeRotation(bool right)
    {
        if (m_DebugMode)
        {
            ErrorLog("%s: unable to change offset while cor debug mode is active");
            return false;
        }
        XrPosef adjustment{Pose::Identity()};
        StoreXrQuaternion(&adjustment.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(0.0f, (right ? -1.0f : 1.0f) * angleToRadian, 0.0f));

        SetReferencePose(Pose::Multiply(adjustment, m_ReferencePose));
        return true;
    }

    bool VirtualTracker::ToggleDebugMode(XrSession session, XrTime time)
    {
        bool success = true;
        if (!m_DebugMode)
        {
            if (!m_Calibrated)
            {
                success = ResetReferencePose(session, time);
            }
            if (success)
            {
                // manipulate ref pose orientation to match motion controller
                XrPosef controllerPose;
                if (GetControllerPose(controllerPose, session, time))
                {
                    m_OriginalRefPose = m_ReferencePose;
                    m_ReferencePose.orientation = controllerPose.orientation;
                    SetReferencePose(m_ReferencePose);
                    m_DebugMode = true;
                    Log("debug cor mode activated\n");
                }
                else
                {
                    ErrorLog("unable to activate cor debug mode\n");
                    success = false;
                }
            }
        }
        else
        {
            SetReferencePose(m_OriginalRefPose);
            m_DebugMode = false;
            Log("debug cor mode deactivated\n");
        }
        return success;
    }

    bool VirtualTracker::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        bool success{true};

        if (!m_DebugMode)
        {
            success = GetVirtualPose(trackerPose, session, time);
        }
        else
        {
            if (GetControllerPose(trackerPose, session, time))
            {
                // remove translation towards reference pose
                StoreXrVector3(&trackerPose.position,
                               DirectX::XMVector3Rotate(LoadXrVector3(m_ReferencePose.position),
                                                        LoadXrQuaternion(m_ReferencePose.orientation)));
            }
            else
            {
                success = false;
            }
        }
        return success;
    }

    void VirtualTracker::SetOffset(float forward, float down, float right)
    {
        m_OffsetForward = forward;
        m_OffsetDown = down;
        m_OffsetRight = right;
    }

    bool YawTracker::LazyInit(XrTime time)
    {
        bool success = true;
        if (!m_SkipLazyInit)
        {
            m_Mmf.SetName("Local\\YawVRGEFile");
            if (m_Mmf.Open())
            {
                YawData data;
                if (m_Mmf.Read(&data, sizeof(data)))
                {
                    // TODO: calculate difference between floor and headset and set offset according to file values
                }
                else
                {
                    ErrorLog("%s: unable to read from mmf 'YawVRGEFile'\n", __FUNCTION__);
                    success = false;
                }
            }
            else
            {
                ErrorLog("%s: unable to open mmf 'YawVRGEFile'. Check if Game Engine is running and motion "
                         "compensation is activated!\n",
                         __FUNCTION__);
                success = false;
            }
        }
        m_SkipLazyInit = success;
        return success;
    }

    bool YawTracker::GetVirtualPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        YawData data;
        XrPosef rotation{Pose::Identity()};
        if (!m_Mmf.Read(&data, sizeof(data)))
        {
            return false;
        }

        DebugLog("YawData:\n\tyaw: %f, pitch: %f, roll: %f\n\tbattery: %f, rotationHeight: %f, "
                 "rotationForwardHead: %f\n\tsixDof: %d, usePos: %d, autoX: %f, autoY: %f\n",
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

        StoreXrQuaternion(&rotation.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(-data.pitch * angleToRadian,
                                                                    -data.yaw * angleToRadian,
                                                                    data.roll * angleToRadian));

        trackerPose = Pose::Multiply(rotation, m_ReferencePose);
        return true;
    }

    bool SixDofTracker::LazyInit(XrTime time)
    {
        bool success = true;
        if (!m_SkipLazyInit)
        {
            m_Mmf.SetName(m_Filename);

            if (!m_Mmf.Open())
            {
                ErrorLog("unable to open mmf '%s'. Check if motion software is running and motion compensation is "
                         "activated!\n",
                         m_Filename.c_str());
                success = false;
            }
        }
        m_SkipLazyInit = success;
        return success;
    }

    bool SixDofTracker::GetVirtualPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        SixDofData data;
        XrPosef rigPose{Pose::Identity()};
        if (!m_Mmf.Read(&data, sizeof(data)))
        {
            return false;
        }

        DebugLog("MotionData:\n\tyaw: %f, pitch: %f, roll: %f\n\tsway: %f, surge: %f, heave: %f\n",
                 data.yaw,
                 data.pitch,
                 data.roll,
                 data.sway,
                 data.surge,
                 data.heave);

        // TODO: determine signum of angles
        StoreXrQuaternion(&rigPose.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(-(float)data.pitch * angleToRadian,
                                                                    -(float)data.yaw * angleToRadian,
                                                                    (float)data.roll * angleToRadian));
        rigPose.position =
            XrVector3f{(float)data.sway / 1000.0f, (float)data.heave / 1000.0f, (float)data.surge / -1000.0f};

        trackerPose = Pose::Multiply(rigPose, m_ReferencePose);
        return true;
    }

    void GetTracker(TrackerBase** tracker)
    {
        TrackerBase* previousTracker = *tracker;
        std::string trackerType;
        if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            if ("yaw" == trackerType)
            {
                Log("using Yaw Game Engine memory mapped file as tracker\n");
                if (previousTracker)
                {
                    delete previousTracker;
                }
                *tracker = new YawTracker();
                return;
            }
            if ("srs" == trackerType)
            {
                Log("using SRS memory mapped file as tracker\n");
                if (previousTracker)
                {
                    delete previousTracker;
                }
                *tracker = new SrsTracker();
                return;
            }
            if ("flypt" == trackerType)
            {
                Log("using FlyPT Mover memory mapped file as tracker\n");
                if (previousTracker)
                {
                    delete previousTracker;
                }
                *tracker = new FlyPtTracker();
                return;
            }
            else if ("controller" == trackerType)
            {
                Log("using motion cotroller as tracker\n");
                if (previousTracker)
                {
                    delete previousTracker;
                }
                *tracker = new OpenXrTracker();
                return;
            }
            else
            {
                ErrorLog("unknown tracker type: %s\n", trackerType.c_str());
            }
        }
        else
        {
            ErrorLog("unable to determine tracker type, defaulting to 'controller'\n");
        }
        if (previousTracker)
        {
            ErrorLog("retaining previous tracker type\n");
            return;
        }
        ErrorLog("defaulting to 'controller'\n");
        *tracker = new OpenXrTracker();
    }
} // namespace Tracker