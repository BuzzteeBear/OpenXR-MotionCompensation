#include "pch.h"

#include "tracker.h"
#include "layer.h"
#include <log.h>
#include <util.h>

using namespace motion_compensation_layer;
using namespace motion_compensation_layer::log;
using namespace xr::math;

OpenXrTracker::OpenXrTracker(OpenXrApi* api)
{
    m_Api = api;
    // Create the resources for the tracker space.
    {
        XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO, nullptr};
        strcpy_s(actionSetCreateInfo.actionSetName, "general_tracker");
        strcpy_s(actionSetCreateInfo.localizedActionSetName, "General Tracker");
        actionSetCreateInfo.priority = 0;
        CHECK_XRCMD(m_Api->xrCreateActionSet(m_Api->GetXrInstance(), &actionSetCreateInfo, &m_ActionSet));
    }
    {
        XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO, nullptr};
        strcpy_s(actionCreateInfo.actionName, "eye_tracker");
        strcpy_s(actionCreateInfo.localizedActionName, "Eye Tracker");
        actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
        actionCreateInfo.countSubactionPaths = 0;
        CHECK_XRCMD(m_Api->xrCreateAction(m_ActionSet, &actionCreateInfo, &m_TrackerPoseAction));
    }
}

OpenXrTracker::~OpenXrTracker(){}

void OpenXrTracker::beginSession(XrSession session)
{
    m_Session = session;

    // Create a reference space.
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
    CHECK_XRCMD(m_Api->xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_ReferenceSpace));

    // create action space
    XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr};
    actionSpaceCreateInfo.action = m_TrackerPoseAction;
    actionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
    actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
    CHECK_XRCMD(m_Api->xrCreateActionSpace(m_Session, &actionSpaceCreateInfo, &m_TrackerSpace));
    
    // TODO: attach actionset if application doesn't call xrAttachSessionActionSets but avoid to attach twice
    /*
    if (!m_IsActionSetAttached)
    {
        XrSessionActionSetsAttachInfo actionSetAttachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
                                                      nullptr,
                                                      1,
                                                      &m_ActionSet}; 
         CHECK_XRCMD(m_Api->OpenXrApi::xrAttachSessionActionSets(session, &actionSetAttachInfo));
    }
    */
    if (!m_IsBindingSuggested)
    {
        // suggest left controller if app has not suggested controller binding for left controller pose
        XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING, nullptr};
        CHECK_XRCMD(m_Api->xrStringToPath(m_Api->GetXrInstance(),
                                          "/interaction_profiles/khr/simple_controller",
                                          &suggestedBindings.interactionProfile));
        XrActionSuggestedBinding binding;
        binding.action = m_TrackerPoseAction;
        CHECK_XRCMD(m_Api->xrStringToPath(m_Api->GetXrInstance(), "/user/hand/left/input/grip/pose", &binding.binding));

        suggestedBindings.suggestedBindings = &binding;
        suggestedBindings.countSuggestedBindings = 1;
        CHECK_XRCMD(m_Api->OpenXrApi::xrSuggestInteractionProfileBindings(m_Api->GetXrInstance(), &suggestedBindings));
    }
    
}

void OpenXrTracker::endSession()
{
    m_Session = XR_NULL_HANDLE;

    if (m_ActionSet != XR_NULL_HANDLE)
    {
        m_Api->xrDestroyActionSet(m_ActionSet);
        m_ActionSet = XR_NULL_HANDLE;
    }
    if (m_TrackerSpace != XR_NULL_HANDLE)
    {
        m_Api->xrDestroySpace(m_TrackerSpace);
        m_TrackerSpace = XR_NULL_HANDLE;
    }
    if (m_TrackerPoseAction != XR_NULL_HANDLE)
    {
        m_Api->xrDestroyAction(m_TrackerPoseAction);
        m_TrackerPoseAction = XR_NULL_HANDLE;
    }
}
bool OpenXrTracker::setReferencePose(XrTime frameTime)
{
    m_IsInitialized = true;
    XrPosef curPose;
    if (getPose(curPose, frameTime))
    {
        m_ReferencePose = curPose;
        m_IsInitialized = true;
        return true;
    }
    else
    {
        return false;
    }
}

bool OpenXrTracker::getPoseDelta(XrPosef& trackerPose, XrTime frameTime)
{
    // pose already calulated for requested time or unable to calculate 
    if (frameTime == m_LastPoseTime
        || !m_IsActionSetAttached
        || !m_IsBindingSuggested)
    {
        // already calulated for requested time;
        trackerPose = m_LastPoseDelta;
        return true;
    }
    XrPosef curPose;
    if (getPose(curPose, frameTime))
    {
        // TODO: move initialization to appropriate location
        if (!m_IsInitialized)
        {
            m_ReferencePose = curPose;
            m_IsInitialized = true;
        }    
        trackerPose = Pose::Multiply(m_ReferencePose,Pose::Invert(curPose));
        m_LastPoseTime = frameTime;
        m_LastPoseDelta = trackerPose;
        return true;
    }
    else
    {
        return false;
    }
}

bool OpenXrTracker::getPose(XrPosef& trackerPose, XrTime frameTime) const
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
        CHECK_XRCMD(m_Api->xrSyncActions(m_Session, &syncInfo));
    }
    {
        XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
        XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
        getActionStateInfo.action = m_TrackerPoseAction;
        CHECK_XRCMD(m_Api->xrGetActionStatePose(m_Session, &getActionStateInfo, &actionStatePose));

        if (!actionStatePose.isActive)
        {
            return false;
        }
    }

    CHECK_XRCMD(m_Api->xrLocateSpace(m_TrackerSpace, m_ReferenceSpace, frameTime, &location));

    if (!Pose::IsPoseValid(location.locationFlags))
    {
        return false;
    }

    trackerPose = location.pose;

    return true;
}