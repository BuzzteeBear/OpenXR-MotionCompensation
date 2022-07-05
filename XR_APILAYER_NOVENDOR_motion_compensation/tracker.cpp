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
}

OpenXrTracker::~OpenXrTracker()
{
    for (auto binding : m_createdBindings)
    {
        delete binding;
    }
    for (auto path : m_createdPaths)
    {
        delete path;
    }
}

void OpenXrTracker::beginSession(XrSession session)
{
    // Create a reference space.
    {
        m_Session = session;
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
        CHECK_XRCMD(m_Api->OpenXrApi::xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_ViewSpace));
    }

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
    {
        XrInteractionProfileSuggestedBinding suggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING, nullptr};

        XrActionSuggestedBinding* binding = new XrActionSuggestedBinding;
        m_createdBindings.push_back(binding);
        binding->action = m_TrackerPoseAction;

        // use a Left simple controller to simulate the tracker.
        XrPath* bindingPath = new XrPath;
        m_createdPaths.push_back(bindingPath);
        CHECK_XRCMD(m_Api->xrStringToPath(m_Api->GetXrInstance(), "/user/hand/left/input/grip/pose", bindingPath));
        binding->binding = *bindingPath;

        const std::string interactionProfilePath = m_Api->IsExtensionActivated(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME)
                                                       ? "/interaction_profiles/microsoft/motion_controller"
                                                       : "/interaction_profiles/khr/simple_controller";

        CHECK_XRCMD(m_Api->xrStringToPath(m_Api->GetXrInstance(),
                                          interactionProfilePath.c_str(),
                                          &suggestedBinding.interactionProfile));

        suggestedBinding.suggestedBindings = binding;
        suggestedBinding.countSuggestedBindings = 1;
        CHECK_XRCMD(m_Api->xrSuggestInteractionProfileBindings(m_Api->GetXrInstance(), &suggestedBinding));

        m_Bindings[interactionProfilePath] = {suggestedBinding, false};
    }
    {
        XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr};
        actionSpaceCreateInfo.action = m_TrackerPoseAction;
        actionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
        actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
        CHECK_XRCMD(m_Api->xrCreateActionSpace(m_Session, &actionSpaceCreateInfo, &m_TrackerSpace));
    }
}

void OpenXrTracker::endSession()
{
    if (m_ActionSet != XR_NULL_HANDLE)
    {
        m_Api->xrDestroyActionSet(m_ActionSet);
        m_ActionSet = XR_NULL_HANDLE;
    }
    if (m_ViewSpace != XR_NULL_HANDLE)
    {
        m_Api->xrDestroySpace(m_ViewSpace);
        m_ViewSpace = XR_NULL_HANDLE;
    }

    m_Session = XR_NULL_HANDLE;

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
    m_Initialized = true;
    XrPosef curPose;
    if (getPose(curPose, frameTime))
    {
        m_ReferencePoseInverse = Pose::Invert(curPose);
        m_Initialized = true;
        return true;
    }
    else
    {
        return false;
    }
}

bool OpenXrTracker::getPoseDelta(XrPosef& trackerPose, XrTime frameTime) const
{
    XrPosef curPose;
    if (getPose(curPose, frameTime))
    {
        trackerPose = Pose::Multiply(curPose, m_ReferencePoseInverse);
        return true;
    }
    else
    {
        return false;
    }
}

bool OpenXrTracker::getPose(XrPosef& trackerPose, XrTime frameTime) const
{
    XrView eyeInViewSpace[2] = {{XR_TYPE_VIEW, nullptr}, {XR_TYPE_VIEW, nullptr}};
    {
        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO, nullptr};
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.space = m_ViewSpace;
        locateInfo.displayTime = frameTime;

        XrViewState state{XR_TYPE_VIEW_STATE, nullptr};
        uint32_t viewCountOutput;
        CHECK_HRCMD(
            m_Api->OpenXrApi::xrLocateViews(m_Session, &locateInfo, &state, 2, &viewCountOutput, eyeInViewSpace));

        if (!Pose::IsPoseValid(state.viewStateFlags))
        {
            return false;
        }
    }
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

        // TODO: investigate why action state pose is not active
        if (!actionStatePose.isActive)
        {
            return false;
        }
    }

    CHECK_XRCMD(m_Api->xrLocateSpace(m_TrackerSpace, m_ViewSpace, frameTime, &location));

    if (!Pose::IsPoseValid(location.locationFlags))
    {
        return false;
    }

    trackerPose = location.pose;

    return true;
}