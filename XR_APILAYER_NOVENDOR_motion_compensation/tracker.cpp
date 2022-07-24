// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "tracker.h"
#include "layer.h"
#include <log.h>
#include <util.h>

using namespace motion_compensation_layer;
using namespace motion_compensation_layer::log;
using namespace xr::math;

OpenXrTracker::OpenXrTracker() {}

OpenXrTracker::~OpenXrTracker()
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

bool OpenXrTracker::Init()
{
    // Create the resources for the tracker space.
    {
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO, nullptr};
        strcpy_s(actionSetCreateInfo.actionSetName, "general_tracker_set");
        strcpy_s(actionSetCreateInfo.localizedActionSetName, "General Tracker Set");
        actionSetCreateInfo.priority = 0;
        if (XR_SUCCESS != GetInstance()->xrCreateActionSet(GetInstance()->GetXrInstance(), &actionSetCreateInfo, &m_ActionSet))
        {
            ErrorLog("%s: error creating action set\n", __FUNCTION__);
            return false;
        }
        TraceLoggingWrite(g_traceProvider, "OpenXrTracker::Init", TLPArg(m_ActionSet, "xrCreateActionSet"));
    }
    {
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO, nullptr};
        strcpy_s(actionCreateInfo.actionName, "general_tracker");
        strcpy_s(actionCreateInfo.localizedActionName, "General Tracker");
        actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        actionCreateInfo.countSubactionPaths = 0;
        if (XR_SUCCESS != GetInstance()->xrCreateAction(m_ActionSet, &actionCreateInfo, &m_TrackerPoseAction))
        {
            ErrorLog("%s: error creating action\n", __FUNCTION__);
            return false;
        }
        TraceLoggingWrite(g_traceProvider, "OpenXrTracker::Init", TLPArg(m_TrackerPoseAction, "xrCreateAction"));
    }
    {
        // suggest simple controller
        XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING, nullptr};
        CHECK_XRCMD(GetInstance()->xrStringToPath(GetInstance()->GetXrInstance(),
                                                  "/interaction_profiles/khr/simple_controller",
                                                  &suggestedBindings.interactionProfile));
        std::string side{"left"};
        if (!GetConfig()->GetString(Cfg::TrackerParam, side))
        {
            ErrorLog("%s: unable to determine contoller side. Defaulting to %s\n", __FUNCTION__, side);
        }
        if ("right" != side && "left" != side)
        {
            ErrorLog("%s: invalid contoller side: %s. Defaulting to 'left'\n", __FUNCTION__, side);
            side = "left";
        }
        else
        {
            const std::string path("/user/hand/" + side + "/input/grip/pose");
            XrActionSuggestedBinding binding;
            binding.action = m_TrackerPoseAction;
            CHECK_XRCMD(GetInstance()->xrStringToPath(GetInstance()->GetXrInstance(), path.c_str(), &binding.binding));

            suggestedBindings.suggestedBindings = &binding;
            suggestedBindings.countSuggestedBindings = 1;
            CHECK_XRCMD(GetInstance()->xrSuggestInteractionProfileBindings(GetInstance()->GetXrInstance(),
                                                                                      &suggestedBindings));
            TraceLoggingWrite(g_traceProvider,
                              "OpenXrTracker::Init",
                              TLArg("/interaction_profiles/khr/simple_controller", "Profile"),
                              TLPArg(binding.action, "Action"),
                              TLArg(path.c_str(), "Path"));
        }
    }
    {
        // set up filters
        int orderTrans, orderRot;
        float strengthTrans, strengthRot; 
        if (!GetConfig()->GetInt(Cfg::TransOrder, orderTrans) || 
            !GetConfig()->GetInt(Cfg::RotOrder, orderRot) ||
            !GetConfig()->GetFloat(Cfg::TransStrength, strengthTrans) ||
            !GetConfig()->GetFloat(Cfg::RotStrength, strengthRot))
        {
            ErrorLog("%s: error reading configured values\n", __FUNCTION__);
            return false;
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
        m_TransStrength = strengthTrans;
        m_RotStrength = strengthRot;


        Log("translational filter stages: %d\n", orderTrans);
        m_TransFilter = 1 == orderTrans   ? new utility::SingleEmaFilter(m_TransStrength)
                        : 2 == orderTrans ? new utility::DoubleEmaFilter(m_TransStrength)
                                          : new utility::TripleEmaFilter(m_TransStrength);
        
        Log("rotational filter stages: %d\n", orderRot);
        m_RotFilter = 1 == orderRot   ? new utility::SingleSlerpFilter(m_RotStrength)
                      : 2 == orderRot ? new utility::DoubleSlerpFilter(m_RotStrength)
                                      : new utility::TripleSlerpFilter(m_RotStrength);
        
    }
    return true;
}

bool OpenXrTracker::LazyInit()
{
    bool success = true;
    if (m_ReferenceSpace == XR_NULL_HANDLE)
    {
        Log("reference space created during lazy init\n");
        // Create a reference space.
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
        TraceLoggingWrite(g_traceProvider, "OpenXrTracker::LazyInit", TLPArg("Executed", "xrCreateReferenceSpace"));
        if (!XR_SUCCEEDED(GetInstance()->xrCreateReferenceSpace(m_Session, &referenceSpaceCreateInfo, &m_ReferenceSpace)))
        {
            ErrorLog("%s: xrCreateReferenceSpace failed\n", __FUNCTION__ );
            success = false;
        }
    }

    if (!m_SkipLazyInit)
    {
        Log("action set attached during lazy init\n");
        std::vector<XrActionSet> actionSets;
        XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
                                                          nullptr,
                                                          0,
                                                          actionSets.data()};
        TraceLoggingWrite(g_traceProvider,
                          "OpenXrTracker::LazyInit", TLPArg("Executed", "xrAttachSessionActionSets"));
        if (!XR_SUCCEEDED(GetInstance()->xrAttachSessionActionSets(m_Session, &actionSetAttachInfo)))
        {
            ErrorLog("%s: xrAttachSessionActionSets failed\n", __FUNCTION__);
            success = false;
        }
    }
    return success;
}

