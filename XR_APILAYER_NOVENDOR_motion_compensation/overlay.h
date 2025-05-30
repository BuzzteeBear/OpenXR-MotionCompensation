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
#include "graphics.h"

namespace openxr_api_layer::graphics
{
    // Colors
    constexpr XrVector3f Red{1, 0, 0};
    constexpr XrVector3f DarkRed{0.25f, 0, 0};
    constexpr XrVector3f LightRed{1, 0.15f, 0.15f};
    constexpr XrVector3f Green{0, 1, 0};
    constexpr XrVector3f DarkGreen{0, 0.25f, 0};
    constexpr XrVector3f LightGreen{0.15f, 1, 0.15f};
    constexpr XrVector3f Blue{0, 0, 1};
    constexpr XrVector3f DarkBlue{0, 0, 0.25f};
    constexpr XrVector3f LightBlue{0.15f, 0.15f, 1};
    constexpr XrVector3f Yellow{1, 1, 0};
    constexpr XrVector3f DarkYellow{0.25f, 0.25f, 0};
    constexpr XrVector3f LightYellow{1, 1, 0.15f};
    constexpr XrVector3f Cyan{0, 1, 1};
    constexpr XrVector3f DarkCyan{0, 0.25f, 0.25f};
    constexpr XrVector3f LightCyan{0.15f, 1, 1};
    constexpr XrVector3f Magenta{1, 0, 1};
    constexpr XrVector3f DarkMagenta{0.25f, 0, 0.25f};
    constexpr XrVector3f LightMagenta{1, 0.15f, 1};
    constexpr XrVector3f Grey{0.25f, 0.25f, 0.25f};
    constexpr XrVector3f DarkGrey{0.0f, 0.0f, 0.0f};
    constexpr XrVector3f LightGrey{0.75f, 0.75f, 0.75f};

    struct SwapchainImages
    {
        std::vector<std::shared_ptr<IGraphicsTexture>> chain{};
    };

    class Overlay
    {
      public:
        void DestroySession(XrSession session);
        // I'd rather have that swapchain tracking stuff in composition/device space. But to not interfere when overlay
        // isn't even used, decided to go for lazy init of composition framework, which unfortunately is too late to
        // capture swapchain creation
        void CreateSwapchain(XrSwapchain swapchain, const XrSwapchainCreateInfo* createInfo);
        void DestroySwapchain(XrSwapchain swapchain);
        XrResult AcquireSwapchainImage(XrSwapchain swapchain,
                                       const XrSwapchainImageAcquireInfo* acquireInfo,
                                       uint32_t* index);
        XrResult ReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);
        void ReleaseAllSwapChainImages();
        void ResetMarker();
        void ResetCrosshair();
        bool ToggleOverlay();
        bool TogglePassthrough();
        bool ToggleCrosshair();
        void DrawMarkers(const XrPosef& referencePose,
                         const XrPosef& delta,
                         bool calibrated,
                         bool compensated,
                         XrSession session,
                         XrFrameEndInfo* chainFrameEndInfo,
                         OpenXrLayer* openXrLayer);
        void DrawCrosshair(XrSession session, XrFrameEndInfo* chainFrameEndInfo, OpenXrLayer* openXrLayer);

        bool m_D3D12inUse{false}, m_SessionVisible{false}, m_MarkersInitialized{true}, m_MarkersActive{false},
            m_CrosshairInitialized{true}, m_CrosshairActive{false};

      private:
        bool InitializeTextures(uint32_t eye, XrSwapchain swapchain, const ICompositionFramework* composition);
        bool InitializeCrosshair(ICompositionFramework* composition, XrSpace viewSpace);
        void RenderMarkers(const XrCompositionLayerProjectionView& view,
                           uint32_t eye,
                           const XrPosef& refPose,
                           const XrPosef& trackerPose,
                           bool compensated,
                           bool calibrated,
                           std::vector<std::pair<XrPosef, int>> estimatorSamples,
                           std::vector<std::tuple<int, XrPosef, float>> estimatorAxes,
                           ICompositionFramework* composition);
        static std::vector<SimpleMeshVertex> CreateMarker(bool reference, bool avoidMagenta);
        static std::vector<SimpleMeshVertex> CreateMarkerMesh(const XrVector3f& top,
                                                              const XrVector3f& innerMiddle,
                                                              const XrVector3f& outerMiddle,
                                                              const XrVector3f& bottom,
                                                              const XrVector3f& darkColor,
                                                              const XrVector3f& pureColor,
                                                              const XrVector3f& lightColor);

        static std::vector<SimpleMeshVertex> CreateDodecahedron(const XrVector3f& color);
        static std::vector<SimpleMeshVertex> CreateRing(const XrVector3f& color);
        static std::vector<SimpleMeshVertex> CreateAxis(const XrVector3f& darkColor,
                                                        const XrVector3f& pureColor,
                                                        const XrVector3f& lightColor);  

        bool m_PassthroughActive{false}, m_NoProjectionLayer{false}, m_CrosshairLockToHorizon{false};
        XrVector3f m_MarkerSize{0.1f, 0.1f, 0.1f};
        std::shared_ptr<ISimpleMesh> m_MeshRGB{}, m_MeshCMY{}, m_MeshCGY{}, m_MeshPoint{}, m_MeshPointR{},
            m_MeshPointG{}, m_MeshPointB{}, m_MeshPointC{}, m_MeshPointY{}, m_MeshPointM{}, m_MeshRingR{},
            m_MeshRingG{}, m_MeshRingB{}, m_MeshAxisR{}, m_MeshAxisG{}, m_MeshAxisB{};
        std::map<XrSwapchain, SwapchainState> m_Swapchains{};
        std::vector<std::pair<std::shared_ptr<IGraphicsTexture>, std::shared_ptr<IGraphicsTexture>>> m_Textures{};
        float m_CrosshairDistance{-1};
        std::shared_ptr<ISwapchain> m_CrosshairSwapchain;
        XrCompositionLayerQuad m_CrosshairLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        std::shared_ptr<std::vector<const XrCompositionLayerBaseHeader*>> m_LayersForSubmission{};
        std::mutex m_DrawMutex;
        std::set<XrSession> m_InitializedSessions{};
    };
} // namespace openxr_api_layer::graphics