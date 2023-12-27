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

#include "input.h"
#include "framework/dispatch.gen.h"
#include "tracker.h"
#include "overlay.h"

namespace openxr_api_layer
{
    // Our API layer implement these extensions, and their specified version.
    inline const std::vector<std::pair<std::string, uint32_t>> advertisedExtensions = {};

    // Initialize these vectors with arrays of extensions to block and implicitly request for the instance.
    const std::vector<std::string> blockedExtensions = {};
    inline std::vector<std::string> implicitExtensions = {};

    // The handle of the dll
    extern HMODULE dllModule;

    // The path where the DLL is loaded from (eg: to load data files).
    extern std::filesystem::path dllHome;
    extern std::filesystem::path localAppData;

    const std::string LayerPrettyName = "OpenXR-MotionCompensation";
    const std::string LayerName = "XR_APILAYER_NOVENDOR_motion_compensation";
    const std::string VersionString = std::string(VERSION_STRING) + " (" + VERSION_NUMBER + ")";

    class OpenXrLayer final : public openxr_api_layer::OpenXrApi
    {
      public:
        OpenXrLayer() = default;

        XrResult xrDestroyInstance(XrInstance instance) override;
        XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo) override;
        XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId) override;
        XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData) override;
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
        XrResult xrAcquireSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageAcquireInfo* acquireInfo,
                                         uint32_t* index) override;
        XrResult xrReleaseSwapchainImage(XrSwapchain swapchain,
                                         const XrSwapchainImageReleaseInfo* releaseInfo) override;
        XrResult xrGetCurrentInteractionProfile(XrSession session,
                                                XrPath topLevelUserPath,
                                                XrInteractionProfileState* interactionProfile) override;
        XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo) override;
        XrResult
        xrSuggestInteractionProfileBindings(XrInstance instance,
                                            const XrInteractionProfileSuggestedBinding* suggestedBindings) override;
        XrResult xrCreateReferenceSpace(XrSession session,
                                        const XrReferenceSpaceCreateInfo* createInfo,
                                        XrSpace* space) override;
        XrResult xrCreateActionSpace(XrSession session,
                                     const XrActionSpaceCreateInfo* createInfo,
                                     XrSpace* space) override;
        XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location) override;
        XrResult xrLocateViews(XrSession session,
                               const XrViewLocateInfo* viewLocateInfo,
                               XrViewState* viewState,
                               uint32_t viewCapacityInput,
                               uint32_t* viewCountOutput,
                               XrView* views) override;
        XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo) override;
        XrResult xrWaitFrame(XrSession session,
                             const XrFrameWaitInfo* frameWaitInfo,
                             XrFrameState* frameState) override;
        XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo) override;
        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override;
        
        void SetForwardRotation(const XrPosef& pose) const;
        bool GetRefToStage(XrSpace space, XrPosef* refToStage, XrPosef* stageToRef);
        std::shared_ptr<graphics::ICompositionFrameworkFactory> GetCompositionFactory();

        XrActionSet m_ActionSet{XR_NULL_HANDLE};
        XrAction m_PoseAction{XR_NULL_HANDLE};
        XrAction m_MoveAction{XR_NULL_HANDLE};
        XrAction m_PositionAction{XR_NULL_HANDLE};
        XrAction m_HapticAction{XR_NULL_HANDLE};
        XrSpace m_TrackerSpace{XR_NULL_HANDLE};
        XrSpace m_ViewSpace{XR_NULL_HANDLE};
        XrSpace m_StageSpace{XR_NULL_HANDLE};

      private:
        [[nodiscard]] bool isSystemHandled(XrSystemId systemId) const;
        bool isSessionHandled(XrSession session) const;
        bool isViewSpace(XrSpace space) const;
        bool isActionSpace(XrSpace space) const;
        [[nodiscard]] uint32_t GetNumViews() const;
        void CreateStageSpace();
        void CreateViewSpace();
        void AddStaticRefSpace(XrSpace space);
        std::optional<XrPosef> LocateRefSpace(XrSpace space);
        bool CreateTrackerActions(const std::string& caller);
        void DestroyTrackerActions();
        bool AttachActionSet(const std::string& caller);
        void SuggestInteractionProfiles(const std::string& caller);
        bool LazyInit(XrTime time);
        void LogCurrentInteractionProfile();
        bool ToggleModifierActive();

        static std::string getXrPath(XrPath path);

        bool TestRotation(XrPosef* pose, XrTime time, bool reverse) const;

        XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
        XrSession m_Session{XR_NULL_HANDLE};
        std::string m_RuntimeName;
        bool m_Enabled{false};
        bool m_PhysicalEnabled{false};
        bool m_VirtualTrackerUsed{false};
        bool m_CompensateControllers{false};
        bool m_SuppressInteraction{false};
        bool m_ActionsCreated{false};
        bool m_ActionSpaceCreated{false};
        bool m_ActionSetAttached{false};
        bool m_InteractionProfileSuggested{false};
        bool m_SimpleProfileSuggested{false};
        bool m_Initialized{true};
        bool m_Activated{false};
        bool m_UseEyeCache{false};
        bool m_ModifierActive{false};
        bool m_LegacyMode{false};
        bool m_VarjoPollWorkaround{false};
        XrTime m_LastFrameTime{0};
        XrTime m_UpdateRefSpaceTime{0};
        std::set<XrSpace> m_StaticRefSpaces{};
        std::map<XrSpace, std::pair<XrPosef, XrPosef>> m_RefToStageMap{};
        std::unique_ptr<XrPosef> m_EyeToHmd{};
        std::string m_Application;
        std::string m_SubActionPath;
        XrPath m_XrSubActionPath{XR_NULL_PATH};
        std::set<XrSpace> m_ViewSpaces{};
        std::set<XrSpace> m_ActionSpaces{};
        std::vector<XrView> m_EyeOffsets{};
        XrViewConfigurationType m_ViewConfigType{XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM};
        Tracker::ViveTrackerInfo m_ViveTracker;
        Input::ButtonPath m_ButtonPath;
        utility::Cache<XrPosef> m_DeltaCache{"delta", xr::math::Pose::Identity()};
        utility::Cache<std::vector<XrPosef>> m_EyeCache{"eyes",
                                                        std::vector<XrPosef>{xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity()}};
        std::mutex m_FrameLock;
        std::unique_ptr<Tracker::TrackerBase> m_Tracker{};
        std::unique_ptr<graphics::Overlay> m_Overlay{};
        std::shared_ptr<Input::InputHandler> m_Input{};
        std::unique_ptr<utility::AutoActivator> m_AutoActivator{};
        std::unique_ptr<Modifier::HmdModifier> m_HmdModifier{};
        std::shared_ptr<graphics::ICompositionFrameworkFactory> m_CompositionFrameworkFactory{};

        // connection recovery
        XrTime m_RecoveryWait{3000000000}; // 3 sec default timeout
        XrTime m_RecoveryStart{0};

        // debugging
        bool m_TestRotation{false};
        XrTime m_TestRotStart{0};

        friend class Input::InputHandler;
    };

    // Singleton accessor.
    OpenXrApi* GetInstance();

} // namespace openxr_api_layer
