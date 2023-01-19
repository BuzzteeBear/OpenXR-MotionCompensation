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
#include "interfaces.h"

namespace graphics
{
    // Colors
    constexpr XrVector3f Red{1.f, 0.f, 0.f};
    constexpr XrVector3f DarkRed{0.25f, 0.f, 0.f};
    constexpr XrVector3f LightRed{1.f, 0.15f, 0.15f};
    constexpr XrVector3f Green{0.f, 1.f, 0.f};
    constexpr XrVector3f DarkGreen{0.f, 0.25f, 0.f};
    constexpr XrVector3f LightGreen{0.15f, 1.f, 0.15f};
    constexpr XrVector3f Blue{0.f, 0.f, 1.f};
    constexpr XrVector3f DarkBlue{0.f, 0.f, 0.25f};
    constexpr XrVector3f LightBlue{0.15f, 0.15f, 1.f};
    constexpr XrVector3f Yellow{1.f, 1.f, 0.f};
    constexpr XrVector3f DarkYellow{0.25f, 0.25f, 0.f};
    constexpr XrVector3f LightYellow{1.f, 1.f, 0.15f};
    constexpr XrVector3f Cyan{0.f, 1.f, 1.f};
    constexpr XrVector3f DarkCyan{0.f, 0.25f, 0.25f};
    constexpr XrVector3f LightCyan{0.15f, 1.f, 1.f};
    constexpr XrVector3f Magenta{1.f, 0.f, 1.f};
    constexpr XrVector3f DarkMagenta{0.25f, 0.f, 0.25f};
    constexpr XrVector3f LightMagenta{1.f, 0.15f, 1.f};

    struct SwapchainImages
    {
        std::vector<std::shared_ptr<graphics::ITexture>> chain;
    };

    struct SwapchainState
    {
        std::vector<SwapchainImages> images;
        uint32_t acquiredImageIndex{0};
        bool delayedRelease{false};
    };

    class Overlay
    {
      public:
        void CreateSession(const XrSessionCreateInfo* createInfo, XrSession* session, const std::string& runtimeName);
        void DestroySession();
        void CreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);
        void DestroySwapchain(const XrSwapchain swapchain);
        XrResult AcquireSwapchainImage(XrSwapchain swapchain,
                                       const XrSwapchainImageAcquireInfo* acquireInfo,
                                       uint32_t* index);
        XrResult ReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);
        bool ToggleOverlay();
        void BeginFrameBefore();
        void BeginFrameAfter();
        void DrawOverlay(const XrFrameEndInfo* chainFrameEndInfo,
                         const XrPosef& referenceTrackerPose,
                         const XrPosef& reversedManipulation,
                         bool mcActivated);
        
        bool m_Initialized{false};

      private:
        static std::vector<SimpleMeshVertex> CreateMarker(bool rgb);
        static std::vector<SimpleMeshVertex> CreateConeMesh(const XrVector3f& top,
                                                            const XrVector3f& side,
                                                            const XrVector3f& offset,
                                                            const XrVector3f& topColor,
                                                            const XrVector3f& sideColor,
                                                            const XrVector3f& bottomColor);
        static std::vector<unsigned short> CreateIndices(size_t amount);

        bool m_OverlayActive{false};
        std::shared_ptr<graphics::IDevice> m_GraphicsDevice;
        std::map<XrSwapchain, graphics::SwapchainState> m_Swapchains;
        std::unordered_map<XrSwapchain, std::shared_ptr<ITexture>> m_OwnDepthBuffers;
        std::shared_ptr<graphics::ISimpleMesh> m_MeshRGB, m_MeshCMY;
    };
} // namespace graphics