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
#include "output.h"
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
        TraceLoggingWriteStart(local, "Overlay::DestroySession", TLXArg(session, "Session"));

        std::lock_guard lock(m_DrawMutex);
        m_Textures.clear();
        m_Swapchains.clear();
        m_MeshRGB.reset();
        m_MeshCMY.reset();
        m_MeshCGY.reset();
        m_InitializedSessions.erase(session);

        TraceLoggingWriteStop(local, "Overlay::DestroySession");
    }
    void Overlay::CreateSwapchain(XrSwapchain swapchain, const XrSwapchainCreateInfo* createInfo)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "Overlay::CreateSwapchain",
                               TLXArg(swapchain, "Swapchain"),
                               TLArg(m_D3D12inUse, "D3D12inUse"));

        uint32_t imageCount;
        if (const XrResult result = GetInstance()->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
            XR_FAILED(result))
        {
            TraceLoggingWriteStop(local,
                                  "Overlay::CreateSwapchain",
                                  TLArg(xr::ToCString(result), "EnumerateImages_Count"));
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
            m_Swapchains[swapchain] = {swapchain,
                                       textures,
                                       std::vector<ID3D12Resource*>(),
                                       createInfo->width,
                                       createInfo->height,
                                       static_cast<DXGI_FORMAT>(createInfo->format),
                                       0,
                                       false};
            Log("swapchain (%llu): access to %u D3D11 textures added to overlay", swapchain, textures.size());
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
            m_Swapchains[swapchain] = {swapchain,
                                       std::vector<ID3D11Texture2D*>(),
                                       textures,
                                       createInfo->width,
                                       createInfo->height,
                                       static_cast<DXGI_FORMAT>(createInfo->format),
                                       0,
                                       false};
            Log("swapchain (%llu): access to %u D3D12 textures added to overlay", swapchain, textures.size());
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
        std::lock_guard lock(m_DrawMutex);
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::AcquireSwapchainImage", TLXArg(swapchain, "Swapchain"));
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
                    DebugLog("AcquireSwapchainImage: swapchain(%llu) released", swapchain);
                    TraceLoggingWriteTagged(local,
                                            "Overlay::AcquireSwapchainImage",
                                            TLXArg(swapchain, "Swapchain_Released"));
                }
                else
                {
                    ErrorLog("%s: xrReleaseSwapchainImage(%llu) failed: %s",
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
                DebugLog("AcquireSwapchainImage(%llu): index = %u", swapchain, *index);
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
        std::lock_guard lock(m_DrawMutex);
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ReleaseSwapchainImage", TLXArg(swapchain, "Swapchain"));

        const auto swapchainIt = m_Swapchains.find(swapchain);
        if (m_MarkersActive && swapchainIt != m_Swapchains.end())
        {
            // Perform a delayed release: we still need to copy the texture in DrawMarkers()
            swapchainIt->second.doRelease = true;
            DebugLog("ReleaseSwapchainImage(%llu): release postponed", swapchain);
            TraceLoggingWriteStop(local, "Overlay::ReleaseSwapchainImage", TLArg(true, "Release_Postponed"));
            return XR_SUCCESS;
        }

        const XrResult result = GetInstance()->OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
        TraceLoggingWriteStop(local, "Overlay::ReleaseSwapchainImage", TLArg(xr::ToCString(result), "Result"));
        return result;
    }

    void Overlay::ReleaseAllSwapChainImages()
    {
        std::lock_guard lock(m_DrawMutex);
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ReleaseAllSwapChainImages");

        // Release the swapchain images. Some runtimes don't seem to lock cross-frame releasing and this can happen
        // when a frame is discarded.
        for (auto& swapchain : m_Swapchains)
        {
            if (swapchain.second.doRelease)
            {
                TraceLoggingWriteTagged(local,
                                        "Overlay::ReleaseAllSwapChainImages",
                                        TLXArg(swapchain.first, "Swapchain_Release"));

                constexpr XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr};
                swapchain.second.doRelease = false;
                if (const XrResult result =
                        GetInstance()->OpenXrApi::xrReleaseSwapchainImage(swapchain.first, &releaseInfo);
                    XR_SUCCEEDED(result))
                {
                    DebugLog("ReleaseAllSwapChainImages: swapchain(%llu) released", swapchain.first);
                    TraceLoggingWriteTagged(local,
                                            "Overlay::ReleaseAllSwapChainImages",
                                            TLXArg(swapchain.first, "Swapchain_Released"));
                }
                else
                {
                    ErrorLog("%s: xrReleaseSwapchainImage(%llu) failed: %s",
                             __FUNCTION__,
                             swapchain.first,
                             xr::ToCString(result));
                }
            }
        }
        TraceLoggingWriteStop(local, "Overlay::ReleaseAllSwapChainImages");
    }

    void Overlay::ResetMarker()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ResetMarker");

        float scaling{0.1f};
        GetConfig()->GetFloat(Cfg::MarkerSize, scaling);
        scaling /= 100.f;
        m_MarkerSize = {scaling, scaling, scaling};

        TraceLoggingWriteStop(local, "Overlay::ResetMarker", TLArg(xr::ToString(m_MarkerSize).c_str(), "MarkerSize"));
    }

    void Overlay::ResetCrosshair()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ResetCrosshair");

        GetConfig()->GetBool(Cfg::CrosshairLockHorizon, m_CrosshairLockToHorizon);
        float distance = 100.f, scale = 1.f;
        GetConfig()->GetFloat(Cfg::CrosshairDistance, distance);
        GetConfig()->GetFloat(Cfg::CrosshairScale, scale);
        distance = std::max(distance, 1.0f);
        scale = std::max(scale, .01f);

        m_CrosshairDistance = distance * -.01f;
        m_CrosshairLayer.pose = xr::math::Pose::Translation({0.0f, 0.0f, m_CrosshairDistance});
        m_CrosshairLayer.size.width = m_CrosshairLayer.size.height = scale * distance * .02f;

        TraceLoggingWriteStop(local,
                              "Overlay::ResetCrosshair",
                              TLArg(xr::ToString(m_CrosshairLayer.pose).c_str(), "CrosshairPose"),
                              TLArg(std::to_string(m_CrosshairLayer.size.width).c_str(), "CrosshairSize"));
    }

    bool Overlay::ToggleOverlay()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ToggleOverlay");

        if (!m_MarkersInitialized)
        {
            m_MarkersActive = false;
            ErrorLog("%s: marker overlay is not properly initialized", __FUNCTION__);
            output::EventSink::Execute(output::Event::Error);
            TraceLoggingWriteStop(local,
                                  "Overlay::ToggleOverlay",
                                  TLArg(false, "Success"),
                                  TLArg(m_MarkersActive, "MarkersActive"));
            return false;
        }
        m_MarkersActive = !m_MarkersActive;

        Log("graphical overlay toggled %s", m_MarkersActive ? "on" : "off");
        output::EventSink::Execute(m_MarkersActive ? output::Event::OverlayOn : output::Event::OverlayOff);
        TraceLoggingWriteStop(local,
                              "Overlay::ToggleOverlay",
                              TLArg(true, "Success"),
                              TLArg(m_MarkersActive, "MarkersActive"));
        return true;
    }

    bool Overlay::TogglePassthrough()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::TogglePassthrough");

        if (!m_MarkersInitialized)
        {
            ErrorLog("%s: marker overlay is not properly initialized", __FUNCTION__);
            output::EventSink::Execute(output::Event::Error);

            TraceLoggingWriteStop(local,
                                  "Overlay::TogglePassthrough",
                                  TLArg(false, "Success"),
                                  TLArg(m_PassthroughActive, "PassthroughActive"));
            return false;
        }
        m_PassthroughActive = !m_PassthroughActive;
        output::EventSink::Execute(m_PassthroughActive ? output::Event::PassthroughOn : output::Event::PassthroughOff);

        TraceLoggingWriteStop(local,
                              "Overlay::TogglePassthrough",
                              TLArg(true, "Success"),
                              TLArg(m_PassthroughActive, "PassthroughActive"));
        return true;
    }

    bool Overlay::ToggleCrosshair()
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local, "Overlay::ToggleCrosshair");

        if (!m_CrosshairInitialized)
        {
            ErrorLog("%s: crosshair overlay is not properly initialized", __FUNCTION__);
            output::EventSink::Execute(output::Event::Error);

            TraceLoggingWriteStop(local,
                                  "Overlay::ToggleCrosshair",
                                  TLArg(false, "Success"),
                                  TLArg(m_CrosshairActive, "CrosshairActive"));
            return false;
        }
        m_CrosshairActive = !m_CrosshairActive;
        output::EventSink::Execute(m_CrosshairActive ? output::Event::CrosshairOn : output::Event::CrosshairOff);

        TraceLoggingWriteStop(local,
                              "Overlay::ToggleCrosshair",
                              TLArg(true, "Success"),
                              TLArg(m_CrosshairActive, "CrosshairActive"));
        return true;
    }

    void Overlay::DrawMarkers(const XrPosef& referencePose,
                              const XrPosef& delta,
                              bool calibrated,
                              bool drawTracker,
                              XrSession session,
                              XrFrameEndInfo* chainFrameEndInfo,
                              OpenXrLayer* openXrLayer)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "Overlay::DrawMarkers",
                               TLArg(chainFrameEndInfo->displayTime, "Time"),
                               TLArg(xr::ToString(referencePose).c_str(), "ReferencePose"),
                               TLArg(xr::ToString(delta).c_str(), "Delta"),
                               TLArg(drawTracker, "DrawTracker"));
        if (!(m_MarkersInitialized && m_MarkersActive && m_SessionVisible))
        {
            TraceLoggingWriteStop(local,
                                  "Overlay::DrawMarkers",
                                  TLArg(m_MarkersInitialized, "MarkersInitialized"),
                                  TLArg(m_MarkersActive, "MarkersActive"),
                                  TLArg(m_SessionVisible, "SessionVisible"));
            return;
        }
        TraceLoggingWriteTagged(local, "Overlay::DrawMarkers", TLArg(true, "Overlay_Active"));

        
        try
        {
            auto factory = openXrLayer->GetCompositionFactory();
            if (!factory)
            {
                ErrorLog("%s: unable to retrieve composition framework factory", __FUNCTION__);
                m_MarkersInitialized = false;
                TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(false, "CompositionFrameworkFactory"));
                return;
            }

            ICompositionFramework* composition = factory->getCompositionFramework(session);
            if (!composition)
            {
                ErrorLog("%s: unable to retrieve composition framework", __FUNCTION__);
                m_MarkersInitialized = false;
                TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(false, "CompositionFramework"));
                return;
            }

            std::lock_guard lock(m_DrawMutex);

            if (!m_InitializedSessions.contains(session))
            {
                std::vector<SimpleMeshVertex> vertices = CreateMarker(true, false);
                std::vector<uint16_t> indices;
                for (uint16_t i = 0; i < static_cast<uint16_t>(vertices.size()); i++)
                {
                    indices.push_back(i);
                }
                m_MeshRGB = composition->getCompositionDevice()->createSimpleMesh(vertices, indices, "RGB Mesh");
                vertices = CreateMarker(false, false);
                m_MeshCMY = composition->getCompositionDevice()->createSimpleMesh(vertices, indices, "CMY Mesh");
                vertices = CreateMarker(false, true);
                m_MeshCGY = composition->getCompositionDevice()->createSimpleMesh(vertices, indices, "CGY Mesh");
                TraceLoggingWriteTagged(local,
                                        "Overlay::DrawMarkers",
                                        TLPArg(m_MeshRGB.get(), "MeshRGB"),
                                        TLPArg(m_MeshCMY.get(), "MeshCMY"),
                                        TLPArg(m_MeshCGY.get(), "MeshCGY"));
                ResetMarker();

                m_InitializedSessions.insert(session);
                DebugLog("initialized marker meshes");
            }

            const XrCompositionLayerProjection* lastProjectionLayer{};
            for (uint32_t i = 0; i < chainFrameEndInfo->layerCount; i++)
            {
                auto layer = chainFrameEndInfo->layers[i];
                if (XR_TYPE_COMPOSITION_LAYER_PROJECTION == layer->type)
                {
                    lastProjectionLayer = reinterpret_cast<const XrCompositionLayerProjection*>(layer);
                }
            }
            if (!lastProjectionLayer)
            {
                if (!std::exchange(m_NoProjectionLayer, true))
                {
                    DebugLog("warning: no projection layer found");
                }
                TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(false, "ProjectionLayer"));
                return;
            }
            if (std::exchange(m_NoProjectionLayer, false))
            {
                DebugLog("projection layer present again");
            }

            // transfer tracker poses into projection reference space
            XrPosef refToStage;
            if (!openXrLayer->GetRefToStage(lastProjectionLayer->space, &refToStage, nullptr))
            {
                ErrorLog("%s(%lld): could not determine stage offset for projection reference space (%llu)",
                         __FUNCTION__,
                         chainFrameEndInfo->displayTime,
                         lastProjectionLayer->space);
                m_MarkersInitialized = false;
                TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(false, "RefToStage"));
                return;
            }
            DebugLog("overlay last projection layer space: %llu, pose to stage: %s",
                     lastProjectionLayer->space,
                     xr::ToString(refToStage).c_str());

            // calculate tracker pose
            const XrPosef trackerPose = xr::Normalize(xr::math::Pose::Multiply(
                (drawTracker || !calibrated) ? referencePose
                                             : xr::math::Pose::Multiply(referencePose, xr::math::Pose::Invert(delta)),
                refToStage));

            // calculate reference pose
            const XrPosef refPose = xr::Normalize(
                xr::math::Pose::Multiply(!drawTracker ? referencePose : xr::math::Pose::Multiply(referencePose, delta),
                                         refToStage));

            DebugLog("overlay reference pose: %s", xr::ToString(refPose).c_str());
            if (drawTracker)
            {
                DebugLog("overlay tracker pose: %s", xr::ToString(trackerPose).c_str());
            }

            for (uint32_t eye = 0; eye < lastProjectionLayer->viewCount; eye++)
            {
                auto& view = lastProjectionLayer->views[eye];
                const XrSwapchain swapchain = view.subImage.swapchain;
                const XrRect2Di* imageRect = &view.subImage.imageRect;

                TraceLoggingWriteTagged(local,
                                        "Overlay::DrawMarkers",
                                        TLArg(eye, "Eye"),
                                        TLArg(imageRect->extent.width, "Width"),
                                        TLArg(imageRect->extent.height, "Heigth"),
                                        TLArg(imageRect->offset.x, "OffsetX"),
                                        TLArg(imageRect->offset.y, "OffsetY"),
                                        TLArg(view.subImage.imageArrayIndex, "ArrayIndex"),
                                        TLArg(xr::ToString(view.pose).c_str(), "Pose"),
                                        TLArg(xr::ToString(view.fov).c_str(), "Fov"),
                                        TLPArg(view.next, "Next"));

                if (!InitializeTextures(eye, swapchain, composition))
                {
                    m_MarkersInitialized = false;
                    TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(false, "AppTexture_Initialized"));
                    return;
                }

                const auto colorTexture = m_Textures[eye].first;

                // copy from application texture
                if (!composition->getApplicationDevice()->CopyAppTexture(m_Swapchains[swapchain],
                                                                         eye,
                                                                         colorTexture,
                                                                         true))
                {
                    ErrorLog("%s: unable to copy app texture for swapchain: %llu", __FUNCTION__, swapchain);
                    m_MarkersInitialized = false;
                    TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(false, "AppTexture_Copied"));
                    return;
                }

                composition->serializePreComposition();

                // draw marker on copied texture
                RenderMarkers(view, eye, refPose, trackerPose, drawTracker || calibrated, composition);

                composition->serializePostComposition();

                // copy back to application texture
                if (!composition->getApplicationDevice()->CopyAppTexture(m_Swapchains[swapchain],
                                                                         eye,
                                                                         colorTexture,
                                                                         false))
                {
                    ErrorLog("%s: unable to copy app texture for swapchain: %llu", __FUNCTION__, swapchain);
                    m_MarkersInitialized = false;
                    TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(false, "AppTexture_Copied_Back"));
                    return;
                }
            }
        }
        catch (std::exception& e)
        {
            ErrorLog("%s: encountered exception: %s", __FUNCTION__, e.what());
            m_MarkersInitialized = false;
        }
        TraceLoggingWriteStop(local, "Overlay::DrawMarkers", TLArg(true, "Success"));
    }

    void Overlay::DrawCrosshair(XrSession session, XrFrameEndInfo* chainFrameEndInfo, OpenXrLayer* openXrLayer)
    {
        TraceLocalActivity(local);
        TraceLoggingWriteStart(local,
                               "Overlay::DrawCrosshair",
                               TLArg(chainFrameEndInfo->displayTime, "Time"));
        if (!(m_CrosshairInitialized && m_CrosshairActive && m_SessionVisible))
        {
            TraceLoggingWriteStop(local,
                                  "Overlay::DrawMarkers",
                                  TLArg(m_CrosshairInitialized, "CrosshairInitialized"),
                                  TLArg(m_CrosshairActive, "CrosshairActive"),
                                  TLArg(m_SessionVisible, "SessionVisible"));
            return;
        }
        TraceLoggingWriteTagged(local, "Overlay::DrawCrosshair", TLArg(true, "Crosshair_Active"));

        try
        {
            auto factory = openXrLayer->GetCompositionFactory();
            if (!factory)
            {
                ErrorLog("%s: unable to retrieve composition framework factory", __FUNCTION__);
                m_MarkersInitialized = false;
                TraceLoggingWriteStop(local, "Overlay::DrawCrosshair", TLArg(false, "CompositionFrameworkFactory"));
                return;
            }

            ICompositionFramework* composition = factory->getCompositionFramework(session);
            if (!composition)
            {
                ErrorLog("%s: unable to retrieve composition framework", __FUNCTION__);
                m_MarkersInitialized = false;
                TraceLoggingWriteStop(local, "Overlay::DrawCrosshair", TLArg(false, "CompositionFramework"));
                return;
            }

            if (!m_CrosshairSwapchain &&
                !((m_CrosshairInitialized = InitializeCrosshair(composition, openXrLayer->m_ViewSpace))))
            {
                ErrorLog("%s: unable to initialize crosshair overlay");
                TraceLoggingWriteStop(local, "Overlay::DrawCrosshair", TLArg(false, "Initialized"));
                return;
            }

            // adjust crosshair rotation so it stays vertically/horizontally aligned
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
            if (const XrResult result = openXrLayer->OpenXrApi::xrLocateSpace(openXrLayer->m_ViewSpace,
                                                                              openXrLayer->m_StageSpace,
                                                                              chainFrameEndInfo->displayTime,
                                                                              &location);
                XR_SUCCEEDED(result) && xr::math::Pose::IsPoseValid(location.locationFlags))
            {
                TraceLoggingWriteTagged(local,
                                        "Overlay::DrawCrosshair",
                                        TLArg(xr::ToString(location.pose).c_str(), "View_Pose")); 
                const auto [pitch, yaw, roll] = utility::ToEulerAngles(location.pose.orientation);
                if (!m_CrosshairLockToHorizon)
                {
                    xr::math::StoreXrQuaternion(&m_CrosshairLayer.pose.orientation,
                                                DirectX::XMQuaternionRotationRollPitchYaw(0, 0, -roll));
                }
                else
                {
                    xr::math::StoreXrQuaternion(&m_CrosshairLayer.pose.orientation,
                                                DirectX::XMQuaternionNormalize(DirectX::XMQuaternionInverse(
                                                    DirectX::XMQuaternionRotationRollPitchYaw(pitch, 0, roll))));
                    xr::math::StoreXrVector3(
                        &m_CrosshairLayer.pose.position,
                        DirectX::XMVector3Rotate({0, 0, m_CrosshairDistance},
                                                 xr::math::LoadXrQuaternion(m_CrosshairLayer.pose.orientation)));
                }
                TraceLoggingWriteTagged(local,
                                        "Overlay::DrawCrosshair",
                                        TLArg(xr::ToString(m_CrosshairLayer.pose).c_str(), "Crosshair_Pose"),
                                        TLArg(m_CrosshairLockToHorizon, "Horizon_Locked")); 
            }

            // add crosshair layer
            m_LayersForSubmission = std::make_shared<std::vector<const XrCompositionLayerBaseHeader*>>(
                chainFrameEndInfo->layers,
                chainFrameEndInfo->layers + chainFrameEndInfo->layerCount);

            m_LayersForSubmission->push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_CrosshairLayer));

            chainFrameEndInfo->layers = m_LayersForSubmission->data();
            chainFrameEndInfo->layerCount = static_cast<uint32_t>(m_LayersForSubmission->size());
        }
        catch (std::exception& e)
        {
            ErrorLog("%s: encountered exception: %s", __FUNCTION__, e.what());
            m_CrosshairInitialized = false;
        }

        TraceLoggingWriteStop(local, "Overlay::DrawCrosshair", TLArg(true, "Success"));
    }


    bool Overlay::InitializeTextures(uint32_t eye, XrSwapchain swapchain, const ICompositionFramework* composition)
    {
        TraceLoggingActivity<g_traceProvider> local;
        TraceLoggingWriteStart(local, "Overlay::InitializeTextures");

        if (!m_Swapchains.contains(swapchain))
        {
            ErrorLog("%s: unable to find state for swapchain: %llu", __FUNCTION__, swapchain);
            TraceLoggingWriteStop(local, "Overlay::InitializeTextures", TLArg(false, "SwapchainState_Found"));
            return false;
        }

        // initialize internal swapchains
        if (m_Textures.size() <= eye)
        {
            // create color texture for marker
            XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO,
                                             nullptr,
                                             0,
                                             XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
                                             m_Swapchains[swapchain].format,
                                             1,
                                             m_Swapchains[swapchain].width,
                                             m_Swapchains[swapchain].height,
                                             1,
                                             1,
                                             1};
            auto colorTexture = composition->getCompositionDevice()->createTexture(createInfo);

            // create depth texture
            createInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            createInfo.format = DXGI_FORMAT_D32_FLOAT;
            auto depthTexture = composition->getCompositionDevice()->createTexture(createInfo);
            m_Textures.push_back({colorTexture, depthTexture});

            DebugLog("overlay(%u) color and depth texture created: %u x %u", eye, createInfo.width, createInfo.height);
            TraceLoggingWriteTagged(local,
                                    "Overlay::InitializeTextures",
                                    TLPArg(colorTexture.get(), "ColorTexture"),
                                    TLPArg(depthTexture.get(), "DepthTexture"));
        }

        TraceLoggingWriteStop(local, "Overlay::InitializeTextures", TLArg(true, "Success"));
        return true;
    }

    bool Overlay::InitializeCrosshair(ICompositionFramework* composition, XrSpace viewSpace)
    {
        TraceLoggingActivity<g_traceProvider> local;
        TraceLoggingWriteStart(local, "Overlay::InitializeCrosshair");

        // locate the resource in the dll.
        const HRSRC imageResHandle = FindResource(openxr_api_layer::dllModule, MAKEINTRESOURCE(CROSSHAIR_PNG), "PNG");
        if (!imageResHandle)
        {
            TraceLoggingWriteStop(local, "Overlay::InitializeCrosshair", TLArg(false, "FindResource"));
            return false;
        }

        // load the resource to the HGLOBAL
        const HGLOBAL imageResDataHandle = LoadResource(openxr_api_layer::dllModule, imageResHandle);
        if (!imageResDataHandle)
        {
            TraceLoggingWriteStop(local, "Overlay::InitializeCrosshair", TLArg(false, "LoadResource"));
            return false;
        }

        // lock the resource to retrieve memory pointer
        const void* pImageFile = LockResource(imageResDataHandle);
        if (!pImageFile)
        {
            TraceLoggingWriteStop(local, "Overlay::InitializeCrosshair", TLArg(false, "LockResource"));
            return false;
        }

        // calculate the size
        const DWORD imageFileSize = SizeofResource(openxr_api_layer::dllModule, imageResHandle);
        if (!imageFileSize)
        {
            TraceLoggingWriteStop(local, "Overlay::InitializeCrosshair", TLArg(false, "ImageFileSize"));
            return false;
        }

        // load image into texture
        ID3D11Device* const device = composition->getCompositionDevice()->getNativeDevice<graphics::D3D11>();
        auto image = std::make_unique<DirectX::ScratchImage>();
        CHECK_HRCMD(DirectX::LoadFromWICMemory(pImageFile, imageFileSize, DirectX::WIC_FLAGS_NONE, nullptr, *image));

        ComPtr<ID3D11Resource> crosshairTexture;
        CHECK_HRCMD(DirectX::CreateTexture(device,
                                           image->GetImages(),
                                           1,
                                           image->GetMetadata(),
                                           crosshairTexture.ReleaseAndGetAddressOf()));

        // create static swapchain
        XrSwapchainCreateInfo crosshairSwapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        crosshairSwapchainInfo.createFlags = XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT;
        crosshairSwapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        crosshairSwapchainInfo.arraySize = 1;
        crosshairSwapchainInfo.format = static_cast<int64_t>(image->GetMetadata().format);
        crosshairSwapchainInfo.width = static_cast<int32_t>(image->GetMetadata().width);
        crosshairSwapchainInfo.height = static_cast<int32_t>(image->GetMetadata().height);
        crosshairSwapchainInfo.mipCount = crosshairSwapchainInfo.sampleCount = crosshairSwapchainInfo.faceCount = 1;
        m_CrosshairSwapchain =
            composition->createSwapchain(crosshairSwapchainInfo,
                                         graphics::SwapchainMode::Write | graphics::SwapchainMode::Submit);
        if (!m_CrosshairSwapchain)
        {
            TraceLoggingWriteStop(local, "Overlay::InitializeCrosshair", TLArg(false, "CrosshairSwapchain"));
            return false;
        }

        // copy static content into swapchain image
        graphics::ISwapchainImage* const acquiredImage = m_CrosshairSwapchain->acquireImage();
        ID3D11DeviceContext* const context =
            composition->getCompositionDevice()->getNativeContext<graphics::D3D11>();
        ID3D11Texture2D* const surface = acquiredImage->getTextureForWrite()->getNativeTexture<graphics::D3D11>();
        context->CopyResource(surface, crosshairTexture.Get());
        m_CrosshairSwapchain->releaseImage();
        m_CrosshairSwapchain->commitLastReleasedImage();

        // initialize crosshair quad layer
        m_CrosshairLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        m_CrosshairLayer.subImage = m_CrosshairSwapchain->getSubImage();
        m_CrosshairLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        m_CrosshairLayer.space = viewSpace;
        ResetCrosshair();

        return true;
    }

    void Overlay::RenderMarkers(const XrCompositionLayerProjectionView& view,
                                uint32_t eye,
                                const XrPosef& refPose,
                                const XrPosef& trackerPose,
                                bool drawTracker,
                                ICompositionFramework* composition)
    {
        // perform actual rendering
        auto graphicsDevice = composition->getCompositionDevice();
        ID3D11Device* const device = composition->getCompositionDevice()->getNativeDevice<D3D11>();
        ID3D11DeviceContext* const context = composition->getCompositionDevice()->getNativeContext<D3D11>();

        // create ephemeral render target view for the drawing.
        auto renderTargetView = ComPtr<ID3D11RenderTargetView>();
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Format = m_Swapchains[view.subImage.swapchain].format;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.FirstArraySlice = 0;
        rtvDesc.Texture2D.MipSlice = D3D11CalcSubresource(0, 0, 1);
        CHECK_HRCMD(device->CreateRenderTargetView(m_Textures[eye].first->getNativeTexture<D3D11>(),
                                                   &rtvDesc,
                                                   set(renderTargetView)));

        // create ephemeral depth stencil view for depth testing / occlusion.
        auto depthStencilView = ComPtr<ID3D11DepthStencilView>();
        D3D11_DEPTH_STENCIL_VIEW_DESC depthDesc{};
        depthDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.Texture2DArray.ArraySize = 1;
        depthDesc.Texture2DArray.FirstArraySlice = 0;
        depthDesc.Texture2D.MipSlice = D3D11CalcSubresource(0, 0, 1);
        CHECK_HRCMD(device->CreateDepthStencilView(m_Textures[eye].second->getNativeTexture<D3D11>(),
                                                   &depthDesc,
                                                   set(depthStencilView)));

        context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), get(depthStencilView));

        // clear depth buffer
        context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);

        if (m_PassthroughActive)
        {
            // fill with magenta for chroma keyed passthrough
            constexpr float background[4] = {1.0f, 0.0f, 1.0f, 1.0f};
            context->ClearRenderTargetView(renderTargetView.Get(), background);
        }

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
        const XrRect2Di* imageRect = &view.subImage.imageRect;
        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = static_cast<float>(imageRect->offset.x);
        viewport.TopLeftY = static_cast<float>(imageRect->offset.y);
        viewport.Width = static_cast<float>(imageRect->extent.width);
        viewport.Height = static_cast<float>(imageRect->extent.height);
        viewport.MaxDepth = 1.0f;
        context->RSSetViewports(1, &viewport);
        DebugLog("overlay(%u) viewport: width = %d, height = %d, offset x: %d, offset y: %d",
                 eye,
                 imageRect->extent.width,
                 imageRect->extent.height,
                 imageRect->offset.x,
                 imageRect->offset.y);

        // draw reference/cor position
        graphicsDevice->draw(m_MeshRGB, refPose, m_MarkerSize);

        // draw tracker marker
        if (drawTracker)
        {
            graphicsDevice->draw(m_PassthroughActive? m_MeshCGY : m_MeshCMY, trackerPose, m_MarkerSize);
        }

        context->Flush();
    }

    std::vector<SimpleMeshVertex> Overlay::CreateMarker(bool reference, bool avoidMagenta)
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

        // right
        std::vector<SimpleMeshVertex> vertices = CreateMarkerMesh({tip, 0, 0},
                                                                  {point65, point05, 0},
                                                                  {point6, point1, 0},
                                                                  {bottom, 0, 0},
                                                                  reference      ? DarkRed
                                                                  : avoidMagenta ? DarkGrey
                                                                                 : DarkMagenta,
                                                                  reference      ? Red
                                                                  : avoidMagenta ? Grey
                                                                                 : Magenta,
                                                                  reference      ? LightRed
                                                                  : avoidMagenta ? LightGrey
                                                                                 : LightMagenta);
        // up
        std::vector<SimpleMeshVertex> top = CreateMarkerMesh({0, tip, 0},
                                                             {0, point65, point05},
                                                             {0, point6, point1},
                                                             {0, bottom, 0},
                                                             reference ? DarkBlue : DarkCyan,
                                                             reference ? Blue : Cyan,
                                                             reference ? LightBlue : LightCyan);
        // forward
        vertices.insert(vertices.end(), top.begin(), top.end());
        std::vector<SimpleMeshVertex> front = CreateMarkerMesh({0, 0, -tip},
                                                               {point05, 0, -point65},
                                                               {point1, 0, -point6},
                                                               {0, 0, -bottom},
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