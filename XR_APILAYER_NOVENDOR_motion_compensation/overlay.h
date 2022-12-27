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
#include "pch.h"
#include "interfaces.h"

namespace graphics
{
    // Colors
    constexpr XrVector3f Red{1.f, 0.f, 0.f};
    constexpr XrVector3f LightRed{1.f, 0.25f, 0.25f};
    constexpr XrVector3f Green{0.f, 1.f, 0.f};
    constexpr XrVector3f LightGreen{0.25f, 1.f, 0.25f};
    constexpr XrVector3f Blue{0.f, 0.f, 1.f};
    constexpr XrVector3f LightBlue{0.25f, 0.25f, 1.f};
    constexpr XrVector3f Yellow{1.f, 1.f, 0.f};
    constexpr XrVector3f LightYellow{1.f, 1.f, 0.25f};
    constexpr XrVector3f Cyan{0.f, 1.f, 1.f};
    constexpr XrVector3f LightCyan{0.25f, 1.f, 1.f};
    constexpr XrVector3f Magenta{1.f, 0.f, 1.f};
    constexpr XrVector3f LightMagenta{1.f, 0.25f, 1.f};

    // Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
    constexpr XrVector3f LBB{-0.5f, -0.5f, -0.5f};
    constexpr XrVector3f LBF{-0.5f, -0.5f, 0.5f};
    constexpr XrVector3f LTB{-0.5f, 0.5f, -0.5f};
    constexpr XrVector3f LTF{-0.5f, 0.5f, 0.5f};
    constexpr XrVector3f RBB{0.5f, -0.5f, -0.5f};
    constexpr XrVector3f RBF{0.5f, -0.5f, 0.5f};
    constexpr XrVector3f RTB{0.5f, 0.5f, -0.5f};
    constexpr XrVector3f RTF{0.5f, 0.5f, 0.5f};
    constexpr XrVector3f LEFT{-4.f, 0.f, 0.f};
    constexpr XrVector3f RIGHT{4.f, 0.f, 0.f};
    
    // Vertices for pyramid top
    constexpr XrVector3f TOP{0.f, 4.f, 0.f};
    constexpr XrVector3f FRONT{0.f, 0.f, 4.f};
    constexpr XrVector3f BACK{0.f, 0.f, -4.f};

#define PYRAMID_BASE(V1, V2, V3, V4, COLOR) \
    {V1, COLOR}, {V2, COLOR}, {V3, COLOR}, {V4, COLOR}, {V1, COLOR}, {V3, COLOR},

#define PYRAMID_SIDE(B1, B2, TOP, COLOR_BASE, COLOR_TOP) \
    {B1, COLOR_BASE}, {B2, COLOR_BASE}, {TOP, COLOR_TOP},

#define PYRAMID(B1, B2, B3, B4, TOP, COLOR_BASE, COLOR_TOP)  \
    PYRAMID_BASE(B3, B2, B1, B4, COLOR_BASE)                 \
    PYRAMID_SIDE(B1, B2, TOP, COLOR_BASE, COLOR_TOP)         \
    PYRAMID_SIDE(B2, B3, TOP, COLOR_BASE, COLOR_TOP)         \
    PYRAMID_SIDE(B3, B4, TOP, COLOR_BASE, COLOR_TOP)         \
    PYRAMID_SIDE(B4, B1, TOP, COLOR_BASE, COLOR_TOP)     

    constexpr SimpleMeshVertex c_MarkerRGB[]{PYRAMID(LTF, RTF, RBF, LBF, FRONT, Green, LightGreen)
                                                 PYRAMID(LTB, LTF, LBF, LBB, LEFT, Red, LightRed)
                                                     PYRAMID(LTB, RTB, RTF, LTF, TOP, Blue, LightBlue)};
    constexpr SimpleMeshVertex c_MarkerCMY[]{PYRAMID(LTF, RTF, RBF, LBF, FRONT, Yellow, LightYellow)
                                                 PYRAMID(LTB, LTF, LBF, LBB, LEFT, Magenta, LightMagenta)
                                                     PYRAMID(LTB, RTB, RTF, LTF, TOP, Cyan, LightCyan)};

#undef PYRAMID_BASE
#undef PYRAMID_SIDE
#undef PYRAMID

    constexpr SimpleMeshVertex forwardAxis[]{{{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}}};

    constexpr unsigned short c_cubeIndices[] = {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, // Front
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, // Right
        36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53
    };

    template <typename T, size_t N>
    void copyFromArray(std::vector<T>& targetVector, const T (&sourceArray)[N])
    {
        targetVector.assign(sourceArray, sourceArray + N);
    }

    struct SwapchainImages
    {
        std::shared_ptr<graphics::ITexture> appTexture;
        std::shared_ptr<graphics::ITexture> runtimeTexture;
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
        ~Overlay();
        void CreateSession(const XrSessionCreateInfo* createInfo, XrSession* session, const std::string& runtimeName);        void DestroySession(XrSession session);
        void CreateSwapchain(XrSession session,
                             const XrSwapchainCreateInfo* chainCreateInfo,
                             const XrSwapchainCreateInfo* createInfo,
                             XrSwapchain* swapchain,
                             bool isDepth);
        void DestroySwapchain(XrSwapchain swapchain);
        XrResult AcquireSwapchainImage(XrSwapchain swapchain,
                                       const XrSwapchainImageAcquireInfo* acquireInfo,
                                       uint32_t* index);
        XrResult ReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);
        bool ToggleOverlay();
        void DrawOverlay(XrFrameEndInfo* chainFrameEndInfo,
                         const XrPosef& referenceTrackerPose,
                         const XrPosef& reversedManipulation,
                         bool mcActivated);
        
        bool m_Initialized{false};

      private:
        bool m_OverlayActive{false};
        std::shared_ptr<graphics::IDevice> m_GraphicsDevice;
        std::map<XrSwapchain, graphics::SwapchainState> m_Swapchains;
        std::unordered_map<XrSwapchain, std::shared_ptr<ITexture>> m_OwnDepthBuffers;
        std::shared_ptr<graphics::ISimpleMesh> m_MeshRGB, m_MeshCMY;
    };
} // namespace graphics