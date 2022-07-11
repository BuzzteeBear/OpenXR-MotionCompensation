#pragma once
#include "pch.h"
namespace motion_compensation_layer
{
    // forward declaration
    class OpenXrApi;
}

class OpenXrTracker
{
  public:
    OpenXrTracker(motion_compensation_layer::OpenXrApi* api);
    ~OpenXrTracker();

    void beginSession(XrSession session);
    void endSession();
    bool setReferencePose(XrTime frameTime);
    bool getPoseDelta(XrPosef& trackerPose, XrTime frameTime);
    bool getPose(XrPosef& trackerPose, XrTime frameTime) const;

    bool m_IsInitialized{false};
    bool m_IsBindingSuggested{false};
    bool m_IsActionSetAttached{false};
    XrActionSet m_ActionSet{XR_NULL_HANDLE};
    XrAction m_TrackerPoseAction{XR_NULL_HANDLE};
    XrSpace m_TrackerSpace{XR_NULL_HANDLE};

  private:

    XrSession m_Session{XR_NULL_HANDLE};
    motion_compensation_layer::OpenXrApi* m_Api;
    XrSpace m_ReferenceSpace{XR_NULL_HANDLE};
    XrPosef m_ReferencePose{xr::math::Pose::Identity()};
    XrPosef m_LastPoseDelta{xr::math::Pose::Identity()};
    XrTime m_LastPoseTime{0};
};