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


#include "pch.h"

#include "overlay.h"
#include "layer.h"
#include "feedback.h"
#include "graphics.h"
#include <log.h>

using namespace openxr_api_layer;
using namespace openxr_api_layer::log;

namespace openxr_api_layer::graphics
{
    void Overlay::Init(const XrInstanceCreateInfo& instanceInfo,
                       XrInstance instance,
                       PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr)
    {
        m_compositionFrameworkFactory =
            createCompositionFrameworkFactory(instanceInfo, instance, xrGetInstanceProcAddr, CompositionApi::D3D11);
        m_Initialized = true;
    }

    void Overlay::CreateSession(const XrSessionCreateInfo* createInfo, XrSession session)
    {
        std::unique_lock lock(m_DrawMutex);
        m_compositionFrameworkFactory->CreateSession(createInfo, session);
        if (const ICompositionFramework* composition = m_compositionFrameworkFactory->getCompositionFramework(session))
        {
            std::vector<SimpleMeshVertex> vertices = CreateMarker(true);
            std::vector<uint16_t> indices = CreateIndices(vertices.size());
            m_MeshRGB = composition->getCompositionDevice()->createSimpleMesh(vertices, indices, "RGB Mesh");
            vertices = CreateMarker(false);
            m_MeshCMY = composition->getCompositionDevice()->createSimpleMesh(vertices, indices, "CMY Mesh");
        }
        else
        {
            ErrorLog("%s: unable to retrieve composition framework", __FUNCTION__);
            m_Initialized = false;
        }
    }

    void Overlay::DestroySession(XrSession session)
    {
        std::unique_lock lock(m_DrawMutex);
        DeleteResources();
        m_MarkerSwapchains.clear();
        m_MarkerDepthTextures.clear();
        m_MeshRGB.reset();
        m_MeshCMY.reset();
        m_compositionFrameworkFactory->DestroySession(session);
    }

    void Overlay::SetMarkerSize()
    {
        float scaling{0.1f};
        GetConfig()->GetFloat(Cfg::MarkerSize, scaling);
        scaling /= 100.f;
        m_MarkerSize = {scaling, scaling, scaling};
    }

    bool Overlay::ToggleOverlay()
    {
        if (!m_Initialized)
        {
            m_OverlayActive = false;
            ErrorLog("graphical overlay is not properly initialized");
            GetAudioOut()->Execute(Feedback::Event::Error);
            return false;
        }
        m_OverlayActive = !m_OverlayActive;
        GetAudioOut()->Execute(m_OverlayActive ? Feedback::Event::OverlayOn : Feedback::Event::OverlayOff);
        return true;
    }

