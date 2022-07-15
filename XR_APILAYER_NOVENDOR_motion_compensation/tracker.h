#pragma once
#include "pch.h"
#include "utility.h"

class OpenXrTracker
{
  public:
    OpenXrTracker();
    ~OpenXrTracker();
    void Init();
    void beginSession(XrSession session);
    void endSession();
    bool ResetReferencePose(XrTime frameTime);
    bool GetPoseDelta(XrPosef& poseDalta, XrTime frameTime);

    bool m_IsInitialized{false};
    bool m_IsBindingSuggested{false};
    bool m_IsActionSetAttached{false};
    XrActionSet m_ActionSet{XR_NULL_HANDLE};
    XrAction m_TrackerPoseAction{XR_NULL_HANDLE};
    XrSpace m_TrackerSpace{XR_NULL_HANDLE};

  private:
    bool getPose(XrPosef& trackerPose, XrTime frameTime) const;

    XrSession m_Session{XR_NULL_HANDLE};
    XrSpace m_ReferenceSpace{XR_NULL_HANDLE};
    XrPosef m_ReferencePose{xr::math::Pose::Identity()};
    XrPosef m_LastPoseDelta{xr::math::Pose::Identity()};
    XrTime m_LastPoseTime{0};
    utilities::FilterBase<XrVector3f>* m_TransFilter = nullptr;
    utilities::FilterBase<XrQuaternionf>* m_RotFilter = nullptr;
};