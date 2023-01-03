// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri, Sebastian Veith
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "framework/dispatch.gen.h"
#include "tracker.h"
#include "overlay.h"

namespace motion_compensation_layer
{
    // The handle of the dll
    extern HMODULE dllModule;

    // The path where the DLL is loaded from (eg: to load data files).
    extern std::filesystem::path dllHome;
    extern std::filesystem::path localAppData;

    const std::string LayerPrettyName = "OpenXR-MotionCompensation";
    const std::string LayerName = "XR_APILAYER_NOVENDOR_motion_compensation";
    const std::string VersionString = std::string(VERSION_STRING) + " (" + VERSION_NUMBER + ")";

    class OpenXrLayer : public motion_compensation_layer::OpenXrApi
    {
      public:
        OpenXrLayer() = default;

        virtual ~OpenXrLayer();
        XrResult xrDestroyInstance(XrInstance instance) override;

        XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo) override;
        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override;
        XrResult xrCreateSession(XrInstance instance,
                                 const XrSessionCreateInfo* createInfo,
                                 XrSession* session) override;
        XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo) override;
        XrResult xrEndSession(XrSession session) override;
        XrResult xrDestroySession(XrSession session) override;
        XrResult xrCreateSwapchain(XrSession session,
                                   const XrSwapchainCreateInfo* createInfo,
                                   XrSwapchain* swapchain) override;
        XrResult xrDestroySwapchain(XrSwapchain swapchain) override;
        XrResult xrWaitSwapchainImage(XrSwapchain swapchain,
                                                   const XrSwapchainImageWaitInfo* waitInfo) override;
        XrResult xrAcquireSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageAcquireInfo* acquireInfo,
                                         uint32_t* index) override;
        XrResult xrReleaseSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageReleaseInfo* releaseInfo) override;
        XrResult xrGetCurrentInteractionProfile(XrSession session,
                                                XrPath topLevelUserPath,
                                                XrInteractionProfileState* interactionProfile);
        XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo) override;
        XrResult
        xrSuggestInteractionProfileBindings(XrInstance instance,
                                            const XrInteractionProfileSuggestedBinding* suggestedBindings) override;
        XrResult xrCreateReferenceSpace(XrSession session,
                                        const XrReferenceSpaceCreateInfo* createInfo,
                                        XrSpace* space) override;
        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) override;
        XrResult xrLocateViews(XrSession session,
                               const XrViewLocateInfo* viewLocateInfo,
                               XrViewState* viewState,
                               uint32_t viewCapacityInput,
                               uint32_t* viewCountOutput,
                               XrView* views) override;
        XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) override;
        XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo);
        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override;
        bool GetStageToLocalSpace(XrTime time, XrPosef& location);

        XrActionSet m_ActionSet{XR_NULL_HANDLE};
        XrAction m_TrackerPoseAction{XR_NULL_HANDLE};
        XrSpace m_TrackerSpace{XR_NULL_HANDLE};
        XrSpace m_ViewSpace{XR_NULL_HANDLE};
        XrSpace m_ReferenceSpace{XR_NULL_HANDLE};
        XrSpace m_StageSpace{XR_NULL_HANDLE};

      private:
        enum class Direction
        {
            Fwd,
            Back,
            Up,
            Down,
            Left,
            Right,
            RotRight,
            RotLeft
        };

        bool isSystemHandled(XrSystemId systemId) const;
        bool isSessionHandled(XrSession session) const;
        bool isViewSpace(XrSpace space) const;
        uint32_t GetNumViews();
        void CreateTrackerAction();
        void CreateTrackerActionSpace();
        void ToggleActive(XrTime time);
        void Recalibrate(XrTime time);
        void ToggleOverlay();
        void ToggleCache();
        void ChangeOffset(Direction dir);
        void ReloadConfig();
        void SaveConfig(XrTime time, bool forApp);
        void ToggleCorDebug(XrTime time);
        bool LazyInit(XrTime time);
        void HandleKeyboardInput(XrTime time);

        static std::string getXrPath(XrPath path);

        bool TestRotation(XrPosef* pose, XrTime time, bool reverse);

        XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
        XrSession m_Session{XR_NULL_HANDLE};
        std::string m_RuntimeName;
        bool m_Enabled{false};
        bool m_PhysicalEnabled{false};
        bool m_ActionSetAttached{false};
        bool m_InteractionProfileSuggested{false};
        bool m_Initialized{true};
        bool m_Activated{false};
        bool m_UseEyeCache{false};
        std::string m_Application;
        std::set<XrSpace> m_ViewSpaces{};
        std::vector<XrView> m_EyeOffsets{};
        XrViewConfigurationType m_ViewConfigType{XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM};
        Tracker::TrackerBase* m_Tracker{nullptr};
        Tracker::ViveTrackerInfo m_ViveTracker;
        utility::Cache<XrPosef> m_PoseCache{xr::math::Pose::Identity()};
        utility::Cache<std::vector<XrPosef>> m_EyeCache{std::vector<XrPosef>{xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity()}};
        utility::KeyboardInput m_Input;
        std::unique_ptr<graphics::Overlay> m_Overlay;

        // connection recovery
        XrTime m_RecoveryWait{3000000000}; // 3 sec default timeout
        XrTime m_RecoveryStart{0};

        // recentering of in-game view
        XrTime m_LastFrameTime{0};
        bool m_LocalRefSpaceCreated{false};
        bool m_RecenterInProgress{false};
       
        // debugging
        bool m_TestRotation{false};
        XrTime m_TestRotStart{0};
    };

    // Singleton accessor.
    OpenXrApi* GetInstance();

    // A function to reset (delete) the singleton.
    void ResetInstance();

} // namespace motion_compensation_layer