void OpenXrTracker::beginSession(XrSession session)
{
    m_Session = session;

    // Create a reference space.
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
    CHECK_XRCMD(GetInstance()->xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_ReferenceSpace));
    TraceLoggingWrite(g_traceProvider,
                      "OpenXrTracker::beginSession",
                      TLPArg(m_ReferenceSpace, "xrCreateReferenceSpace"));

    // create action space
    XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr};
    actionSpaceCreateInfo.action = m_TrackerPoseAction;
    actionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
    actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
    CHECK_XRCMD(GetInstance()->xrCreateActionSpace(m_Session, &actionSpaceCreateInfo, &m_TrackerSpace));
    TraceLoggingWrite(g_traceProvider, "OpenXrTracker::beginSession", TLPArg(m_TrackerSpace, "xrCreateActionSpace"));


}

void OpenXrTracker::endSession()
{
    m_Session = XR_NULL_HANDLE;

    if (m_TrackerSpace != XR_NULL_HANDLE)
    {
        GetInstance()->xrDestroySpace(m_TrackerSpace);
        m_TrackerSpace = XR_NULL_HANDLE;
    }
}

bool OpenXrTracker::ResetReferencePose(XrTime frameTime)
{
    XrPosef curPose;
    if (GetPose(curPose, frameTime))
    {
        m_TransFilter->Reset(curPose.position);
        m_RotFilter->Reset(curPose.orientation);
        m_ReferencePose = curPose;
        m_Calibrated = true;
        Log("tracker reference pose reset\n");
        return true;
    }
    else
    {
        ErrorLog("%s: unable to get current pose\n", __FUNCTION__);
        m_Calibrated = false;
        return false;
    }
}

bool OpenXrTracker::GetPoseDelta(XrPosef& poseDelta, XrTime frameTime)
{
    // pose already calulated for requested time or unable to calculate
    if (frameTime == m_LastPoseTime)
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
        m_ResetReferencePose = !ResetReferencePose(frameTime);
    }
    XrPosef curPose;
    if (GetPose(curPose, frameTime))
    {      
        // apply translational filter
        m_TransFilter->Filter(curPose.position);

        // apply rotational filter
        m_RotFilter->Filter(curPose.orientation);

        TraceLoggingWrite(g_traceProvider,
                          "GetPoseDelta",
                          TLArg(xr::ToString(curPose).c_str(), "Location_After_Filter"),
                          TLArg(frameTime, "Time"));

        // calculate difference toward reference pose
        poseDelta = Pose::Multiply(Pose::Invert(curPose), m_ReferencePose);

        TraceLoggingWrite(g_traceProvider, "GetPoseDelta", TLArg(xr::ToString(poseDelta).c_str(), "Delta"));
  
        m_LastPoseTime = frameTime;
        m_LastPoseDelta = poseDelta;
        return true;
    }
    else
    {
        return false;
    }
}

void OpenXrTracker::ModifyFilterStrength(bool trans, bool increase)
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

bool OpenXrTracker::GetPose(XrPosef& trackerPose, XrTime frameTime) const
{
    // Query the latest tracker pose.
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
    {
        XrActiveActionSet activeActionSets;
        activeActionSets.actionSet = m_ActionSet;
        activeActionSets.subactionPath = XR_NULL_PATH;

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr};
        syncInfo.activeActionSets = &activeActionSets;
        syncInfo.countActiveActionSets = 1;

        TraceLoggingWrite(g_traceProvider, "GetPose", TLPArg(m_ActionSet, "xrSyncActions"), TLArg(frameTime, "Time"));
        CHECK_XRCMD(GetInstance()->xrSyncActions(m_Session, &syncInfo));
    }
    {
        XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
        XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
        getActionStateInfo.action = m_TrackerPoseAction;

        TraceLoggingWrite(g_traceProvider,
                          "GetPose",
                          TLPArg(m_TrackerPoseAction, "xrGetActionStatePose"),
                          TLArg(frameTime, "Time"));
        CHECK_XRCMD(GetInstance()->xrGetActionStatePose(m_Session, &getActionStateInfo, &actionStatePose));

        if (!actionStatePose.isActive)
        {
            ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active\n", __FUNCTION__);
            return false;
        }
    }

    CHECK_XRCMD(GetInstance()->xrLocateSpace(m_TrackerSpace, m_ReferenceSpace, frameTime, &location));

    if (!Pose::IsPoseValid(location.locationFlags))
    {
        ErrorLog("%s: unable to determine tracker pose - XrSpaceLocation not valid\n", __FUNCTION__);
        return false;
    }
    TraceLoggingWrite(g_traceProvider,
                      "GetPose",
                      TLArg(xr::ToString(location.pose).c_str(), "Location"),
                      TLArg(frameTime, "Time"));
    trackerPose = location.pose;

    return true;
}