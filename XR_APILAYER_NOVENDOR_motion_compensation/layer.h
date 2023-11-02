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
    // The handle of the dll
    extern HMODULE dllModule;

    // The path where the DLL is loaded from (eg: to load data files).
    extern std::filesystem::path dllHome;
    extern std::filesystem::path localAppData;

    const std::string LayerPrettyName = "OpenXR-MotionCompensation";
    const std::string LayerName = "XR_APILAYER_NOVENDOR_motion_compensation";
    const std::string VersionString = std::string(VERSION_STRING) + " (" + VERSION_NUMBER + ")";

    class OpenXrLayer : public openxr_api_layer::OpenXrApi
    {
      public:
        OpenXrLayer() = default;

        virtual ~OpenXrLayer() override;
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
        XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* frameBeginInfo)override;
        XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frameEndInfo) override;
        bool GetStageToLocalSpace(XrTime time, XrPosef& pose);
        void RequestCurrentInteractionProfile();

        XrActionSet m_ActionSet{XR_NULL_HANDLE};
        XrAction m_PoseAction{XR_NULL_HANDLE};
        XrAction m_MoveAction{XR_NULL_HANDLE};
        XrAction m_PositionAction{XR_NULL_HANDLE};
        XrAction m_HapticAction{XR_NULL_HANDLE};
        XrSpace m_TrackerSpace{XR_NULL_HANDLE};
        XrSpace m_ViewSpace{XR_NULL_HANDLE};
        XrSpace m_ReferenceSpace{XR_NULL_HANDLE};
        XrSpace m_StageSpace{XR_NULL_HANDLE};
        XrSpace m_ReferenceStageSpace{XR_NULL_HANDLE};

      private:
        [[nodiscard]] bool isSystemHandled(XrSystemId systemId) const;
        bool isSessionHandled(XrSession session) const;
        bool isViewSpace(XrSpace space) const;
        bool isActionSpace(XrSpace space) const;
        bool isCompensationRequired(const XrActionSpaceCreateInfo* createInfo) const;
        [[nodiscard]] uint32_t GetNumViews() const;
        bool CreateTrackerActions(const std::string& caller);
        void DestroyTrackerActions(const std::string& caller);
        bool AttachActionSet(const std::string& caller);
        void SuggestInteractionProfiles(const std::string& caller);
        bool LazyInit(XrTime time);

        static std::string getXrPath(XrPath path);

        bool TestRotation(XrPosef* pose, XrTime time, bool reverse) const;

        XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
        XrSession m_Session{XR_NULL_HANDLE};
        std::string m_RuntimeName;
        bool m_Enabled{false};
        bool m_PhysicalEnabled{false};
        bool m_OverlayEnabled{false};
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
        bool m_VarjoPollWorkaround{false};
        std::string m_Application;
        std::string m_SubActionPath;
        XrPath m_XrSubActionPath{XR_NULL_PATH};
        std::set<XrSpace> m_ViewSpaces{};
        std::set<XrSpace> m_ActionSpaces{};
        std::vector<XrView> m_EyeOffsets{};
        XrViewConfigurationType m_ViewConfigType{XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM};
        Tracker::TrackerBase* m_Tracker{nullptr};
        Tracker::ViveTrackerInfo m_ViveTracker;
        Input::ButtonPath m_ButtonPath;
        utility::Cache<XrPosef> m_PoseCache{xr::math::Pose::Identity()};
        utility::Cache<std::vector<XrPosef>> m_EyeCache{std::vector<XrPosef>{xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity(),
                                                                             xr::math::Pose::Identity()}};
        std::mutex m_FrameLock;
        std::unique_ptr<graphics::Overlay> m_Overlay{};
        std::shared_ptr<Input::InputHandler> m_Input{};
        std::unique_ptr<utility::AutoActivator> m_AutoActivator{};

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

        friend class Input::InputHandler;
    };

    // Singleton accessor.
    OpenXrApi* GetInstance();

    // A function to reset (delete) the singleton.
    void ResetInstance();

} // namespace openxr_api_layer
