// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "layer.h"
#include "output.h"
#include <log.h>
#include <util.h>

using namespace openxr_api_layer;
using namespace log;
using namespace output;
using namespace xr::math;
using namespace utility;

namespace tracker
{
    bool ControllerBase::Init()
    {
        GetConfig()->GetBool(Cfg::PhysicalEnabled, m_PhysicalEnabled);
        return true;
    }

    bool ControllerBase::GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "ControllerBase::GetPoseDelta", TLXArg(session, "Session"), TLArg(time, "Time"));

        // pose already calculated for requested time
        if (time == m_LastPoseTime)
        {
            // already calculated for requested time;
            poseDelta = m_LastPoseDelta;
            TraceLoggingWriteStop(local,
                                  "ControllerBase::GetPoseDelta",
                                  TLArg(xr::ToString(m_LastPoseDelta).c_str(), "LastDelta"));
            DebugLog("delta(%lld) reused", time);
            return true;
        }
        if (XrPosef curPose{Pose::Identity()}; GetPose(curPose, session, time))
        {
            m_Recorder->AddFrameTime(time);
            m_Recorder->AddPose(m_ReferencePose, Reference);
            m_Recorder->AddPose(curPose, Unfiltered);

            ApplyFilters(curPose);
            m_Recorder->AddPose(curPose, Filtered);

            ApplyModifier(curPose);
            m_Recorder->AddPose(curPose, Modified);

            // calculate difference toward reference pose
            poseDelta = Pose::Multiply(Pose::Invert(curPose), m_ReferencePose);
            m_Recorder->Write();

            if (!m_FallBackUsed)
            {
                m_LastPose = curPose;
                m_LastPoseDelta = poseDelta;
                m_LastPoseTime = time;
            }

            DebugLog("delta(%lld): %s", time, xr::ToString(poseDelta).c_str());
            TraceLoggingWriteStop(local,
                                  "ControllerBase::GetPoseDelta",
                                  TLArg(true, "Success"),
                                  TLArg(xr::ToString(poseDelta).c_str(), "Delta"));
            return true;
        }
        TraceLoggingWriteStop(local, "ControllerBase::GetPoseDelta", TLArg(false, "Success"));

        return false;
    }

    XrPosef ControllerBase::GetLastPoseDelta() const
    {
        return m_LastPoseDelta;
    }

    bool ControllerBase::ResetReferencePose(XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "ControllerBase::ResetReferencePose",
                               TLXArg(session, "Session"),
                               TLArg(time, "Time"));

        if (XrPosef curPose; GetPose(curPose, session, time))
        {
            SetReferencePose(curPose, false);
            TraceLoggingWriteStop(local, "ControllerBase::ResetReferencePose", TLArg(true, "Success"));
            return true;
        }
        ErrorLog("%s: unable to get current pose", __FUNCTION__);
        TraceLoggingWriteStop(local, "ControllerBase::ResetReferencePose", TLArg(false, "Success"));
        return false;
    }

    void ControllerBase::SetReferencePose(const XrPosef& pose, const bool silent)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "ControllerBase::SetReferencePose", TLArg(xr::ToString(pose).c_str(), "Pose"));

        m_ReferencePose = pose;
        if (!silent)
        {
            Log("tracker reference pose set");
        }

        TraceLoggingWriteStop(local, "ControllerBase::SetReferencePose");
    }

    bool ControllerBase::GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "ControllerBase::GetControllerPose",
                               TLXArg(session, "Session"),
                               TLArg(time, "Time"));

        if (!m_PhysicalEnabled)
        {
            ErrorLog("%s: physical tracker disabled in config file", __FUNCTION__);
            TraceLoggingWriteStop(local, "ControllerBase::GetControllerPose", TLArg(false, "Success"));
            return false;
        }

        if (auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance()))
        {
            // Query the latest tracker pose.
            if (!layer->SyncActions("GetControllerPose"))
            {
                TraceLoggingWriteStop(local, "ControllerBase::GetControllerPose", TLArg(false, "Success"));
                return false;
            }
            {
                XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
                XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
                getActionStateInfo.action = layer->m_PoseAction;

                TraceLoggingWriteTagged(local,
                                        "ControllerBase::GetControllerPose",
                                        TLXArg(layer->m_PoseAction, "xrGetActionStatePose"));
                if (const XrResult result =
                        GetInstance()->xrGetActionStatePose(session, &getActionStateInfo, &actionStatePose);
                    XR_FAILED(result))
                {
                    ErrorLog("%s: xrGetActionStatePose failed: %s", __FUNCTION__, xr::ToCString(result));
                    TraceLoggingWriteStop(local, "ControllerBase::GetControllerPose", TLArg(false, "Success"));
                    return false;
                }

                if (!actionStatePose.isActive)
                {
                    if (!m_ConnectionLost)
                    {
                        ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active", __FUNCTION__);
                        m_ConnectionLost = true;
                    }
                    TraceLoggingWriteStop(local, "ControllerBase::GetControllerPose", TLArg(false, "Success"));
                    return false;
                }
            }
            TraceLoggingWriteTagged(local,
                                    "ControllerBase::GetControllerPose",
                                    TLXArg(layer->m_ActionSet, "xrLocateSpace"));
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
            if (const XrResult result = GetInstance()->OpenXrApi::xrLocateSpace(layer->m_TrackerSpace,
                                                                                layer->m_StageSpace,
                                                                                time,
                                                                                &location);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrLocateSpace failed: %s", __FUNCTION__, xr::ToCString(result));
                if (XR_ERROR_TIME_INVALID == result && m_LastPoseTime != 0)
                {
                    TraceLoggingWriteTagged(local,
                                            "ControllerBase::GetControllerPose",
                                            TLArg(time, "RequestedTime"),
                                            TLArg(m_LastPoseTime, "LastGoodTime"));
                    if (!m_FallBackUsed)
                    {
                        Log("Warning: requested time (%lld) is out of bounds, using last known tracker pose (%lld)",
                            time,
                            m_LastPoseTime);
                        m_FallBackUsed = true;
                    }
                    trackerPose = m_LastPose;
                    TraceLoggingWriteStop(local,
                                          "ControllerBase::GetControllerPose",
                                          TLArg(true, "Success"),
                                          TLArg(true, "Fallback"));
                    return true;
                }
                TraceLoggingWriteStop(local, "ControllerBase::GetControllerPose", TLArg(false, "Success"));
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
            TraceLoggingWriteStop(local,
                                  "ControllerBase::GetControllerPose",
                                  TLArg(true, "Success"),
                                  TLArg(xr::ToString(location.pose).c_str(), "TrackerPose"));
            m_ConnectionLost = false;
            m_FallBackUsed = false;
            trackerPose = location.pose;
            return true;
        }
        ErrorLog("%s: unable to cast instance to OpenXrLayer", __FUNCTION__);
        TraceLoggingWriteStop(local, "ControllerBase::GetControllerPose", TLArg(false, "Success"));
        return false;
    }

    XrVector3f ControllerBase::GetForwardVector(const XrQuaternionf& quaternion)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "ControllerBase::GetForwardVector",
                               TLArg(xr::ToString(quaternion).c_str(), "Quaternion"));
        XrVector3f forward;
        StoreXrVector3(&forward,
                       DirectX::XMVector3Rotate(LoadXrVector3(XrVector3f{0, 0, 1}), LoadXrQuaternion(quaternion)));
        forward.y = 0;
        forward = Normalize(forward);
        TraceLoggingWriteStop(local,
                              "ControllerBase::GetForwardVector",
                              TLArg(xr::ToString(forward).c_str(), "Forward"));
        return forward;
    }

    XrQuaternionf ControllerBase::GetLeveledRotation(const XrVector3f& forward, float yawAdjustment)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "ControllerBase::GetLeveledRotation",
                               TLArg(xr::ToString(forward).c_str(), "Forward"),
                               TLArg(yawAdjustment, "YawAdjustment"));

        XrQuaternionf rotation{};
        StoreXrQuaternion(&rotation,
                          DirectX::XMQuaternionNormalize(
                              DirectX::XMQuaternionRotationRollPitchYaw(0, GetYawAngle(forward) + yawAdjustment, 0)));

        TraceLoggingWriteStop(local,
                              "ControllerBase::GetLeveledRotation",
                              TLArg(xr::ToString(rotation).c_str(), "Rotation"));
        return rotation;
    }

    float ControllerBase::GetYawAngle(const XrVector3f& forward)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "ControllerBase::GetYawAngle", TLArg(xr::ToString(forward).c_str(), "Forward"));

        const float angle = atan2f(forward.x, forward.z);

        TraceLoggingWriteStop(local, "ControllerBase::GetYawAngle", TLArg(angle, "Angle"));

        return angle;
    }

    TrackerBase::TrackerBase(const std::vector<DofValue>& relevant) : m_RelevantValues(relevant)
    {
        m_Recorder = std::make_shared<output::PoseAndDofRecorder>();
        bool samplerEnabled;
        if (GetConfig()->GetBool(Cfg::StabilizerEnabled, samplerEnabled) && samplerEnabled)
        {
            m_Sampler = new sampler::Sampler(this, m_RelevantValues, m_Recorder);
            m_Recorder->m_Sampling.store(true);
            Log("input stabilizer enabled");
        }
    }

    TrackerBase::~TrackerBase()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::~TrackerBase");

        delete m_TransFilter;
        delete m_RotFilter;
        delete m_Sampler;

        TraceLoggingWriteStop(local, "TrackerBase::~TrackerBase");
    }

    bool TrackerBase::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::Init");

        bool success = GetConfig()->GetBool(Cfg::LoadRefPoseFromFile, m_LoadPoseFromFile);
        Log("reference pose is %s read from config file", m_LoadPoseFromFile ? "" : "not");

        m_TrackerModifier = std::make_unique<modifier::TrackerModifier>();
        ControllerBase::Init();

        success = !success ? success : LoadFilters();

        TraceLoggingWriteStop(local,
                              "TrackerBase::Init",
                              TLArg(success, "Success"),
                              TLArg(m_LoadPoseFromFile, "m_LoadPoseFromFile"));
        return success;
    }

    bool TrackerBase::LazyInit(XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::LazyInit");

        m_SkipLazyInit = true;

        TraceLoggingWriteStop(local, "TrackerBase::LazyInit", TLArg(true, "Success"));

        return true;
    }

    bool TrackerBase::LoadFilters()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::LoadFilters");

        // set up filters
        int orderTrans = 2, orderRot = 2;
        float strengthTrans = 0.0f, strengthRot = 0.0f;
        if (!GetConfig()->GetInt(Cfg::TransOrder, orderTrans) || !GetConfig()->GetInt(Cfg::RotOrder, orderRot) ||
            !GetConfig()->GetFloat(Cfg::TransStrength, strengthTrans) ||
            !GetConfig()->GetFloat(Cfg::RotStrength, strengthRot))
        {
            ErrorLog("%s: unable to read configured values for filters", __FUNCTION__);
        }
        if (1 > orderTrans || 3 < orderTrans)
        {
            ErrorLog("%s: invalid order for translational filter: %d", __FUNCTION__, orderTrans);
            TraceLoggingWriteStop(local, "TrackerBase::LoadFilters", TLArg(false, "Success"));
            return false;
        }
        if (1 > orderRot || 3 < orderRot)
        {
            ErrorLog("%s: invalid order for rotational filter: %d", __FUNCTION__, orderRot);
            TraceLoggingWriteStop(local, "TrackerBase::LoadFilters", TLArg(false, "Success"));
            return false;
        }
        // remove previous filter objects

        delete m_TransFilter;
        delete m_RotFilter;

        m_TransStrength = strengthTrans;
        m_RotStrength = strengthRot;

        Log("translational filter stages: %d", orderTrans);
        Log("translational filter strength: %f", m_TransStrength);
        m_TransFilter = 1 == orderTrans   ? new filter::SingleEmaFilter(m_TransStrength)
                        : 2 == orderTrans ? new filter::DoubleEmaFilter(m_TransStrength)
                                          : new filter::TripleEmaFilter(m_TransStrength);

        Log("rotational filter stages: %d", orderRot);
        Log("rotational filter strength: %f", m_RotStrength);
        m_RotFilter = 1 == orderRot   ? new filter::SingleSlerpFilter(m_RotStrength)
                      : 2 == orderRot ? new filter::DoubleSlerpFilter(m_RotStrength)
                                      : new filter::TripleSlerpFilter(m_RotStrength);

        TraceLoggingWriteStop(local,
                              "TrackerBase::LoadFilters",
                              TLArg(orderTrans, "OrderTrans"),
                              TLArg(m_TransStrength, "TransStrength"),
                              TLArg(orderRot, "OrderRot"),
                              TLArg(m_RotStrength, "RotStrength"));

        return true;
    }

    void TrackerBase::ModifyFilterStrength(const bool trans, const bool increase, const bool fast)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerBase::ModifyFilterStrength",
                               TLArg(trans, "Translation"),
                               TLArg(increase, "Increase"),
                               TLArg(fast, "Fast"));

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
        EventSink::Execute(*currentValue == prevValue ? increase ? Event::Max : Event::Min
                          : increase                 ? Event::Plus
                                                     : Event::Minus);

        TraceLoggingWriteStop(local,
                              "TrackerBase::ModifyFilterStrength",
                              TLArg(prevValue, "Previous"),
                              TLArg(newValue, "New"),
                              TLArg(amount, "Amount"));
    }

    
    void TrackerBase::ToggleStabilizer()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::ToggleStabilizer");

        if (!m_Sampler)
        {
            m_Sampler = new sampler::Sampler(this, m_RelevantValues, m_Recorder);
            m_Recorder->m_Sampling.store(true);
            if (m_Calibrated)
            {
                m_Sampler->StartSampling();
            }
            GetConfig()->SetValue(Cfg::StabilizerEnabled, true);
            EventSink::Execute(Event::StabilizerOn);

            Log("stabilizer activated");
            TraceLoggingWriteStop(local, "TrackerBase::ToggleStabilizer", TLArg(true, "Activated"));
            return;
        }
        delete m_Sampler;
        m_Sampler = nullptr;
        m_Recorder->m_Sampling.store(false);
        GetConfig()->SetValue(Cfg::StabilizerEnabled, false);
        EventSink::Execute(Event::StabilizerOff);

        Log("stabilizer off");
        TraceLoggingWriteStop(local, "TrackerBase::ToggleStabilizer", TLArg(false, "Activated"));
    }

    void TrackerBase::ModifyStabilizer(bool increase, bool fast)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerBase::ModifyStabilizer",
                               TLArg(increase, "Increase"),
                               TLArg(fast, "Fast"));
        float prevStrength;
        if (!GetConfig()->GetFloat(Cfg::StabilizerStrength, prevStrength))
        {
            EventSink::Execute(Event::Error);
            TraceLoggingWriteStop(local, "TrackerBase::ModifyStabilizer", TLArg(false, "Success"));
            return;
        }
        float amount = (increase ? 1 : -1) * (fast ? 0.10f : 0.01f);

        float newStrength = prevStrength + amount;
        if (newStrength < 0.001f)
        {
            newStrength = 0;

            EventSink::Execute(Event::Min);
            TraceLoggingWriteTagged(local, "TrackerBase::ModifyStabilizer", TLArg(true, "Minimum"));
        }
        else if (newStrength > 0.999f)
        {
            newStrength = 1;

            EventSink::Execute(Event::Max);
            TraceLoggingWriteTagged(local, "TrackerBase::ModifyStabilizer", TLArg(true, "Maximum"));
        }
        else
        {
            EventSink::Execute(increase ? Event::Plus : Event::Minus);
        }

        GetConfig()->SetValue(Cfg::StabilizerStrength, newStrength);
        Log("stabilizer strength: %.4f", newStrength);

        if (m_Sampler)
        {
            m_Sampler->SetStrength(newStrength);
        }

        TraceLoggingWriteStop(local,
                              "TrackerBase::ModifyStabilizer",
                              TLArg(prevStrength, "Previous"),
                              TLArg(amount, "Amount"),
                              TLArg(newStrength, "New"),
                              TLArg(true, "Success"));
    }

    XrPosef TrackerBase::GetReferencePose() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::GetReferencePose");
        TraceLoggingWriteStop(local,
                              "TrackerBase::GetReferencePose",
                              TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));

        return m_ReferencePose;
    }

    bool TrackerBase::ChangeOffset(XrVector3f modification)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerBase::ChangeOffset",
                               TLArg(xr::ToString(modification).c_str(), "Modification"),
                               TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        if (!m_Calibrated)
        {
            ErrorLog("%s: please calibrate cor position before trying to move it", __FUNCTION__);
            TraceLoggingWriteStop(local, "TrackerBase::ChangeOffset", TLArg(false, "Success"));
            return false;
        }

        const XrPosef adjustment{{Quaternion::Identity()}, modification};
        SetReferencePose(Pose::Multiply(adjustment, m_ReferencePose), true);
        TraceLoggingWriteStop(local,
                              "TrackerBase::ChangeOffset",
                              TLArg(true, "Success"),
                              TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        return true;
    }

    bool TrackerBase::ChangeRotation(float radian)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerBase::ChangeRotation",
                               TLArg(radian, "Radian"),
                               TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        if (!m_Calibrated)
        {
            ErrorLog("%s: please calibrate cor position before trying to rotate it", __FUNCTION__);
            TraceLoggingWriteStop(local, "VirtualTracker::ChangeRotation", TLArg(false, "Success"));
            return false;
        }

        XrPosef adjustment{Pose::Identity()};
        StoreXrQuaternion(&adjustment.orientation, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, radian, 0.0f));
        SetReferencePose(Pose::Multiply(adjustment, m_ReferencePose), true);

        TraceLoggingWriteStop(local,
                              "TrackerBase::ChangeRotation",
                              TLArg(true, "Success"),
                              TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        return true;
    }

    void TrackerBase::SetModifierActive(const bool active) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::SetModifierActive", TLArg(active, "Active"));

        m_TrackerModifier->SetActive(active);

        TraceLoggingWriteStop(local, "TrackerBase::SetModifierActive");
    }

    void TrackerBase::LogCurrentTrackerPoses(XrSession session, XrTime time, bool activated)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerBase::LogCurrentTrackerPoses",
                               TLXArg(session, "Session"),
                               TLArg(time, "Time"),
                               TLArg(activated, "Activated"));

        if (!m_Calibrated)
        {
            ErrorLog("%s: tracker not calibrated", __FUNCTION__);
            TraceLoggingWriteStop(local, "TrackerBase::LogCurrentTrackerPose", TLArg(false, "Success"));
            return;
        }
        Log("current reference pose in stage space: %s", xr::ToString(m_ReferencePose).c_str());
        XrPosef currentPose{Pose::Identity()};
        if (GetPose(currentPose, session, time))
        {
            Log("current tracker pose in stage space: %s", xr::ToString(currentPose).c_str());
        }

        TraceLoggingWriteStop(local, "TrackerBase::LogCurrentTrackerPose");
    }

    void TrackerBase::SetReferencePose(const XrPosef& pose, const bool silent)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::SetReferencePose", TLArg(xr::ToString(pose).c_str(), "Pose"));

        m_TransFilter->Reset(pose.position);
        m_RotFilter->Reset(pose.orientation);
        m_Calibrated = true;
        ControllerBase::SetReferencePose(pose, silent);
        if (m_Sampler)
        {
            m_Sampler->StartSampling();
        }

        TraceLoggingWriteStop(local, "TrackerBase::SetReferencePose");
    }

    void TrackerBase::InvalidateCalibration(bool silent)
    {
        m_Calibrated = false;
        EventSink::Execute(Event::CalibrationLost, silent);
        if (m_Sampler)
        {
            m_Sampler->StopSampling();
        }
        auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (!layer)
        {
        
            ErrorLog("%s: cast of layer failed", __FUNCTION__);
            return;
        }
        layer->ResetCalibratedHmdPose();
    }

    void TrackerBase::SaveReferencePose() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::SaveReferencePose");
        if (m_Calibrated)
        {
            SaveReferencePoseImpl(m_ReferencePose);
        }
        TraceLoggingWriteStop(local, "TrackerBase::SaveReferencePose", TLArg(m_Calibrated, "Success"));
    }

    void TrackerBase::SaveReferencePoseImpl(const XrPosef& refPose) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::SaveReferencePose");
       
        GetConfig()->SetValue(Cfg::CorX, refPose.position.x);
        GetConfig()->SetValue(Cfg::CorY, refPose.position.y);
        GetConfig()->SetValue(Cfg::CorZ, refPose.position.z);
        GetConfig()->SetValue(Cfg::CorA, refPose.orientation.w);
        GetConfig()->SetValue(Cfg::CorB, refPose.orientation.x);
        GetConfig()->SetValue(Cfg::CorC, refPose.orientation.y);
        GetConfig()->SetValue(Cfg::CorD, refPose.orientation.z);
        
        TraceLoggingWriteStop(local, "TrackerBase::SaveReferencePose", TLArg(m_Calibrated, "Success"));
    }

    bool TrackerBase::ToggleRecording() const
    {
        return m_Recorder->Toggle(m_Calibrated);
    }

    void TrackerBase::ApplyFilters(XrPosef& pose)
    {
         TraceLocalActivity(local);
         TraceLoggingWriteStart(local, "TrackerBase::ApplyFilters", TLArg(xr::ToString(pose).c_str(), "Pose"));

         // apply translational filter
         m_TransFilter->Filter(pose.position);

         // apply rotational filter
         m_RotFilter->Filter(pose.orientation);

         TraceLoggingWriteStop(local, "TrackerBase::ApplyFilters", TLArg(xr::ToString(pose).c_str(), "NewPose"));
    }

    void TrackerBase::ApplyModifier(XrPosef& pose)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::ApplyModifier", TLArg(xr::ToString(pose).c_str(), "Pose"));

        m_TrackerModifier->Apply(pose, m_ReferencePose);

        TraceLoggingWriteStop(local, "TrackerBase::ApplyModifier", TLArg(xr::ToString(pose).c_str(), "NewPose"));
    }

    bool TrackerBase::LoadReferencePose()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerBase::LoadReferencePose");

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
            TraceLoggingWriteStop(local, "TrackerBase::LoadReferencePose", TLArg(false, "Success"));
            return false;
        }
        if (success)
        {
            Log("reference pose successfully loaded from config file");
            SetReferencePose(refPose, false);
        }
        TraceLoggingWriteStop(local, "TrackerBase::LoadReferencePose", TLArg(false, "Success"));
        return success;
    }

    std::optional<XrPosef> TrackerBase::GetForwardView(XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "TrackerBase::GetForwardView",
                               TLArg(time, "Time"));

        auto view = GetCurrentView(session, time);
        if (!view.has_value())
        {
            TraceLoggingWriteStop(local, "TrackerBase::GetForwardView", TLArg(false, "Success"));
            return {};
        }

        // calculate leveled orientation (parallel to floor)
        view.value().orientation = GetLeveledRotation(GetForwardVector(view.value().orientation), 0);

        TraceLoggingWriteStop(local,
                              "TrackerBase::GetForwardView",
                              TLArg(true, "Success"),
                              TLArg(xr::ToString(view.value()).c_str(), "View"));
        return view;
    }

    std::optional<XrPosef> TrackerBase::GetCurrentView(XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::GetCurrentView", TLArg(time, "Time"));
        OpenXrLayer* layer;
        if (layer = reinterpret_cast<OpenXrLayer*>(GetInstance()); !layer)
        {
            ErrorLog("%s: cast of layer failed", __FUNCTION__);
            TraceLoggingWriteStop(local, "TrackerBase::GetCurrentView", TLArg(false, "Success"));
            return {};
        }
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        if (const XrResult result =
                layer->OpenXrApi::xrLocateSpace(layer->m_ViewSpace, layer->m_StageSpace, time, &location);
            XR_FAILED(result))
        {
            ErrorLog("%s: xrLocateSpace(view) failed: %s", __FUNCTION__, xr::ToCString(result));
            TraceLoggingWriteStop(local, "TrackerBase::GetCurrentView", TLArg(false, "Success"));
            return {};
        }
        if (!Pose::IsPoseValid(location.locationFlags))
        {
            ErrorLog("%s: view space location invalid", __FUNCTION__);
            TraceLoggingWriteStop(local, "TrackerBase::GetCurrentView", TLArg(false, "Success"));
            return {};
        }
        TraceLoggingWriteStop(local,
                              "TrackerBase::GetCurrentView",
                              TLArg(true, "Success"),
                              TLArg(xr::ToString(location.pose).c_str(), "View"));
        return location.pose;
    }

    void TrackerBase::SetForwardRotation(const XrPosef& pose) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "TrackerBase::SetForwardRotation", TLArg(xr::ToString(pose).c_str(), "Pose"));

        // update forward rotation
        const XrPosef fwdRotation = {pose.orientation, {0, 0, 0}};
        m_TrackerModifier->SetFwdToStage(fwdRotation);
        m_Recorder->SetFwdToStage(fwdRotation);
        if (const auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance()))
        {
            layer->SetForwardRotation(fwdRotation);
        }
        else
        {
            ErrorLog("%s: cast of layer failed", __FUNCTION__);
        }
        TraceLoggingWriteStop(local, "TrackerBase::SetForwardRotation");
    }

    OpenXrTracker::OpenXrTracker()
        : TrackerBase({sway, surge, heave, yaw, roll, pitch})
    {
        m_Source = std::make_unique<PhysicalSource>(this);
    }

    bool OpenXrTracker::LazyInit(XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrTracker::LazyInit", TLArg(time, "Time"));
        if (m_SkipLazyInit)
        {
            TraceLoggingWriteStop(local, "OpenXrTracker::LazyInit", TLArg(true, "Skipped"));
            return true;
        }
        if (auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance()))
        {
            layer->LogCurrentInteractionProfileAndSource("LazyInit");
            m_SkipLazyInit = true;

            TraceLoggingWriteStop(local, "OpenXrTracker::LazyInit", TLArg(true, "Logged"));
            return true;
        }

        TraceLoggingWriteStop(local, "OpenXrTracker::LazyInit", TLArg(false, "Success"));
        ErrorLog("%s: cast of layer failed", __FUNCTION__);
        return false;
    }

    bool OpenXrTracker::ResetReferencePose(XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrTracker::ResetReferencePose",
                               TLXArg(session, "Session"),
                               TLArg(time, "Time"));

        std::lock_guard lock(m_SampleMutex); 
        m_Session = session;
        m_Calibrated = false;
        auto forward = GetForwardView(session, time);
        if (!forward.has_value() ||
            !(m_LoadPoseFromFile ? LoadReferencePose() : ControllerBase::ResetReferencePose(session, time)))
        {
            if (auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance()))
            {
                layer->LogCurrentInteractionProfileAndSource("ResetReferencePose");
            }
            EventSink::Execute(Event::CalibrationLost);
            TraceLoggingWriteStop(local, "OpenXrTracker::ResetReferencePose", TLArg(false, "Success"));
            return false;
        }
        SetForwardRotation(forward.value());
        m_RefToFwd = xr::Normalize(Pose::Multiply({forward.value().orientation, {0, 0, 0}},
                                                  {Pose::Invert(m_ReferencePose).orientation, {0, 0, 0}}));
        m_ReferencePose = xr::Normalize(Pose::Multiply(m_RefToFwd, m_ReferencePose));

        EventSink::Execute(m_LoadPoseFromFile ? Event::Restored : Event::Calibrated);
        TraceLoggingWriteStop(local, "OpenXrTracker::ResetReferencePose", TLArg(true, "Success"));
        return true;
    }

    void OpenXrTracker::SaveReferencePose() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrTracker::SaveReferencePose");
        if (m_Calibrated)
        {
            SaveReferencePoseImpl(xr::Normalize(Pose::Multiply(Pose::Invert(m_RefToFwd), m_ReferencePose)));
        }
        TraceLoggingWriteStop(local, "OpenXrTracker::SaveReferencePose", TLArg(m_Calibrated, "Success"));
    }

    DataSource* OpenXrTracker::GetSource()
    {
        return m_Source.get();
    }

    bool OpenXrTracker::ReadSource(XrTime time, Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "OpenXrTracker::ReadSource",
                               TLArg(time, "Time"));

        std::lock_guard lock(m_SampleMutex); 
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        TraceLoggingWriteTagged(local, "OpenXrTracker::ReadSource", TLArg(now.QuadPart, "QuadPart"));
        XrTime nowXr;
        if (XR_FAILED(
                GetInstance()->xrConvertWin32PerformanceCounterToTimeKHR(GetInstance()->GetXrInstance(), &now, &nowXr)))
        {
            TraceLoggingWriteStop(local, "OpenXrTracker::ReadSource", TLArg(false, "Conversion"));
            return false;
        }
        XrPosef controllerPose;
        if (!GetControllerPose(controllerPose, m_Session, nowXr))
        {
            TraceLoggingWriteStop(local, "OpenXrTracker::ReadSource", TLArg(false, "Pose"));
            return false;
        }
        dof = PoseToDof(Pose::Multiply(m_RefToFwd, controllerPose));
        TraceLoggingWriteStop(local, "OpenXrTracker::ReadSource", TLArg(true, "Success"));
        return true;
    }

    bool OpenXrTracker::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrTracker::GetPose", TLXArg(session, "Session"), TLArg(time, "Time"));
        
        bool success; 
        
        if (!m_Sampler || !m_Calibrated)
        {
            success = GetControllerPose(trackerPose, session, time);
            if (m_Calibrated)
            {
                trackerPose = Pose::Multiply(m_RefToFwd, trackerPose);
            }
            TraceLoggingWriteTagged(local,
                                    "OpenXrTracker::GetPose",
                                    TLArg(false, "Sampled"),
                                    TLArg(m_Calibrated, "Calibrated"));
        }
        else
        {
            if (Dof momentary; m_Recorder && ReadSource(time, momentary))
            {
                m_Recorder->AddDofValues(momentary, Momentary);
            }

            Dof dof;
            success = m_Sampler->ReadData(dof, time);
            if (success)
            {
                m_Recorder->AddDofValues(dof, Read);
                trackerPose = DofToPose(dof);
            }
            TraceLoggingWriteTagged(local,
                                    "OpenXrTracker::GetPose",
                                    TLArg(true, "Sampled"));
        }

        TraceLoggingWriteStop(local,
                              "OpenXrTracker::GetPose",
                              TLArg(success, "Success"),
                              TLArg(xr::ToString(trackerPose).c_str(), "TrackerPose"));

        return success;
    }

    Dof OpenXrTracker::PoseToDof(const XrPosef& pose) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrTracker::PoseToDof", TLArg(xr::ToString(pose).c_str(), "Pose"));

        const XrPosef diff = Pose::Multiply(pose, Pose::Invert(m_ReferencePose));
        const auto angles = utility::ToEulerAngles(diff.orientation);
        Dof dof{diff.position.x * -1000,
                diff.position.z * 1000,
                diff.position.y * 1000,
                angles.y / angleToRadian,
                angles.z / angleToRadian,
                angles.x / angleToRadian};

        TraceLoggingWriteStop(local, "OpenXrTracker::PoseToDof", TLArg(xr::ToString(dof).c_str(), "Dof"));
        return dof;
    }

    XrPosef OpenXrTracker::DofToPose(const Dof& dof) const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "OpenXrTracker::PoseToDof", TLArg(xr::ToString(dof).c_str(), "Dof"));

        XrPosef controller;
        StoreXrQuaternion(&controller.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(dof.data[pitch] * angleToRadian,
                                                                    dof.data[yaw] * angleToRadian,
                                                                    dof.data[roll] * angleToRadian));
        controller.position = XrVector3f{dof.data[sway] / -1000.f, dof.data[heave] / 1000.f, dof.data[surge] / 1000.f};
        const XrPosef pose = xr::Normalize(Pose::Multiply(controller, m_ReferencePose));

        TraceLoggingWriteStop(local, "OpenXrTracker::PoseToDof", TLArg(xr::ToString(pose).c_str(), "Pose"));
        return pose;
    }

    bool OpenXrTracker::PhysicalSource::Open(int64_t time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "PhysicalSource::Open", TLArg(time, "Time"));

        Dof dof;
        const bool success = m_Tracker->ReadSource(time, dof);

        TraceLoggingWriteStop(local, "PhysicalSource::Open", TLArg(success, "Success"));
        return success;
    }

    bool VirtualTracker::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "VirtualTracker::Init");

        bool success{true};
        
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
        if (GetConfig()->GetFloat(Cfg::TrackerConstantPitch, value))
        {
            m_PitchConstant = fmod(value * angleToRadian + floatPi, 2.0f * floatPi) - floatPi;
            StoreXrQuaternion(&m_ConstantPitchQuaternion,
                              DirectX::XMQuaternionRotationRollPitchYaw(m_PitchConstant, 0, 0));
        } 
        else
        {
            success = false;
        }

        LogOffsetValues();

        if (GetConfig()->GetBool(Cfg::TrackerNonNeutralCalibration, m_NonNeutralCalibration))
        {
            Log("tracker pose is %s incorporated into COR calibration", m_NonNeutralCalibration ? "" : "not" );
        }
        else
        {
            success = false;
        }

        Log("constant pitch value = %.3f deg", m_PitchConstant / angleToRadian);

        
        if (!TrackerBase::Init())
        {
            success = false;
        }

        m_Manipulator = std::make_unique<CorManipulator>(CorManipulator(this));

        bool physicalEnabled{false};
        if (GetConfig()->GetBool(Cfg::PhysicalEnabled, physicalEnabled) && physicalEnabled)
        {
            m_Manipulator->Init();
        }
 
        TraceLoggingWriteStop(local,
                              "VirtualTracker::Init",
                              TLArg(success, "Success"),
                              TLArg(m_OffsetForward, "m_OffsetForward"),
                              TLArg(m_OffsetDown, "m_OffsetDown"),
                              TLArg(m_OffsetRight, "m_OffsetRight"),
                              TLArg(m_OffsetYaw, "m_OffsetYaw"));
        return success;
    }

    bool VirtualTracker::LazyInit(const XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "VirtualTracker::LazyInit",
                               TLArg(time, "Time"),
                               TLArg(m_SkipLazyInit, "SkipLazyInit"));

        bool success = true;
        if (!m_SkipLazyInit)
        {
            m_Mmf.SetName(m_Filename);

            if (!m_Mmf.Open(time))
            {
                ErrorLog(
                    "%s: unable to open mmf '%s'. Check if motion software is running and data output is activated!",
                    __FUNCTION__,
                    m_Filename.c_str());
                success = false;
            }
        }
        m_SkipLazyInit = success;
        TraceLoggingWriteStop(local, "VirtualTracker::LazyInit", TLArg(success, "Success"));

        return success;
    }

    bool VirtualTracker::ResetReferencePose(const XrSession session, const XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "VirtualTracker::ResetReferencePose",
                               TLXArg(session, "Session"),
                               TLArg(time, "Time"));
        bool success = true;
        if (m_LoadPoseFromFile)
        {
            success = LoadReferencePose();
            EventSink::Execute(Event::Restored);
        }
        else
        {
            auto forward = GetForwardView(session, time);
            if (!forward.has_value())
            {
                success = false;
            }
            else
            {
                TrackerBase::SetReferencePose(xr::Normalize(forward.value()), false);
                EventSink::Execute(Event::Calibrated);
            }
        }
        m_Calibrated = success;

        TraceLoggingWriteStop(local,
                              "VirtualTracker::ResetReferencePose",
                              TLArg(success, "Success"),
                              TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        return success;
    }

    void VirtualTracker::SetCorPose(const XrPosef& pose)
    {
        m_Manipulator->SetCorPose(pose);
    }

    void VirtualTracker::ApplyCorManipulation(XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "VirtualTracker::ApplyCorManipulation",
                               TLXArg(session, "Session"),
                               TLArg(time, "Time"));
        
        m_Manipulator->ApplyManipulation(session, time);

        TraceLoggingWriteStop(local, "VirtualTracker::ApplyCorManipulation");
    }

    bool VirtualTracker::ChangeOffset(XrVector3f modification)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "VirtualTracker::ChangeOffset",
                               TLArg(xr::ToString(modification).c_str(), "Modifiaction"),
                               TLArg(m_OffsetForward, "OffsetForward"),
                               TLArg(m_OffsetDown, "OffsetDown"),
                               TLArg(m_OffsetRight, "OffsetForward"));

        if (!TrackerBase::ChangeOffset(modification))
        {
            TraceLoggingWriteStop(local, "VirtualTracker::ChangeOffset", TLArg(false, "Success"));
            return false;
        }

        const XrVector3f relativeToHmd{cosf(m_OffsetYaw) * modification.x + sinf(m_OffsetYaw) * modification.z,
                                       modification.y,
                                       cosf(m_OffsetYaw) * modification.z - sinf(m_OffsetYaw) * modification.x};
        m_OffsetForward -= relativeToHmd.z;
        m_OffsetDown -= relativeToHmd.y;
        m_OffsetRight += relativeToHmd.x;
        GetConfig()->SetValue(Cfg::TrackerOffsetForward, m_OffsetForward * 100.0f);
        GetConfig()->SetValue(Cfg::TrackerOffsetDown, m_OffsetDown * 100.0f);
        GetConfig()->SetValue(Cfg::TrackerOffsetRight, m_OffsetRight * 100.0f);

        TraceLoggingWriteStop(local,
                              "VirtualTracker::ChangeOffset",
                              TLArg(true, "Success"),
                              TLArg(m_OffsetForward, "NewOffsetForward"),
                              TLArg(m_OffsetDown, "NewOffsetDown"),
                              TLArg(m_OffsetRight, "NewOffsetForward"));
        return true;
    }

    bool VirtualTracker::ChangeRotation(float radian)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "VirtualTracker::ChangeRotation",
                               TLArg(radian, "Radian"),
                               TLArg(m_OffsetYaw, "OffsetYaw"),
                               TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        if (!TrackerBase::ChangeRotation(radian))
        {
            TraceLoggingWriteStop(local, "VirtualTracker::ChangeRotation", TLArg(false, "Success"));
            return false;
        }

        m_OffsetYaw += radian;
        // restrict to +- pi
        m_OffsetYaw = fmod(m_OffsetYaw + floatPi, 2.0f * floatPi) - floatPi;
        const float yawAngle = m_OffsetYaw / angleToRadian;
        GetConfig()->SetValue(Cfg::TrackerOffsetYaw, yawAngle);

        TraceLoggingWriteTagged(local, "VirtualTracker::ChangeRotation", TLArg(yawAngle, "YawAngle"));

        TraceLoggingWriteStop(local,
                              "VirtualTracker::ChangeRotation",
                              TLArg(true, "Success"),
                              TLArg(m_OffsetYaw, "OffsetYaw"),
                              TLArg(xr::ToString(this->m_ReferencePose).c_str(), "ReferencePose"));
        return true;
    }

    void VirtualTracker::LogOffsetValues() const
    {
        Log("offset values: forward = %.3f m, down = %.3f m, right = %.3f m, yaw = %.3f deg",
            m_OffsetForward,
            m_OffsetDown,
            m_OffsetRight,
            m_OffsetYaw / angleToRadian);
    }

    DataSource* VirtualTracker::GetSource()
    {
        return &m_Mmf;
    }

    void VirtualTracker::SetReferencePose(const ::XrPosef& pose, const bool silent)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "VirtualTracker::SetReferencePose", TLArg(xr::ToString(pose).c_str(), "Pose"));

        SetForwardRotation(pose);
        TrackerBase::SetReferencePose(pose, silent);

        TraceLoggingWriteStop(local, "VirtualTracker::SetReferencePose");
    }

    bool VirtualTracker::GetPose(XrPosef& trackerPose, const XrSession session, const XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "VirtualTracker::GetPose", TLXArg(session, "Session"), TLArg(time, "Time"));

        Dof dof{};
        if (!ReadData(time, dof))
        {
            TraceLoggingWriteStop(local, "VirtualTracker::GetPose", TLArg(false, "Success"));
            return false;
        }
        m_Recorder->AddDofValues(dof, Read);

        DebugLog("MotionData: %s", xr::ToString(dof).c_str());
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::GetPose",
                                TLArg(xr::ToString(dof).c_str(), "Data"));

        XrPosef rigPose = DataToPose(dof);
        TraceLoggingWriteTagged(local, "VirtualTracker::GetPose", TLArg(xr::ToString(rigPose).c_str(), "RigPose"));

        if (0 != m_PitchConstant)
        {
            StoreXrQuaternion(&rigPose.orientation,
                              DirectX::XMQuaternionMultiply(LoadXrQuaternion(m_ConstantPitchQuaternion),
                                                            LoadXrQuaternion(rigPose.orientation)));

            TraceLoggingWriteTagged(local,
                                    "VirtualTracker::GetPose",
                                    TLArg(xr::ToString(rigPose).c_str(), "RigPose_Pitched"),
                                    TLArg(std::to_string(this->m_PitchConstant).c_str(), "PitchConstant"));
        }


        trackerPose = Pose::Multiply(rigPose, m_ReferencePose);
        TraceLoggingWriteStop(local,
                              "VirtualTracker::GetPose",
                              TLArg(true, "Success"),
                              TLArg(xr::ToString(trackerPose).c_str(), "TrackerPose"));
        return true;
    }

    std::optional<XrPosef> VirtualTracker::GetForwardView(XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "VirtualTracker::GetCurrentView", TLArg(time, "Time"));

        auto view = TrackerBase ::GetCurrentView(session, time);
        if (!view.has_value())
        {
            TraceLoggingWriteStop(local, "VirtualTracker::GetCurrentView", TLArg(false, "BaseSuccess"));
            return {};
        }
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::GetCurrentView",
                                TLArg(xr::ToString(view.value()).c_str(), "ViewPoseOriginal"));
        // apply offset according to current virtual tracker pose
        Dof dof{};
        if (!ReadData(time, dof))
        {
            ErrorLog("%s: unable to get current virtual tracker pose for calibration", __FUNCTION__);
            TraceLoggingWriteStop(local, "VirtualTracker::GetCurrentView", TLArg(false, "ReadSuccess"));
            return {};
        }
        TraceLoggingWriteTagged(local, "VirtualTracker::GetCurrentView", TLArg(xr::ToString(dof).c_str(), "Data"));

        XrPosef tracker = DataToPose(dof);
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::GetCurrentView",
                                TLArg(xr::ToString(tracker).c_str(), "Tracker"));

        ApplyOffsets(dof, view.value());
        TraceLoggingWriteStop(local,
                              "VirtualTracker::GetCurrentView",
                              TLArg(false, "Success"),
                              TLArg(xr::ToString(view.value()).c_str(), "View"));

        // calculate leveled orientation (parallel to floor)
        view.value().orientation = GetLeveledRotation(GetForwardVector(view.value().orientation), 0);

        return view;
    }
    
    bool VirtualTracker::ReadData(XrTime time, Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "VirtualTracker::ReadData", TLArg(time, "Time"));

        if (!m_Sampler)
        {
            if (!ReadSource(time, dof))
            {
                TraceLoggingWriteStop(local, "VirtualTracker::ReadData", TLArg(false, "Success"));
                return false;
            }
        }
        else
        {
            if (Dof momentary; m_Recorder && ReadSource(time, momentary))
            {
                m_Recorder->AddDofValues(momentary, Momentary);
            }
            if (!m_Sampler->ReadData(dof, time))
            {
                TraceLoggingWriteStop(local, "VirtualTracker::ReadData", TLArg(false, "Success"));
                return false;
            }
        }
        DebugLog("virtual tracker data: %s", xr::ToString(dof).c_str());

        TraceLoggingWriteTagged(local,
                                "VirtualTracker::GetVirtualPose",
                                TLArg(dof.data[yaw], "Yaw"),
                                TLArg(dof.data[pitch], "Pitch"),
                                TLArg(dof.data[roll], "Yaw"));

        TraceLoggingWriteStop(local, "VirtualTracker::ReadData", TLArg(true, "Success"));
        return true;
    }

    void VirtualTracker::ApplyOffsets(const Dof& dof, XrPosef& view)
    {
        using namespace DirectX;

        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "VirtualTracker::ApplyOffsets",
                               TLArg(xr::ToString(view).c_str(), "View"),
                               TLArg(xr::ToString(dof).c_str(), "Dof"));

        XrVector3f configTranslation{m_OffsetRight, -m_OffsetDown, -m_OffsetForward};

        if (!m_NonNeutralCalibration)
        {
            // calculate leveled view orientation
            view.orientation = GetLeveledRotation(GetForwardVector(view.orientation), 0);
            // apply config offset
            view = Pose::Multiply({GetLeveledRotation({0, 0, 1}, m_OffsetYaw), configTranslation}, view);
            TraceLoggingWriteStop(local,
                                  "VirtualTracker::ApplyOffsets",
                                  TLArg(false, "Active"),
                                  TLArg(xr::ToString(view).c_str(), "view"));
            return;
        }

        // get tracker rotations
        XrQuaternionf trackerYaw = DataToPose({0, 0, 0, dof.data[yaw], 0, 0}).orientation;
        const XrQuaternionf trackerRoll = DataToPose({0, 0, 0, 0, dof.data[roll], 0}).orientation;
        const XrQuaternionf trackerPitch = DataToPose({0, 0, 0, 0, 0, dof.data[pitch]}).orientation;
        XrQuaternionf trackerPitchRoll;
        StoreXrQuaternion(
            &trackerPitchRoll,
            XMQuaternionNormalize(XMQuaternionMultiply(LoadXrQuaternion(trackerPitch), LoadXrQuaternion(trackerRoll))));

        TraceLoggingWriteTagged(local,
                                "VirtualTracker::ApplyOffsets",
                                TLArg(xr::ToString(trackerYaw).c_str(), "Yaw"),
                                TLArg(xr::ToString(trackerPitchRoll).c_str(), "PitchRoll"));

        // get translations
        const XrVector3f trackerTranslation =
            DataToPose({dof.data[sway], dof.data[surge], dof.data[heave], 0, 0, 0}).position;
        const XrVector3f viewTranslation = view.position;
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::ApplyOffsets",
                                TLArg(xr::ToString(trackerYaw).c_str(), "Yaw"),
                                TLArg(xr::ToString(trackerPitchRoll).c_str(), "PitchRoll"),
                                TLArg(xr::ToString(trackerTranslation).c_str(), "TrackerTranslation"),
                                TLArg(xr::ToString(viewTranslation).c_str(), "ViewTranslation"));

        // calculate euler angles
        const XrVector3f viewAngles = ToEulerAngles(view.orientation);
        const XrVector3f trackerAngles = ToEulerAngles(trackerPitchRoll);
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::ApplyOffsets",
                                TLArg(xr::ToString(viewAngles).c_str(), "ViewAngles"),
                                TLArg(xr::ToString(trackerAngles).c_str(), "TrackerAngles"));

        // transfer yaw component from pitch/roll to yaw to eliminate positional offset
        StoreXrQuaternion(&trackerPitchRoll, XMQuaternionRotationRollPitchYaw(trackerAngles.x, 0, trackerAngles.z));
        StoreXrQuaternion(
            &trackerYaw,
            XMQuaternionNormalize(XMQuaternionMultiply(LoadXrQuaternion(trackerYaw),
                                                       XMQuaternionRotationRollPitchYaw(0, trackerAngles.y, 0))));
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::ApplyOffsets",
                                TLArg(xr::ToString(trackerYaw).c_str(), "Yaw_Modified"),
                                TLArg(xr::ToString(trackerPitchRoll).c_str(), "PitchRoll_Modified"));

        // offset yaw angle of view to account for local pitch deviation of hmd pose
        float yawCorrectionAngle = (trackerAngles.x > 1e-3f || trackerAngles.x < -1e-3f)
                                       ? trackerAngles.y * (trackerAngles.x - viewAngles.x) / trackerAngles.x
                                       : 0;
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::ApplyOffsets",
                                TLArg(std::to_string(yawCorrectionAngle).c_str(), "YawCorrectionAngle"));

        XrQuaternionf viewYawCorrected;
        StoreXrQuaternion(&viewYawCorrected, XMQuaternionRotationRollPitchYaw(0, viewAngles.y + yawCorrectionAngle, 0));

        // apply rig offsets in local space
        XrPosef viewLocal{GetLeveledRotation({0, 0, 1}, m_OffsetYaw), configTranslation - trackerTranslation};
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::ApplyOffsets",
                                TLArg(xr::ToString(viewLocal).c_str(), "ViewLocal"));

        // rotate from local tracker space into 'fwd' space (pitch first, roll second)
        XrPosef viewGlobal = Pose::Multiply(viewLocal, {trackerPitchRoll, {}});

        // apply yaw rotation without changing position vector
        StoreXrQuaternion(
            &viewGlobal.orientation,
            XMQuaternionNormalize(XMQuaternionMultiply(LoadXrQuaternion(viewGlobal.orientation),
                                                       XMQuaternionInverse(LoadXrQuaternion(trackerYaw)))));
        TraceLoggingWriteTagged(local,
                                "VirtualTracker::ApplyOffsets",
                                TLArg(xr::ToString(viewGlobal).c_str(), "ViewGlobal_Rotated"));

        // rotate view according to hmd yaw angle (including position) and add view translation
        view = Pose::Multiply(viewGlobal, {viewYawCorrected, view.position});
        TraceLoggingWriteStop(local, "VirtualTracker::ApplyOffsets", TLArg(xr::ToString(view).c_str(), "view"));
    }

    bool YawTracker::ReadSource(XrTime now, Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "YawTracker::ReadSource", TLArg(now, "Now"));

        YawData mmfData{};
        if (!m_Mmf.Read(&mmfData, sizeof(mmfData), now))
        {
            TraceLoggingWriteStop(local, "YawTracker::ReadSource", TLArg(false, "Success"));
            return false;
        }
        dof = {0, 0, 0, mmfData.yaw, mmfData.roll, mmfData.pitch};

        TraceLoggingWriteStop(local,
                              "YawTracker::ReadSource",
                              TLArg(mmfData.yaw, "Yaw"),
                              TLArg(mmfData.roll, "Roll"),
                              TLArg(mmfData.pitch, "Pitch"),
                              TLArg(true, "Success"));
        return true;
    }

    XrPosef YawTracker::DataToPose(const Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "YawTracker::DataToPose", TLArg(xr::ToString(dof).c_str(), "Dof"));

        XrPosef rigPose{Pose::Identity()};
        StoreXrQuaternion(&rigPose.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(-dof.data[pitch] * angleToRadian,
                                                                    -dof.data[yaw] * angleToRadian,
                                                                    dof.data[roll] * angleToRadian));

        TraceLoggingWriteStop(local, "YawTracker::DataToPose", TLArg(xr::ToString(rigPose).c_str(), "Pose"));
        return rigPose;
    }

    bool SixDofTracker::ReadSource(XrTime now, Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "SixDofTracker::ReadSource", TLArg(now, "Now"));
        
        SixDof mmfData{};
        if (!m_Mmf.Read(&mmfData, sizeof(mmfData), now))
        {
            TraceLoggingWriteStop(local, "SixDofTracker::ReadSource", TLArg(false, "Read"));
            return false;
        }

        TraceLoggingWriteTagged(local,
                                "SixDofTracker::ReadSource",
                                TLArg(mmfData.sway, "Sway"),
                                TLArg(mmfData.surge, "Surge"),
                                TLArg(mmfData.heave, "Heave"),
                                TLArg(mmfData.yaw, "Yaw"),
                                TLArg(mmfData.roll, "Roll"),
                                TLArg(mmfData.pitch, "Pitch"));

        dof.data[sway] = static_cast<float>(mmfData.sway);
        dof.data[surge]= static_cast<float>(mmfData.surge);
        dof.data[heave] = static_cast<float>(mmfData.heave);
        dof.data[yaw] = static_cast<float>(mmfData.yaw);
        dof.data[pitch] = static_cast<float>(mmfData.pitch);
        dof.data[roll] =  static_cast<float>(mmfData.roll);

        TraceLoggingWriteStop(local, "SixDofTracker::ReadSource", TLArg(true, "Success"));
        return true;
    }

    XrPosef SixDofTracker::DataToPose(const Dof& dof)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "SixDofTracker::DataToPose", TLArg(xr::ToString(dof).c_str(), "Dof"));

        XrPosef rigPose{Pose::Identity()};
        ExtractRotationQuaternion(dof, rigPose);
        ExtractTranslationVector(dof, rigPose);

        TraceLoggingWriteStop(local, "SixDofTracker::DataToPose", TLArg(xr::ToString(rigPose).c_str(), "Pose"));
        return rigPose;
    }

    void SixDofTracker::ExtractTranslationVector(const utility::Dof& dof, XrPosef& rigPose)
    {
        // helper to rotate translation vector into 'local rig space'
        StoreXrVector3(
            &rigPose.position,
            DirectX::XMVector3Rotate({dof.data[sway] / 1000.f, dof.data[heave] / 1000.f, dof.data[surge] / -1000.f},
                                     LoadXrQuaternion(rigPose.orientation)));
    }
    
    void FlyPtTracker::ExtractRotationQuaternion(const Dof& dof, XrPosef& rigPose)
    {
        StoreXrQuaternion(&rigPose.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(dof.data[pitch] * angleToRadian,
                                                                    dof.data[yaw] * angleToRadian,
                                                                    -dof.data[roll] * angleToRadian));
    }

    void SrsTracker::ExtractRotationQuaternion(const Dof& dof, XrPosef& rigPose)
    {
        StoreXrQuaternion(&rigPose.orientation,
                          DirectX::XMQuaternionRotationRollPitchYaw(-dof.data[pitch] * angleToRadian,
                                                                    dof.data[yaw] * angleToRadian,
                                                                    dof.data[roll] * angleToRadian));
    }

    void CorManipulator::ApplyManipulation(XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "CorManipulator::ApplyManipulation",
                               TLXArg(session, "Session"),
                               TLArg(time, "Time"));

        if (!m_Initialized)
        {
            TraceLoggingWriteStop(local, "CorManipulator::ApplyManipulation", TLArg(false, "Initilaized"));
            return;
        }

        auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (!layer)
        {
            m_Initialized = false;
            ErrorLog("%s: unable to cast layer to OpenXrLayer", __FUNCTION__);
            TraceLoggingWriteStop(local, "CorManipulator::ApplyManipulation", TLArg(false, "Layer_Valid"));
            return;
        }

        bool movePressed{false}, positionPressed{false};
        GetButtonState(session, movePressed, positionPressed);

        TraceLoggingWriteTagged(local,
                                "ApplyManipulation",
                                TLArg(movePressed, "MovePressed"),
                                TLArg(m_MoveActive, "MoveActive"),
                                TLArg(positionPressed, "PositionPressed"),
                                TLArg(m_PositionActive, "PositionActive"));

        // apply vibration to acknowledge button state change
        if ((positionPressed && !m_PositionActive) || movePressed != m_MoveActive)
        {
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude = 0.75;
            vibration.duration = XR_MIN_HAPTIC_DURATION;
            vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

            TraceLoggingWriteTagged(local, "ApplyManipulation", TLXArg(layer->m_HapticAction, "xrApplyHapticFeedback"));
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
                    SetReferencePose(m_LastPose, true);
                }
            }
        }
        m_PositionActive = positionPressed;
        m_MoveActive = !positionPressed && movePressed;

        TraceLoggingWriteStop(local,
                              "CorManipulator::ApplyManipulation",
                              TLArg(m_PositionActive, "PositionActive"),
                              TLArg(m_MoveActive, "MoveActive"));
    }

    void CorManipulator::SetCorPose(const XrPosef& pose)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "CorManipulator::SetCorPose", TLArg(xr::ToString(pose).c_str(), "pose"));
        m_LastPose = pose;
        ApplyPosition();

        TraceLoggingWriteStop(local, "CorManipulator::SetCorPose");
    }

    bool CorManipulator::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "CorManipulator::GetPose", TLXArg(session, "Session"), TLArg(time, "Time"));

        const bool success = GetControllerPose(trackerPose, session, time);

        TraceLoggingWriteStop(local,
                              "CorManipulator::GetPose",
                              TLArg(success, "Success")); // removed parameter
        return success;
    }

    void CorManipulator::GetButtonState(XrSession session, bool& moveButton, bool& positionButton)
    {
         TraceLocalActivity(local);
         TraceLoggingWriteStart(local, "CorManipulator::GetButtonState", TLXArg(session, "Session"));

         auto* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
         if (!layer)
         {
            m_Initialized = false;
            ErrorLog("%s: unable to cast layer to OpenXrLayer", __FUNCTION__);
            TraceLoggingWriteStop(local, "CorManipulator::GetButtonState", TLArg(false, "Layer_Valid"));
            return;
         }
         if (!layer->SyncActions("GetButtonState"))
         {
             TraceLoggingWriteStop(local, "CorManipulator::GetButtonState", TLArg(false, "SyncActionSuccces"));
             return;
         }
         {
            // obtain current action state
            TraceLoggingWriteTagged(local,
                                    "CorManipulator::GetButtonState",
                                    TLXArg(layer->m_MoveAction, "xrGetActionStateBoolean"));
            XrActionStateGetInfo moveInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            moveInfo.action = layer->m_MoveAction;

            XrActionStateBoolean moveButtonState{XR_TYPE_ACTION_STATE_BOOLEAN};
            if (const XrResult result = layer->xrGetActionStateBoolean(session, &moveInfo, &moveButtonState);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrGetActionStateBoolean failed: %s", __FUNCTION__, xr::ToCString(result));
                TraceLoggingWriteStop(local,
                                      "CorManipulator::GetButtonState",
                                      TLArg(false, "xrGetActionStateBooleanSuccess_Move"));
                return;
            }
            if (moveButtonState.isActive == XR_TRUE)
            {
                moveButton = XR_TRUE == moveButtonState.currentState;
                TraceLoggingWriteTagged(local, "CorManipulator::GetButtonState", TLArg(moveButton, "MoveButtonState"));
            }

            TraceLoggingWriteTagged(local,
                                    "CorManipulator::GetButtonState",
                                    TLXArg(layer->m_SnapAction, "xrGetActionStateBoolean"));
            XrActionStateGetInfo positionInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            positionInfo.action = layer->m_SnapAction;

            XrActionStateBoolean positionButtonState{XR_TYPE_ACTION_STATE_BOOLEAN};
            if (const XrResult result = layer->xrGetActionStateBoolean(session, &positionInfo, &positionButtonState);
                XR_FAILED(result))
            {
                ErrorLog("%s: xrGetActionStateBoolean failed: %s", __FUNCTION__, xr::ToCString(result));
                TraceLoggingWriteStop(local,
                                      "CorManipulator::GetButtonState",
                                      TLArg(false, "xrGetActionStateBooleanSuccess_Position"));
                return;
            }
            if (positionButtonState.isActive == XR_TRUE)
            {
                positionButton = XR_TRUE == positionButtonState.currentState;
                TraceLoggingWriteTagged(local,
                                        "CorManipulator::GetButtonState",
                                        TLArg(positionButton, "PositionButtonState"));
            }
         }
         TraceLoggingWriteStop(local, "CorManipulator::GetButtonState", TLArg(true, "Success"));
    }

    void CorManipulator::ApplyPosition() const
    {
         TraceLocalActivity(local);
         TraceLoggingWriteStart(local,
                                "CorManipulator::ApplyPosition",
                                TLArg(xr::ToString(this->m_LastPose).c_str(), "tracker"));

         const XrPosef corPose = m_Tracker->GetReferencePose();
         const auto [relativeOrientation, relativePosition] = Pose::Multiply(m_LastPose, Pose::Invert(corPose));
         m_Tracker->ChangeOffset(relativePosition);
         const float angleDelta = GetYawAngle(GetForwardVector(m_LastPose.orientation)) -
                                  GetYawAngle(GetForwardVector(corPose.orientation));
         m_Tracker->ChangeRotation(angleDelta);

         TraceLoggingWriteStop(local, "CorManipulator::ApplyPosition");
    }

    void CorManipulator::ApplyTranslation() const
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "CorManipulator::ApplyTranslation",
                               TLArg(xr::ToString(this->m_ReferencePose).c_str(), "Reference"),
                               TLArg(xr::ToString(this->m_LastPose).c_str(), "tracker"));

        const XrPosef corPose = m_Tracker->GetReferencePose();
        const DirectX::XMVECTOR rot = LoadXrQuaternion(corPose.orientation);
        const DirectX::XMVECTOR ref = LoadXrVector3(m_ReferencePose.position);
        const DirectX::XMVECTOR current = LoadXrVector3(m_LastPose.position);
        XrVector3f relativeTranslation;
        StoreXrVector3(
            &relativeTranslation,
            DirectX::XMVector3InverseRotate(DirectX::XMVectorAdd(current, DirectX::XMVectorNegate(ref)), rot));
        m_Tracker->ChangeOffset(relativeTranslation);

        TraceLoggingWriteStop(local, "CorManipulator::ApplyTranslation");
    }

    void CorManipulator::ApplyRotation(const XrPosef& poseDelta) const
    {
         TraceLocalActivity(local);
         TraceLoggingWriteStart(local,
                                "CorManipulator::ApplyRotation",
                                TLArg(xr::ToString(poseDelta).c_str(), "Delta"));

         const float yawAngle = -GetYawAngle(GetForwardVector(poseDelta.orientation));
         m_Tracker->ChangeRotation(yawAngle);

         TraceLoggingWriteStop(local, "CorManipulator::ApplyRotation");
    }

    bool ViveTrackerInfo::Init()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "ViveTrackerInfo::Init");

        std::string trackerType;
        if (!GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            TraceLoggingWriteStop(local, "ViveTrackerInfo::Init", TLArg(false, "TrackerType_Get"));
            return false;
        }
        if ("vive" == trackerType)
        {
            if (!GetInstance()->IsExtensionGranted(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME))
            {
                ErrorLog("%s: runtime does not support Vive tracker OpenXR extension: %s",
                         __FUNCTION__,
                         XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);
                TraceLoggingWriteStop(local, "ViveTrackerInfo::Init", TLArg(false, "Extansion_Granted"));
                return false;
            }
            std::string role;
            if (!GetConfig()->GetString(Cfg::TrackerRole, role))
            {
                TraceLoggingWriteStop(local, "ViveTrackerInfo::Init", TLArg(false, "Side_Get"));
                return false;
            }
            if (!validRoles.contains(role))
            {
                ErrorLog("%s: invalid role for vive tracker: %s", __FUNCTION__, role.c_str());
                std::string validOptions;
                for (const auto& option : validRoles)
                {
                    validOptions += (validOptions.empty() ? "" : ", ") + option;
                }
                Log("try one of the following instead: %s", validOptions.c_str());
                TraceLoggingWriteStop(local, "ViveTrackerInfo::Init", TLArg(false, "Side_Valid"));
                return false;
            }
            path = "/user/vive_tracker_htcx/role/" + role;
            Log("vive tracker is using path %s", path.c_str());
            active = true;
        }
        TraceLoggingWriteStop(local,
                              "ViveTrackerInfo::Init",
                              TLArg(true, "Success"),
                              TLArg(active, "Active"),
                              TLArg(path.c_str(), "Path"));
        return true;
    }

    std::unique_ptr<tracker::TrackerBase> GetTracker()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "GetTracker");

        std::string trackerType;
        if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
        {
            if ("yaw" == trackerType)
            {
                Log("Yaw Game Engine memory mapped file is used as reference tracker");
                TraceLoggingWriteStop(local, "GetTracker", TLPArg(trackerType.c_str(), "tracker"));
                return std::make_unique<YawTracker>();
            }
            if ("srs" == trackerType)
            {
                Log("SRS memory mapped file is used as reference tracker");
                TraceLoggingWriteStop(local, "GetTracker", TLPArg(trackerType.c_str(), "tracker"));
                return std::make_unique<SrsTracker>();
            }
            if ("flypt" == trackerType)
            {
                Log("FlyPT Mover memory mapped file is used as reference tracker");
                TraceLoggingWriteStop(local, "GetTracker", TLPArg(trackerType.c_str(), "tracker"));
                return std::make_unique<FlyPtTracker>();
            }
            if ("controller" == trackerType)
            {
                Log("%s motion controller is used as reference tracker", GetConfig()->GetControllerSide().c_str());
                TraceLoggingWriteStop(local, "GetTracker", TLPArg(trackerType.c_str(), "tracker"));
                return std::make_unique<OpenXrTracker>();
            }
            if ("vive" == trackerType)
            {
                std::string role;
                GetConfig()->GetString(Cfg::TrackerRole, role);
                Log("vive tracker (%s) is used as reference tracker", role.c_str());
                TraceLoggingWriteStop(local, "GetTracker", TLPArg(trackerType.c_str(), "tracker"));
                return std::make_unique<OpenXrTracker>();
            }
            ErrorLog("%s: unknown tracker type: %s", __FUNCTION__, trackerType.c_str());
        }
        else
        {
            ErrorLog("%s: unable to determine tracker type", __FUNCTION__);
        }
        ErrorLog("defaulting to 'controller'");
        TraceLoggingWriteStop(local, "GetTracker", TLPArg("Default", "tracker"));
        return std::make_unique<OpenXrTracker>();
    }
} // namespace tracker