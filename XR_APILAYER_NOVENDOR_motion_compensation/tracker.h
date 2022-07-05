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
    bool getPoseDelta(XrPosef& trackerPose, XrTime frameTime) const;

    bool m_Initialized{false};
    XrActionSet m_ActionSet{XR_NULL_HANDLE};
    XrAction m_TrackerPoseAction{XR_NULL_HANDLE};
    // TODO: neceesary to store bindings?
    std::map<std::string, std::pair<XrInteractionProfileSuggestedBinding, bool>> m_Bindings{};

  private:
    bool getPose(XrPosef& trackerPose, XrTime frameTime) const;

    XrSession m_Session{XR_NULL_HANDLE};
    motion_compensation_layer::OpenXrApi* m_Api;
    XrSpace m_ViewSpace{XR_NULL_HANDLE};
    XrSpace m_TrackerSpace{XR_NULL_HANDLE};
    XrPosef m_ReferencePoseInverse{xr::math::Pose::Identity()};
    // TODO: neceessary to create bindings and paths on the heap?
    std::vector<XrActionSuggestedBinding*> m_createdBindings;
    std::vector<XrPath*> m_createdPaths;
};