    void Overlay::DrawOverlay(XrSession session,
                              XrFrameEndInfo* chainFrameEndInfo,
                              const XrPosef& referenceTrackerPose,
                              const XrPosef& reversedManipulation,
                              bool mcActivated)
    {
        if (m_Initialized && m_OverlayActive)
        {
            std::unique_lock lock(m_DrawMutex);
            try
            {
                m_BaseLayerVector =
                    std::vector(chainFrameEndInfo->layers, chainFrameEndInfo->layers + chainFrameEndInfo->layerCount);
                XrCompositionLayerProjection const* lastProjectionLayer{};
                for (auto layer : m_BaseLayerVector)
                {
                    if (XR_TYPE_COMPOSITION_LAYER_PROJECTION == layer->type)
                    {
                        lastProjectionLayer = reinterpret_cast<const XrCompositionLayerProjection*>(layer);
                    }
                }
                if (lastProjectionLayer)
                {
                    if (ICompositionFramework* composition =
                            m_compositionFrameworkFactory->getCompositionFramework(session))
                    {
                        auto graphicsDevice = composition->getCompositionDevice();

                        ID3D11Device* const device = composition->getCompositionDevice()->getNativeDevice<D3D11>();

                        ID3D11DeviceContext* const context =
                            composition->getCompositionDevice()->getNativeContext<D3D11>();
                        // draw marker

                        // acquire last projection layer data
                        auto viewsForMarker = new std::vector<XrCompositionLayerProjectionView>{};
                        for (uint32_t eye = 0; eye < lastProjectionLayer->viewCount; eye++)
                        {
                            auto lastView = lastProjectionLayer->views[eye];
                            XrCompositionLayerProjectionView view{lastView.type,
                                                                  nullptr,
                                                                  lastView.pose,
                                                                  lastView.fov,
                                                                  lastView.subImage};
                            viewsForMarker->push_back(view);
                        }
                        m_CreatedViews.push_back(viewsForMarker);

                        for (size_t eye = 0; eye < viewsForMarker->size(); ++eye)
                        {
                            auto& view = (*viewsForMarker)[eye];
                            const XrRect2Di* viewPort = &view.subImage.imageRect;

                            if (m_MarkerSwapchains.size() <= eye)
                            {
                                // create swapchain for marker
                                XrSwapchainCreateInfo markerInfo{
                                    XR_TYPE_SWAPCHAIN_CREATE_INFO,
                                    nullptr,
                                    0,
                                    XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                                    composition->getPreferredSwapchainFormatOnApplicationDevice(
                                        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT),
                                    1,
                                    static_cast<uint32_t>(viewPort->extent.width),
                                    static_cast<uint32_t>(viewPort->extent.height),
                                    1,
                                    1,
                                    1};
                                m_MarkerSwapchains.push_back(
                                    composition->createSwapchain(markerInfo,
                                                                 SwapchainMode::Write | SwapchainMode::Submit));

                                // create depth texture
                                markerInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                                markerInfo.format = DXGI_FORMAT_D32_FLOAT;
                                m_MarkerDepthTextures.push_back(
                                    composition->getCompositionDevice()->createTexture(markerInfo));
                            }

                            auto markerImage = m_MarkerSwapchains[eye]->acquireImage();
                            ID3D11Texture2D* const rgbTexture =
                                markerImage->getTextureForWrite()->getNativeTexture<D3D11>();

                            const XrSwapchainCreateInfo& markerSwapchainCreateInfo =
                                m_MarkerSwapchains[eye]->getInfoOnCompositionDevice();

                            // create ephemeral render target view for the drawing.
                            auto renderTargetView = ComPtr<ID3D11RenderTargetView>();

                            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
                            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                            rtvDesc.Format = static_cast<DXGI_FORMAT>(markerSwapchainCreateInfo.format);
                            rtvDesc.Texture2DArray.ArraySize = 1;
                            rtvDesc.Texture2DArray.FirstArraySlice = -1;
                            rtvDesc.Texture2D.MipSlice = D3D11CalcSubresource(0, 0, 1);
                            CHECK_HRCMD(device->CreateRenderTargetView(rgbTexture, &rtvDesc, set(renderTargetView)));

                            // create ephemeral depth stencil view for depth testing / occlusion.
                            auto depthStencilView = ComPtr<ID3D11DepthStencilView>();
                            D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc{};
                            depthDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                            depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
                            depthDesc.Texture2DArray.ArraySize = 1;
                            depthDesc.Texture2DArray.FirstArraySlice = -1;
                            depthDesc.Texture2D.MipSlice = D3D11CalcSubresource(0, 0, 1);
                            CHECK_HRCMD(
                                device->CreateDepthStencilView(m_MarkerDepthTextures[eye]->getNativeTexture<D3D11>(),
                                                               &depthDesc,
                                                               set(depthStencilView)));

                            context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), get(depthStencilView));

                            // clear render target
                            constexpr float background[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                            context->ClearRenderTargetView(renderTargetView.Get(), background);

                            // clear depth buffer
                            context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);

                            // take over view projection
                            xr::math::ViewProjection viewProjection{};
                            viewProjection.Pose = view.pose;
                            viewProjection.Fov = view.fov;
                            viewProjection.NearFar = xr::math::NearFar{0.001f, 100.f};
                            graphicsDevice->setViewProjection(viewProjection);

                            // set viewport to match resolution
                            D3D11_VIEWPORT viewport{};
                            viewport.TopLeftX = static_cast<float>(viewPort->offset.x);
                            viewport.TopLeftY = static_cast<float>(viewPort->offset.y);
                            viewport.Width = static_cast<float>(viewPort->extent.width);
                            viewport.Height = static_cast<float>(viewPort->extent.height);
                            viewport.MaxDepth = 1.0f;
                            context->RSSetViewports(1, &viewport);

                            // draw reference pose marker
                            XrPosef referencePose =
                                mcActivated ? xr::math::Pose::Multiply(referenceTrackerPose,
                                                                       xr::math::Pose::Invert(reversedManipulation))
                                            : referenceTrackerPose;
                            graphicsDevice->draw(m_MeshRGB, referencePose, m_MarkerSize);

                            // draw tracker marker
                            if (mcActivated)
                            {
                                graphicsDevice->draw(m_MeshCMY, referenceTrackerPose, m_MarkerSize);
                            }

                            m_MarkerSwapchains[eye]->releaseImage();
                            m_MarkerSwapchains[eye]->commitLastReleasedImage();

                            view.subImage = m_MarkerSwapchains[eye]->getSubImage();
                        }

                        graphicsDevice->UnsetDrawResources();

                        m_CreatedProjectionLayer = new XrCompositionLayerProjection{
                            XR_TYPE_COMPOSITION_LAYER_PROJECTION,
                            nullptr,
                            lastProjectionLayer->layerFlags | XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
                            lastProjectionLayer->space,
                            lastProjectionLayer->viewCount,
                            viewsForMarker->data()};

                        m_BaseLayerVector.push_back(
                            reinterpret_cast<XrCompositionLayerBaseHeader*>(m_CreatedProjectionLayer));
                        chainFrameEndInfo->layerCount = static_cast<uint32_t>(m_BaseLayerVector.size());
                        chainFrameEndInfo->layers = m_BaseLayerVector.data();
                    }
                }
            }
            catch (std::exception& e)
            {
                ErrorLog("%s: encountered exception: %s", __FUNCTION__, e.what());
                m_Initialized = false;
            }
        }
    }

    void Overlay::DeleteResources()
    {
        if (m_CreatedProjectionLayer)
        {
            delete m_CreatedProjectionLayer;
            m_CreatedProjectionLayer = nullptr;
        }
        for (const auto view : m_CreatedViews)
        {
            delete view;
        }
        m_CreatedViews.clear();
        m_BaseLayerVector.clear();
    }

    std::vector<SimpleMeshVertex> Overlay::CreateMarker(bool reference)
    {
        float tip{1.f}, point4{0.4f}, point1{0.1f}, bottom{0.f};
        if (reference)
        {
            // slightly decrease size of reference marker to avoid z-fighting
            tip = 0.995f;
            point4 = 0.3995f;
            point1 = 0.099f;
            bottom = 0.005f;
        }
        bool upsideDown;
        GetConfig()->GetBool(Cfg::UpsideDown, upsideDown);
        // right
        std::vector<SimpleMeshVertex> vertices = CreateConeMesh({upsideDown ? tip : -tip, 0.f, 0.f},
                                                                {upsideDown ? point4 : -point4, point1, 0.f},
                                                                {upsideDown ? bottom : -bottom, 0.f, 0.f},
                                                                reference ? DarkRed : DarkMagenta,
                                                                reference ? Red : Magenta,
                                                                reference ? LightRed : LightMagenta);
        // up
        std::vector<SimpleMeshVertex> top = CreateConeMesh({0.f, upsideDown ? -tip : tip, 0.f},
                                                           {0.f, upsideDown ? -point4 : point4, point1},
                                                           {0.f, upsideDown ? -bottom : bottom, 0.f},
                                                           reference ? DarkBlue : DarkCyan,
                                                           reference ? Blue : Cyan,
                                                           reference ? LightBlue : LightCyan);
        // forward
        vertices.insert(vertices.end(), top.begin(), top.end());
        std::vector<SimpleMeshVertex> front = CreateConeMesh({0.f, 0.f, tip},
                                                             {point1, 0.f, point4},
                                                             {0.f, 0.f, bottom},
                                                             reference ? DarkGreen : DarkYellow,
                                                             reference ? Green : Yellow,
                                                             reference ? LightGreen : LightYellow);
        vertices.insert(vertices.end(), front.begin(), front.end());
        return vertices;
    }

    std::vector<SimpleMeshVertex> Overlay::CreateConeMesh(const XrVector3f& top,
                                                          const XrVector3f& side,
                                                          const XrVector3f& bottom,
                                                          const XrVector3f& topColor,
                                                          const XrVector3f& sideColor,
                                                          const XrVector3f& bottomColor)
    {
        std::vector<SimpleMeshVertex> vertices;
        const DirectX::XMVECTOR dxTop = xr::math::LoadXrVector3(top);
        

        constexpr float angleIncrement = DirectX::XM_2PI / 32.f;
        const DirectX::XMVECTOR rotation = DirectX::XMQuaternionRotationAxis(dxTop, angleIncrement);
        DirectX::XMVECTOR side1 = xr::math::LoadXrVector3(side);
        XrVector3f xrSide0, xrSide1;
        for (int i = 0; i < 128; i++)
        {
            const DirectX::XMVECTOR side0 = side1;
            side1 = DirectX::XMVector3Rotate(side0, rotation);
            xr::math::StoreXrVector3(&xrSide0, side0);
            xr::math::StoreXrVector3(&xrSide1, side1);

            // bottom
            vertices.push_back({bottom, bottomColor});
            vertices.push_back({xrSide0, sideColor});
            vertices.push_back({xrSide1, sideColor});

            // top
            vertices.push_back({top, topColor});
            vertices.push_back({xrSide1, sideColor});
            vertices.push_back({xrSide0, sideColor});
        }
        return vertices;
    }

    std::vector<uint16_t> Overlay::CreateIndices(size_t amount)
    {
        std::vector<uint16_t> indices;
        for (unsigned short i = 0; i < static_cast<uint16_t>(amount); i++)
        {
            indices.push_back(i);
        }
        return indices;
    }
} // namespace openxr_api_layer::graphics