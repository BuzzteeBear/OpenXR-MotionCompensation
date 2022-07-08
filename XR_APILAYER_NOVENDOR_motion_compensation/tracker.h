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

    bool m_Initialized{false};
    XrActionSet m_ActionSet{XR_NULL_HANDLE};
    XrAction m_TrackerPoseAction{XR_NULL_HANDLE};

  private:
    bool getPose(XrPosef& trackerPose, XrTime frameTime) const;

    XrSession m_Session{XR_NULL_HANDLE};
    motion_compensation_layer::OpenXrApi* m_Api;
    XrSpace m_ViewSpace{XR_NULL_HANDLE};
    XrSpace m_TrackerSpace{XR_NULL_HANDLE};
    XrPosef m_ReferencePoseInverse{xr::math::Pose::Identity()};
};