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
#include <util.h>
#include <log.h>

using namespace openxr_api_layer;
using namespace openxr_api_layer::log;

namespace openxr_api_layer::graphics
{
    void Overlay::DestroySession(XrSession session)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::DestroySession", TLPArg(session, "Session"));

        std::unique_lock lock(m_DrawMutex);
        DeleteResources();
        m_MarkerSwapchains.clear();
        m_MarkerDepthTextures.clear();
        m_Swapchains.clear();
        m_MeshRGB.reset();
        m_MeshCMY.reset();
        m_InitializedSessions.erase(session);

        TraceLoggingWriteStop(local, "Overlay::DestroySession");
    }
    void Overlay::CreateSwapchain(XrSwapchain swapchain, const XrSwapchainCreateInfo* createInfo)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "Overlay::CreateSwapchain",
                               TLPArg(swapchain, "Swapchain"),
                               TLArg(m_D3D12inUse, "D3D12inUse"));

        uint32_t imageCount;
        if (const XrResult result = GetInstance()->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr); XR_FAILED(result))
        {
            TraceLoggingWriteStop(local, "Overlay::CreateSwapchain", TLArg(xr::ToCString(result),"EnumerateImages_Count"));
            return;
        }
        if (imageCount == 0)
        {
            TraceLoggingWriteStop(local, "Overlay::CreateSwapchain", TLArg(imageCount, "Image_Count"));
            return;
        }

        if (!m_D3D12inUse)
        {
            std::vector<XrSwapchainImageD3D11KHR> d3dImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
            if (const XrResult result = GetInstance()->OpenXrApi::xrEnumerateSwapchainImages(
                    swapchain,
                    imageCount,
                    &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())))
            {
                TraceLoggingWriteStop(local,
                                      "Overlay::CreateSwapchain",
                                      TLArg(xr::ToCString(result), "EnumerateImages_Images"));
                return;
            }

            if (d3dImages[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR)
            {
                ErrorLog("%s: image type %d is not matching XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR (%d)",
                         __FUNCTION__,
                         d3dImages[0].type,
                         XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR);
                TraceLoggingWriteStop(local, "Overlay::CreateSwapchain", TLArg(false, "ImageType_Match"));
                return;

            }
            // dump the descriptor for the first texture returned by the runtime for debug purposes.
            {
                D3D11_TEXTURE2D_DESC desc;
                d3dImages[0].texture->GetDesc(&desc);
                TraceLoggingWriteTagged(local,
                                  "Overlay::CreateSwapchain",
                                  TLArg(desc.Width, "Width"),
                                  TLArg(desc.Height, "Height"),
                                  TLArg(desc.ArraySize, "ArraySize"),
                                  TLArg(desc.MipLevels, "MipCount"),
                                  TLArg(desc.SampleDesc.Count, "SampleCount"),
                                  TLArg((int)desc.Format, "Format"),
                                  TLArg((int)desc.Usage, "Usage"),
                                  TLArg(desc.BindFlags, "BindFlags"),
                                  TLArg(desc.CPUAccessFlags, "CPUAccessFlags"),
                                  TLArg(desc.MiscFlags, "MiscFlags"));
            }

            std::vector<ID3D11Texture2D*> textures{};
            for (uint32_t i = 0; i < imageCount; i++)
            {
                TraceLoggingWriteTagged(local,
                                        "Overlay::CreateSwapchain",
                                        TLArg(i, "Index"),
                                        TLPArg(d3dImages[i].texture, "Texture"));
                textures.push_back(d3dImages[i].texture);
            }
            m_Swapchains[swapchain] = {textures,
                                       std::vector<ID3D12Resource*>(),
                                       static_cast<DXGI_FORMAT>(createInfo->format),
                                       0,
                                       false};
        }
        else
        {
            std::vector<XrSwapchainImageD3D12KHR> d3dImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
            if (const XrResult result = GetInstance()->OpenXrApi::xrEnumerateSwapchainImages(
                    swapchain,
                    imageCount,
                    &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())))
            {
                TraceLoggingWriteStop(local,
                                      "Overlay::CreateSwapchain",
                                      TLArg(xr::ToCString(result), "EnumerateImages_Images"));
                return;
            }

            if (d3dImages[0].type != XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR)
            {
                ErrorLog("%s: image type %d is not matching XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR (%d)",
                         __FUNCTION__,
                         d3dImages[0].type,
                         XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR);
                TraceLoggingWriteStop(local, "Overlay::CreateSwapchain", TLArg(false, "ImageType_Match"));
                return;
            }
            // Dump the descriptor for the first texture returned by the runtime for debug purposes.
            {
                const auto& desc = d3dImages[0].texture->GetDesc();
                TraceLoggingWrite(g_traceProvider,
                                  "RuntimeSwapchain",
                                  TLArg(desc.Width, "Width"),
                                  TLArg(desc.Height, "Height"),
                                  TLArg(desc.DepthOrArraySize, "ArraySize"),
                                  TLArg(desc.MipLevels, "MipCount"),
                                  TLArg(desc.SampleDesc.Count, "SampleCount"),
                                  TLArg((int)desc.Format, "Format"),
                                  TLArg((int)desc.Flags, "Flags"));
            }

            std::vector<ID3D12Resource*> textures{};
            for (uint32_t i = 0; i < imageCount; i++)
            {
                TraceLoggingWriteTagged(local,
                                        "Overlay::CreateSwapchain",
                                        TLArg(i, "Index"),
                                        TLPArg(d3dImages[i].texture, "Texture"));
                textures.push_back(d3dImages[i].texture);
            }
            m_Swapchains[swapchain] = {std::vector<ID3D11Texture2D*>(),
                                       textures,
                                       static_cast<DXGI_FORMAT>(createInfo->format),
                                       0,
                                       false};
        }
        TraceLoggingWriteStop(local, "Overlay::CreateSwapchain", TLArg(true, "Success"));
    }

    void Overlay::DestroySwapchain(const XrSwapchain swapchain)
    {
        m_Swapchains.erase(swapchain);
    }

    XrResult Overlay::AcquireSwapchainImage(XrSwapchain swapchain,
                                            const XrSwapchainImageAcquireInfo* acquireInfo,
                                            uint32_t* index)
    {
        std::unique_lock lock(m_DrawMutex);
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::AcquireSwapchainImage", TLPArg(swapchain, "Swapchain"));
        const auto swapchainIt = m_Swapchains.find(swapchain);
        if (swapchainIt != m_Swapchains.end())
        {
            // Perform the release now in case it was delayed.
            if (swapchainIt->second.doRelease)
            {
                TraceLoggingWriteTagged(local, "Overlay::AcquireSwapchainImage", TLArg(true, "Delayed_Release"));

                swapchainIt->second.doRelease = false;
                constexpr XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr};
                if (const XrResult result = GetInstance()->OpenXrApi::xrReleaseSwapchainImage(swapchain, &releaseInfo);
                    XR_SUCCEEDED(result))
                {
                    DebugLog("AcquireSwapchainImage: swapchain(%u) released", swapchain);
                    TraceLoggingWriteTagged(local,
                                            "Overlay::AcquireSwapchainImage",
                                            TLPArg(swapchain, "Swapchain_Released"));
                }
                else
                {
                    ErrorLog("%s: xrReleaseSwapchainImage(%u) failed: %d",
                             __FUNCTION__,
                             swapchain,
                             xr::ToCString(result));
                }               
            }
        }

        const XrResult result = GetInstance()->OpenXrApi::xrAcquireSwapchainImage(swapchain, acquireInfo, index);
        if (XR_SUCCEEDED(result))
        {
            // Record the index so we know which texture to use in xrEndFrame().
            if (swapchainIt != m_Swapchains.end())
            {
                DebugLog("AcquireSwapchainImage(%u): index = %u", swapchain, *index);
                TraceLoggingWriteTagged(local, "Overlay::AcquireSwapchainImage", TLArg(*index, "Acquired_Index"));
                swapchainIt->second.index = *index;
            }
        }
        TraceLoggingWriteStop(local,
                              "Overlay::AcquireSwapchainImage",
                              TLArg(*index, "Index"),
                              TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    XrResult Overlay::ReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
    {
        std::unique_lock lock(m_DrawMutex);
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ReleaseSwapchainImage", TLPArg(swapchain, "Swapchain"));

        const auto swapchainIt = m_Swapchains.find(swapchain);
        if (m_OverlayActive && swapchainIt != m_Swapchains.end())
        {
            // Perform a delayed release: we still need to copy the texture in DrawOverlay()
            swapchainIt->second.doRelease = true;
            DebugLog("ReleaseSwapchainImage(%u): release postponed", swapchain);
            TraceLoggingWriteStop(local, "Overlay::ReleaseSwapchainImage", TLArg(true, "Release_Postponed"));
            return XR_SUCCESS;
        }
        
        const XrResult result = GetInstance()->OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
        TraceLoggingWriteStop(local, "Overlay::ReleaseSwapchainImage", TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    void Overlay::BeginFrame()
    {
        std::unique_lock lock(m_DrawMutex);
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::BeginFrame");

        // Release the swapchain images. Some runtimes don't seem to lock cross-frame releasing and this can happen
        // when a frame is discarded.
        for (auto& swapchain : m_Swapchains)
        {
            if (swapchain.second.doRelease)
            {
                TraceLoggingWriteTagged(local, "Overlay::BeginFrame", TLPArg(swapchain.first, "Swapchain_Release"));

                constexpr XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr};
                swapchain.second.doRelease = false;
                if (const XrResult result = GetInstance()->xrReleaseSwapchainImage(swapchain.first, &releaseInfo);
                    XR_SUCCEEDED(result))
                {
                    DebugLog("BeginFrame: swapchain(%u) released", swapchain.first);
                    TraceLoggingWriteTagged(local, "Overlay::BeginFrame", TLPArg(swapchain.first, "Swapchain_Released"));
                }
                else
                {
                    ErrorLog("%s: xrReleaseSwapchainImage(%u) failed: %s",
                             __FUNCTION__,
                             swapchain.first,
                             xr::ToCString(result));
                }
            }
        }
        TraceLoggingWriteStop(local, "Overlay::BeginFrame");
    }

    void Overlay::SetMarkerSize()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::SetMarkerSize");

        float scaling{0.1f};
        GetConfig()->GetFloat(Cfg::MarkerSize, scaling);
        scaling /= 100.f;
        m_MarkerSize = {scaling, scaling, scaling};

        TraceLoggingWriteStop(local, "Overlay::SetMarkerSize", TLArg(xr::ToString(m_MarkerSize).c_str(), "MarkerSize"));
    }

    bool Overlay::ToggleOverlay()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ToggleOverlay");

        if (!m_Initialized)
        {
            m_OverlayActive = false;
            ErrorLog(" %s: graphical overlay is not properly initialized", __FUNCTION__);
            Feedback::AudioOut::Execute(Feedback::Event::Error);

            TraceLoggingWriteStop(local,
                                  "Overlay::ToggleOverlay",
                                  TLArg(false, "Success"),
                                  TLArg(m_OverlayActive, "OverlayACtive"));
            return false;
        }
        m_OverlayActive = !m_OverlayActive;
        Feedback::AudioOut::Execute(m_OverlayActive ? Feedback::Event::OverlayOn : Feedback::Event::OverlayOff);

        TraceLoggingWriteStop(local,
                              "Overlay::ToggleOverlay",
                              TLArg(true, "Success"),
                              TLArg(m_OverlayActive, "OverlayACtive"));
        return true;
    }

    void Overlay::DrawOverlay(const XrPosef& referencePose,
                              const XrPosef& delta,
                              bool mcActivated,
                              XrSession session,
                              XrFrameEndInfo* chainFrameEndInfo,
                              OpenXrLayer* openXrLayer)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "Overlay::DrawOverlay",
                               TLArg(chainFrameEndInfo->displayTime, "Time"),
                               TLArg(xr::ToString(referencePose).c_str(), "ReferencePose"),
                               TLArg(xr::ToString(delta).c_str(), "Delta"),
                               TLArg(mcActivated, "MC_Activated"));

        if (auto factory = openXrLayer->GetCompositionFactory(); m_Initialized && m_OverlayActive && factory)
        {
            TraceLoggingWriteTagged(local, "Overlay::DrawOverlay", TLArg(true, "Overlay_Active"));

            ICompositionFramework* composition = factory->getCompositionFramework(session);
            if (!composition)
            {
                ErrorLog("%s: unable to retrieve composition framework", __FUNCTION__);
                m_Initialized = false;
                TraceLoggingWriteStop(local, "Overlay::DrawOverlay", TLArg(false, "CompositionFramework"));
                return;
            }

            std::unique_lock lock(m_DrawMutex);
            composition->serializePreComposition();

            if (!m_InitializedSessions.contains(session))
            {
                std::vector<SimpleMeshVertex> vertices = CreateMarker(true);
                std::vector<uint16_t> indices;
                for (uint16_t i = 0; i < static_cast<uint16_t>(vertices.size()); i++)
                {
                    indices.push_back(i);
                }
                m_MeshRGB = composition->getCompositionDevice()->createSimpleMesh(vertices, indices, "RGB Mesh");
                vertices = CreateMarker(false);
                m_MeshCMY = composition->getCompositionDevice()->createSimpleMesh(vertices, indices, "CMY Mesh");
                TraceLoggingWriteTagged(local,
                                        "Overlay::DrawOverlay",
                                        TLPArg(m_MeshRGB.get(), "MeshRGB"),
                                        TLPArg(m_MeshCMY.get(), "MeshCMY"));
                m_InitializedSessions.insert(session);
                DebugLog("initialized marker meshes");
            }

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
                    XrPosef refToStage;
                    if (openXrLayer->GetRefToStage(lastProjectionLayer->space, &refToStage, nullptr))
                    {
                        DebugLog("overlay last projection layer space: %u, pose to stage: %s",
                                 lastProjectionLayer->space,
                                 xr::ToString(refToStage).c_str());

                        // transfer trackerPose into projection reference space
                        const XrPosef trackerPose = xr::math::Pose::Multiply(referencePose, refToStage);

                        // determine reference pose
                        const XrPosef refPose =
                            mcActivated ? xr::math::Pose::Multiply(trackerPose, delta) : trackerPose;

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
                        m_CreatedViews = viewsForMarker;

                        for (size_t eye = 0; eye < viewsForMarker->size(); ++eye)
                        {
                            auto& view = (*viewsForMarker)[eye];
                            XrSwapchain swapchain = view.subImage.swapchain;
                            const XrRect2Di* viewPort = &view.subImage.imageRect;

                            TraceLoggingWriteTagged(local,
                                                    "Overlay::DrawOverlay_ViewInfo",
                                                    TLArg(eye, "Eye"),
                                                    TLArg(viewPort->extent.width, "Width"),
                                                    TLArg(viewPort->extent.height, "Heigth"),
                                                    TLArg(viewPort->offset.x, "OffsetX"),
                                                    TLArg(viewPort->offset.y, "OffsetY"),
                                                    TLArg(view.subImage.imageArrayIndex, "ArrayIndex"),
                                                    TLArg(xr::ToString(view.pose).c_str(), "Pose"),
                                                    TLArg(xr::ToString(view.fov).c_str(), "Fov"),
                                                    TLPArg(view.next, "Next"));

                            if (!m_Swapchains.contains(swapchain))
                            {
                                throw std::runtime_error(fmt::format("unable to find swapchain state for: {}",
                                                                     reinterpret_cast<std::uintptr_t>(swapchain)));
                            }
                            const auto& swapchainState = m_Swapchains[swapchain];

                            if (m_MarkerSwapchains.size() <= eye)
                            {
                                // create swapchain for marker
                                XrSwapchainCreateInfo markerInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO,
                                                                 nullptr,
                                                                 0,
                                                                 XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                                                                 swapchainState.format,
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

                                DebugLog("overlay(%u) created swapchain and depth texture: %u x %u",
                                         eye,
                                         markerInfo.width,
                                         markerInfo.height);
                                TraceLoggingWriteTagged(local,
                                                        "Overlay::DrawOverlay_CreateSwapcahin",
                                                        TLPArg(m_MarkerSwapchains[eye].get(), "Swapchain"),
                                                        TLPArg(m_MarkerDepthTextures[eye].get(), "DepthTexture"));
                            }

                            auto markerImage = m_MarkerSwapchains[eye]->acquireImage();
                            ID3D11Texture2D* const rgbTexture =
                                markerImage->getTextureForWrite()->getNativeTexture<D3D11>();

                            // copy application texture
                            CopyAppTexture(composition->getApplicationDevice(),
                                           swapchain,
                                           swapchainState,
                                           device,
                                           context,
                                           rgbTexture);

                            // prepare marker rendering
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

                            // clear depth buffer
                            context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);

                            // take over view projection
                            xr::math::ViewProjection viewProjection{};
                            viewProjection.Pose = view.pose;
                            viewProjection.Fov = view.fov;
                            viewProjection.NearFar = xr::math::NearFar{0.001f, 100.f};
                            graphicsDevice->setViewProjection(viewProjection);
                            DebugLog("overlay(%u) view projection: pose = %s, fov = %s",
                                     eye,
                                     xr::ToString(viewProjection.Pose).c_str(),
                                     xr::ToString(viewProjection.Fov).c_str());

                            // set viewport to match resolution
                            D3D11_VIEWPORT viewport{};
                            viewport.TopLeftX = viewport.TopLeftY = 0;
                            viewport.Width = static_cast<float>(viewPort->extent.width);
                            viewport.Height = static_cast<float>(viewPort->extent.height);
                            viewport.MaxDepth = 1.0f;
                            context->RSSetViewports(1, &viewport);
                            DebugLog("overlay(%u) viewport: width = %d, height = %d, offset x: %d, offset y: %d",
                                     eye,
                                     viewPort->extent.width,
                                     viewPort->extent.height,
                                     viewPort->offset.x,
                                     viewPort->offset.y);

                            // draw reference/cor position
                            graphicsDevice->draw(m_MeshRGB, refPose, m_MarkerSize);
                            DebugLog("overlay(%u) reference pose: %s", eye, xr::ToString(refPose).c_str());

                            // draw tracker marker
                            if (mcActivated)
                            {
                                graphicsDevice->draw(m_MeshCMY, trackerPose, m_MarkerSize);
                                DebugLog("overlay(%u) tracker pose: %s", eye, xr::ToString(trackerPose).c_str());
                            }
                            context->Flush();

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

                        // replace last projection layer
                        std::ranges::replace(
                            m_BaseLayerVector,
                            reinterpret_cast<XrCompositionLayerBaseHeader const*>(lastProjectionLayer),
                            reinterpret_cast<XrCompositionLayerBaseHeader const*>(m_CreatedProjectionLayer));
                        chainFrameEndInfo->layerCount = static_cast<uint32_t>(m_BaseLayerVector.size());
                        chainFrameEndInfo->layers = m_BaseLayerVector.data();
                    }
                    else
                    {
                        ErrorLog("%s(%u): could not determine stage offset for projection reference space (%u)",
                                 __FUNCTION__,
                                 chainFrameEndInfo->displayTime,
                                 lastProjectionLayer->space);
                    }
                }
            }
            catch (std::exception& e)
            {
                ErrorLog("%s: encountered exception: %s", __FUNCTION__, e.what());
                m_Initialized = false;
            }

            composition->serializePostComposition();
        }
        // release all the swapchain images now, as we are really done this time
        for (auto& swapchain : m_Swapchains)
        {
            if (swapchain.second.doRelease)
            {
                TraceLoggingWriteTagged(local,
                                        "Overlay::DrawOverlay",
                                        TLArg(true, "Delayed_Release"),
                                        TLPArg(swapchain.first, "Swapchain"));

                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                swapchain.second.doRelease = false;
                const XrResult result =
                    GetInstance()->OpenXrApi::xrReleaseSwapchainImage(swapchain.first, &releaseInfo);
                if (XR_SUCCEEDED(result))
                {
                    DebugLog("DrawOverlay: swapchain(%u) released", swapchain.first);
                    TraceLoggingWriteTagged(local,
                                            "Overlay::AcquireSwapchainImage",
                                            TLPArg(swapchain.first, "Swapchain_Released"));
                }
                else
                {
                    ErrorLog("%s: xrReleaseSwapchainImage(%u) failed: %d",
                             __FUNCTION__,
                             swapchain,
                             xr::ToCString(result));
                }
            }
        }
        TraceLoggingWriteStop(local, "Overlay::DrawOverlay");
    }

    void Overlay::CopyAppTexture(IGraphicsDevice* appDevice,
                                 XrSwapchain swapchain,
                                 const SwapchainState& swapchainState,
                                 ID3D11Device* const device,
                                 ID3D11DeviceContext* const context,
                                 ID3D11Texture2D* const rgbTexture)
    {
        // TODO: keep sharedTexture instead of recreate
        // TODO: differentiate D3D12 / move to GraphicDevice?
        // TODO: clean up resources
        if (swapchainState.index >= swapchainState.texturesD3D11.size())
        {
            throw std::runtime_error(fmt::format("invalid to texture index {} for swapchain {}, max: {}",
                                                 swapchainState.index,
                                                 reinterpret_cast<std::uintptr_t>(swapchain),
                                                 swapchainState.texturesD3D11.size() - 1));
        }
        auto appTexture = swapchainState.texturesD3D11[swapchainState.index];

        D3D11_TEXTURE2D_DESC desc;
        appTexture->GetDesc(&desc);
        desc.Format = swapchainState.format;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        ComPtr<ID3D11Texture2D> shareTexture;
        CHECK_HRCMD(appDevice->getNativeDevice<D3D11>()->CreateTexture2D(&desc,
                                                                         nullptr,
                                                                         shareTexture.ReleaseAndGetAddressOf()));

        ComPtr<IDXGIKeyedMutex> appMutex;
        CHECK_HRCMD(shareTexture->QueryInterface(IID_PPV_ARGS(appMutex.ReleaseAndGetAddressOf())));
        if (!appMutex || WAIT_OBJECT_0 != appMutex->AcquireSync(0, 20))
        {
            throw std::runtime_error(
                fmt::format("unable to acquire mutex for shared texture on app device for swapchain {}",
                            reinterpret_cast<std::uintptr_t>(swapchain)));
        }

        auto appContext = appDevice->getNativeContext<D3D11>();
        appContext->CopyResource(shareTexture.Get(), appTexture);
        appContext->Flush();
        if (!appMutex || WAIT_OBJECT_0 != appMutex->ReleaseSync(1))
        {
            throw std::runtime_error(
                fmt::format("unable release mutex for shared texture on app device for swapchain {}",
                            reinterpret_cast<std::uintptr_t>(swapchain)));
        }

        ComPtr<IDXGIResource1> dxgiResource;
        CHECK_HRCMD(shareTexture->QueryInterface(IID_PPV_ARGS(dxgiResource.ReleaseAndGetAddressOf())));

        ShareableHandle sharedHandle{};
        CHECK_HRCMD(dxgiResource->GetSharedHandle(&sharedHandle.handle));
        dxgiResource->Release();

        CHECK_HRCMD(device->OpenSharedResource(sharedHandle.handle,
                                               IID_ID3D11Texture2D,
                                               reinterpret_cast<void**>(shareTexture.ReleaseAndGetAddressOf())));

        ComPtr<IDXGIKeyedMutex> compositionMutex;
        CHECK_HRCMD(shareTexture->QueryInterface(IID_PPV_ARGS(compositionMutex.ReleaseAndGetAddressOf())));

        if (!compositionMutex || WAIT_OBJECT_0 != compositionMutex->AcquireSync(1, 20))
        {
            throw std::runtime_error(fmt::format("unable acquire mutex for shared texture on "
                                                 "composition device for swapchain {}",
                                                 reinterpret_cast<std::uintptr_t>(swapchain)));
        }
        context->CopyResource(rgbTexture, shareTexture.Get());
        context->Flush();
        if (!compositionMutex || WAIT_OBJECT_0 != compositionMutex->ReleaseSync(0))
        {
            throw std::runtime_error(fmt::format("unable to release mutex for shared texture on "
                                                 "composition device for swapchain {}",
                                                 reinterpret_cast<std::uintptr_t>(swapchain)));
        }
    }

    void Overlay::DeleteResources()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::DeleteResources");

        if (m_CreatedProjectionLayer)
        {
            delete m_CreatedProjectionLayer;
            m_CreatedProjectionLayer = nullptr;
        }
        if (m_CreatedViews)
        {
            delete m_CreatedViews;
            m_CreatedViews = nullptr;
        }
        m_BaseLayerVector.clear();

        TraceLoggingWriteStop(local, "Overlay::DeleteResources");
    }

    std::vector<SimpleMeshVertex> Overlay::CreateMarker(bool reference)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::CreateMarker", TLArg(reference, "Refernace"));

        float tip{1.f}, point65{0.65f}, point6{0.6f}, point1{0.1f}, point05{0.05f}, bottom{0.f};
        if (reference)
        {
            // slightly decrease size of reference marker to avoid z-fighting
            tip = 0.995f;
            point65 = 0.6575f;
            point6 = 0.605f;
            point1 = 0.095f;
            point05 = 0.0475f;
            bottom = 0.005f;
        }
        bool upsideDown;
        GetConfig()->GetBool(Cfg::UpsideDown, upsideDown);

        // right
        std::vector<SimpleMeshVertex> vertices = CreateMarkerMesh({upsideDown ? tip : -tip, 0.f, 0.f},
                                                                {upsideDown ? point65 : -point65, point05, 0.f},
                                                                {upsideDown ? point6 : -point6, point1, 0.f},
                                                                {upsideDown ? bottom : -bottom, 0.f, 0.f},
                                                                reference ? DarkRed : DarkMagenta,
                                                                reference ? Red : Magenta,
                                                                reference ? LightRed : LightMagenta);
        // up
        std::vector<SimpleMeshVertex> top = CreateMarkerMesh({0.f, upsideDown ? -tip : tip, 0.f},
                                                           {0.f, upsideDown ? -point65 : point65, point05},
                                                           {0.f, upsideDown ? -point6 : point6, point1},
                                                           {0.f, upsideDown ? -bottom : bottom, 0.f},
                                                           reference ? DarkBlue : DarkCyan,
                                                           reference ? Blue : Cyan,
                                                           reference ? LightBlue : LightCyan);
        // forward
        vertices.insert(vertices.end(), top.begin(), top.end());
        std::vector<SimpleMeshVertex> front = CreateMarkerMesh({0.f, 0.f, tip},
                                                             {point05, 0.f, point65},
                                                             {point1, 0.f, point6},
                                                             {0.f, 0.f, bottom},
                                                             reference ? DarkGreen : DarkYellow,
                                                             reference ? Green : Yellow,
                                                             reference ? LightGreen : LightYellow);
        vertices.insert(vertices.end(), front.begin(), front.end());

        TraceLoggingWriteStop(local, "Overlay::CreateMarker");

        return vertices;
    }

    std::vector<SimpleMeshVertex> Overlay::CreateMarkerMesh(const XrVector3f& top,
                                                          const XrVector3f& innerMiddle,
                                                          const XrVector3f& outerMiddle,
                                                          const XrVector3f& bottom,
                                                          const XrVector3f& darkColor,
                                                          const XrVector3f& pureColor,
                                                          const XrVector3f& lightColor)
    {
        std::vector<SimpleMeshVertex> vertices;
        const DirectX::XMVECTOR dxTop = xr::math::LoadXrVector3(top);

        constexpr float angleIncrement = DirectX::XM_2PI / 32.f;
        const DirectX::XMVECTOR rotation = DirectX::XMQuaternionRotationAxis(dxTop, angleIncrement);
        DirectX::XMVECTOR sideInner1 = xr::math::LoadXrVector3(innerMiddle);
        DirectX::XMVECTOR sideOuter1 = xr::math::LoadXrVector3(outerMiddle);
        XrVector3f xrSide0, xrSide1, xrSide2, xrSide3;
        for (int i = 0; i < 128; i++)
        {
            const DirectX::XMVECTOR side0 = sideInner1;
            sideInner1 = DirectX::XMVector3Rotate(side0, rotation);
            xr::math::StoreXrVector3(&xrSide0, side0);
            xr::math::StoreXrVector3(&xrSide1, sideInner1);

            const DirectX::XMVECTOR side2 = sideOuter1;
            sideOuter1 = DirectX::XMVector3Rotate(side2, rotation);
            xr::math::StoreXrVector3(&xrSide2, side2);
            xr::math::StoreXrVector3(&xrSide3, sideOuter1);

            // bottom
            vertices.push_back({bottom, darkColor});
            vertices.push_back({xrSide0, pureColor});
            vertices.push_back({xrSide1, pureColor});

            // middle inner
            vertices.push_back({xrSide2, pureColor});
            vertices.push_back({xrSide1, darkColor});
            vertices.push_back({xrSide0, darkColor});

             // middle outer
            vertices.push_back({xrSide1, darkColor});
            vertices.push_back({xrSide2, pureColor});
            vertices.push_back({xrSide3, pureColor});

            // top
            vertices.push_back({top, lightColor});
            vertices.push_back({xrSide3, pureColor});
            vertices.push_back({xrSide2, pureColor});
        }
        return vertices;
    }
} // namespace openxr_api_layer::graphics