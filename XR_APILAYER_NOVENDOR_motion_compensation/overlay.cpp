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
#include "shader_utilities.h"
#include "d3dcommon.h"
#include <util.h>
#include <log.h>

using namespace motion_compensation_layer;
using namespace motion_compensation_layer::log;

namespace graphics
{
    Overlay::~Overlay()
    {
        m_Swapchains.clear();
    }
    void Overlay::CreateSession(const XrSessionCreateInfo* createInfo,
                                XrSession* session,
                                const std::string& runtimeName)
    {
        m_Initialized = true;
        m_OwnDepthBuffers.clear();

        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            try
            {
                // Get the graphics device.
                const XrBaseInStructure* entry = reinterpret_cast<const XrBaseInStructure*>(createInfo->next);
                while (entry)
                {
                    if (entry->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR)
                    {
                        TraceLoggingWrite(g_traceProvider, "UseD3D11");

                        const XrGraphicsBindingD3D11KHR* d3dBindings =
                            reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(entry);
                        // Workaround: On Oculus, we must delay the initialization of Detour.
                        const bool enableOculusQuirk = runtimeName.find("Oculus") != std::string::npos;
                        m_GraphicsDevice = graphics::WrapD3D11Device(d3dBindings->device, enableOculusQuirk);
                        break;
                    }
                    else if (entry->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR)
                    {
                        TraceLoggingWrite(g_traceProvider, "UseD3D12");

                        const XrGraphicsBindingD3D12KHR* d3dBindings =
                            reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(entry);
                        m_GraphicsDevice =
                            graphics::WrapD3D12Device(d3dBindings->device, d3dBindings->queue);
                        break;
                    }

                    entry = entry->next;
                }

                if (m_GraphicsDevice)
                {
                    std::vector<graphics::SimpleMeshVertex> vertices = CreateMarker(true);
                    std::vector<uint16_t> indices = CreateIndices(vertices.size());
                    m_MeshRGB = m_GraphicsDevice->createSimpleMesh(vertices, indices, "RGB Mesh");
                    vertices = CreateMarker(false);
                    m_MeshCMY = m_GraphicsDevice->createSimpleMesh(vertices, indices, "CMY Mesh");
                    
                    if (m_GraphicsDevice->getApi() != graphics::Api::D3D11 &&
                        m_GraphicsDevice->getApi() != graphics::Api::D3D12)
                    {
                        throw std::runtime_error("Unsupported graphics runtime");
                    }
                }
                else
                {
                    Log("Unsupported graphics runtime.\n");
                }
            }
            catch (std::exception e)
            {
                ErrorLog("%s: encountered exception: %s", __FUNCTION__, e.what());
                m_Initialized = false;
            }
        }
        else
        {
            ErrorLog("%s: unable to cast instance to OpenXrLayer\n", __FUNCTION__);
            m_Initialized = false;
        }
    }

    void Overlay::DestroySession()
    {
        if (m_GraphicsDevice)
        {
            m_GraphicsDevice->blockCallbacks();
            m_GraphicsDevice->flushContext(true);
        }
        m_Swapchains.clear();
        m_OwnDepthBuffers.clear();
        m_MeshRGB.reset();
        m_MeshCMY.reset();
        if (m_GraphicsDevice)
        {
            m_GraphicsDevice->shutdown();
        }
        m_GraphicsDevice.reset();
        // A good check to ensure there are no resources leak is to confirm that the graphics device is
        // destroyed _before_ we see this message.
        // eg:
        // 2022-01-01 17:15:35 -0800: D3D11Device destroyed
        // 2022-01-01 17:15:35 -0800: Session destroyed
        // If the order is reversed or the Device is destructed missing, then it means that we are not cleaning
        // up the resources properly.
        Log("Session destroyed\n");
    }

    void Overlay::CreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain)
    {
        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            // Identify the swapchains of interest for our processing chain.
            if (createInfo->usageFlags &
                (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))

            {
                try
                {
                    uint32_t imageCount;
                    CHECK_XRCMD(layer->xrEnumerateSwapchainImages(*swapchain, 0, &imageCount, nullptr));

                    graphics::SwapchainState swapchainState;
                    int64_t overrideFormat = 0;
                    if (m_GraphicsDevice->getApi() == graphics::Api::D3D11)
                    {
                        std::vector<XrSwapchainImageD3D11KHR> d3dImages(imageCount,
                                                                        {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                        CHECK_XRCMD(layer->OpenXrApi::xrEnumerateSwapchainImages(
                            *swapchain,
                            imageCount,
                            &imageCount,
                            reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

                        // Dump the descriptor for the first texture returned by the runtime for debug purposes.
                        {
                            D3D11_TEXTURE2D_DESC desc;
                            d3dImages[0].texture->GetDesc(&desc);
                            TraceLoggingWrite(g_traceProvider,
                                              "RuntimeSwapchain",
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

                            // Make sure to create the app texture typeless.
                            overrideFormat = (int64_t)desc.Format;
                        }

                        for (uint32_t i = 0; i < imageCount; i++)
                        {
                            graphics::SwapchainImages images;

                            // Store the runtime images into the state (last entry in the processing chain).
                            images.chain.push_back(
                                graphics::WrapD3D11Texture(m_GraphicsDevice,
                                                           *createInfo,
                                                           d3dImages[i].texture,
                                                           fmt::format("Runtime swapchain {} TEX2D", i)));

                            swapchainState.images.push_back(std::move(images));
                        }
                    }
                    else if (m_GraphicsDevice->getApi() == graphics::Api::D3D12)
                    {
                        std::vector<XrSwapchainImageD3D12KHR> d3dImages(imageCount,
                                                                        {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                        CHECK_XRCMD(layer->OpenXrApi::xrEnumerateSwapchainImages(
                            *swapchain,
                            imageCount,
                            &imageCount,
                            reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

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

                            // Make sure to create the app texture typeless.
                            overrideFormat = (int64_t)desc.Format;
                        }

                        for (uint32_t i = 0; i < imageCount; i++)
                        {
                            graphics::SwapchainImages images;

                            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
                            if ((createInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT))
                            {
                                initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
                            }
                            else if ((createInfo->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
                            {
                                initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
                            }

                            // Store the runtime images into the state (last entry in the processing chain).
                            images.chain.push_back(
                                graphics::WrapD3D12Texture(m_GraphicsDevice,
                                                           *createInfo,
                                                           d3dImages[i].texture,
                                                           fmt::format("Runtime swapchain {} TEX2D", i)));

                            swapchainState.images.push_back(std::move(images));
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Unsupported graphics runtime");
                    }

                    if (m_OwnDepthBuffers.find(*swapchain) == m_OwnDepthBuffers.cend())
                    {
                        XrSwapchainCreateInfo depthInfo = *createInfo;
                        depthInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

                        std::shared_ptr<ITexture> texture =
                            m_GraphicsDevice->createTexture(depthInfo, "depth buffer", DXGI_FORMAT_D32_FLOAT);
                        m_OwnDepthBuffers.insert_or_assign(*swapchain, texture);
                    }

                    m_Swapchains.insert_or_assign(*swapchain, swapchainState);

                    TraceLoggingWrite(g_traceProvider, "xrCreateSwapchain", TLPArg(*swapchain, "Swapchain"));
                }
                catch (std::exception e)
                {
                    ErrorLog("%s: encountered exception: %s", __FUNCTION__, e.what());
                    m_Initialized = false;
                }
            }
        }
        else
        {
            ErrorLog("%s: unable to cast instance to OpenXrLayer\n", __FUNCTION__);
            m_Initialized = false;
        }
    }

    void Overlay::DestroySwapchain(XrSwapchain swapchain)
    {
        m_OwnDepthBuffers.erase(swapchain);
        m_Swapchains.erase(swapchain);
    }

    XrResult Overlay::AcquireSwapchainImage(XrSwapchain swapchain,
                                                  const XrSwapchainImageAcquireInfo* acquireInfo,
                                                  uint32_t* index)
    {
        
        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            auto swapchainIt = m_Swapchains.find(swapchain);
            if (swapchainIt != m_Swapchains.end())
            {
                // Perform the release now in case it was delayed.
                if (swapchainIt->second.delayedRelease)
                {
                    TraceLoggingWrite(g_traceProvider, "ForcedSwapchainRelease", TLPArg(swapchain, "Swapchain"));

                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, nullptr};
                    swapchainIt->second.delayedRelease = false;
                    CHECK_XRCMD(layer->OpenXrApi::xrReleaseSwapchainImage(swapchain, &releaseInfo));
                }
            }

            const XrResult result = layer->OpenXrApi::xrAcquireSwapchainImage(swapchain, acquireInfo, index);
            if (XR_SUCCEEDED(result))
            {
                // Record the index so we know which texture to use in xrEndFrame().
                if (swapchainIt != m_Swapchains.end())
                {
                    swapchainIt->second.acquiredImageIndex = *index;
                }

                TraceLoggingWrite(g_traceProvider, "xrAcquireSwapchainImage", TLArg(*index, "Index"));
            }

            return result;
        }
        else
        {
            ErrorLog("%s: unable to cast instance to OpenXrLayer\n", __FUNCTION__);
            return XR_ERROR_INSTANCE_LOST;
        }

    }

    XrResult Overlay::ReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo)
    {
        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            auto swapchainIt = m_Swapchains.find(swapchain);
            if (swapchainIt != m_Swapchains.end())
            {
                // Perform a delayed release: we still need to write to the swapchain in our xrEndFrame()!
                swapchainIt->second.delayedRelease = true;
                return XR_SUCCESS;
            }

            return layer->OpenXrApi::xrReleaseSwapchainImage(swapchain, releaseInfo);
        }
        else
        {
            ErrorLog("%s: unable to cast instance to OpenXrLayer\n", __FUNCTION__);
            return XR_ERROR_INSTANCE_LOST;
        }
    }
    bool Overlay::ToggleOverlay()
    {
        if (!m_Initialized)
        {
            m_OverlayActive = false;
            GetAudioOut()->Execute(Feedback::Event::Error);
            return false;
        }
        m_OverlayActive = !m_OverlayActive;
        GetAudioOut()->Execute(m_OverlayActive ? Feedback::Event::OverlayOn : Feedback::Event::OverlayOff);
        return true;
    }

    void Overlay::BeginFrameBefore()
    {
        // Release the swapchain images. Some runtimes don't seem to look cross-frame releasing and this can happen
        // when a frame is discarded.
        for (auto& swapchain : m_Swapchains)
        {
            if (swapchain.second.delayedRelease)
            {
                TraceLoggingWrite(g_traceProvider, "ForcedSwapchainRelease", TLPArg(swapchain.first, "Swapchain"));

                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                swapchain.second.delayedRelease = false;
                CHECK_XRCMD(GetInstance()->xrReleaseSwapchainImage(swapchain.first, &releaseInfo));
            }
        }
    }

    void Overlay::BeginFrameAfter()
    {
        if (m_GraphicsDevice)
        {
            // With D3D12, we want to make sure the query is enqueued now.
            if (m_GraphicsDevice->getApi() == graphics::Api::D3D12)
            {
                m_GraphicsDevice->flushContext();
            }
        }
    }

    void Overlay::DrawOverlay(XrFrameEndInfo* chainFrameEndInfo,
                              const XrPosef& referenceTrackerPose,
                              const XrPosef& reversedManipulation,
                              bool mcActivated)
    {
        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            if (m_OverlayActive && m_GraphicsDevice)
            {
                m_GraphicsDevice->resolveQueries();
                m_GraphicsDevice->blockCallbacks();
                m_GraphicsDevice->saveContext();
                m_GraphicsDevice->unsetRenderTargets();

                std::shared_ptr<graphics::ITexture> textureForOverlay[graphics::ViewCount]{};
                uint32_t sliceForOverlay[graphics::ViewCount]{};
                std::shared_ptr<graphics::ITexture> depthForOverlay[graphics::ViewCount]{};
                xr::math::ViewProjection viewForOverlay[graphics::ViewCount]{};

                std::vector<XrCompositionLayerProjection> layerProjectionAllocator;
                std::vector<std::array<XrCompositionLayerProjectionView, 2>> layerProjectionViewsAllocator;

                for (uint32_t i = 0; i < chainFrameEndInfo->layerCount; i++)
                {
                    if (chainFrameEndInfo->layers[i]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
                    {
                        const XrCompositionLayerProjection* proj =
                            reinterpret_cast<const XrCompositionLayerProjection*>(chainFrameEndInfo->layers[i]);
                        for (uint32_t eye = 0; eye < graphics::ViewCount; eye++)
                        {
                            const XrCompositionLayerProjectionView& view = proj->views[eye];

                            auto swapchainIt = m_Swapchains.find(view.subImage.swapchain);
                            if (swapchainIt == m_Swapchains.end())
                            {
                                throw std::runtime_error("Swapchain is not registered");
                            }
                            auto& swapchainState = swapchainIt->second;
                            auto& swapchainImages = swapchainState.images[swapchainState.acquiredImageIndex];

                            // Look for the depth buffer.
                            std::shared_ptr<graphics::ITexture> depthBuffer;
                            xr::math::NearFar nearFar{0.001f, 100.f};
                            const XrBaseInStructure* entry = reinterpret_cast<const XrBaseInStructure*>(view.next);
                            while (entry)
                            {
                                if (entry->type == XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR)
                                {
                                    DebugLog("Depthbuffer found\n ");
                                    const XrCompositionLayerDepthInfoKHR* depth =
                                        reinterpret_cast<const XrCompositionLayerDepthInfoKHR*>(entry);

                                    TraceLoggingWrite(
                                        g_traceProvider,
                                        "xrEndFrame_View",
                                        TLArg("Depth", "Type"),
                                        TLArg(eye, "Index"),
                                        TLPArg(depth->subImage.swapchain, "Swapchain"),
                                        TLArg(depth->subImage.imageArrayIndex, "ImageArrayIndex"),
                                        TLArg(xr::ToString(depth->subImage.imageRect).c_str(), "ImageRect"),
                                        TLArg(depth->nearZ, "Near"),
                                        TLArg(depth->farZ, "Far"),
                                        TLArg(depth->minDepth, "MinDepth"),
                                        TLArg(depth->maxDepth, "MaxDepth"));

                                    // The order of color/depth textures must match.
                                    if (depth->subImage.imageArrayIndex == view.subImage.imageArrayIndex)
                                    {
                                        auto depthSwapchainIt = m_Swapchains.find(depth->subImage.swapchain);
                                        if (depthSwapchainIt == m_Swapchains.end())
                                        {
                                            throw std::runtime_error("Swapchain is not registered");
                                        }
                                        auto& depthSwapchainState = depthSwapchainIt->second;

                                        assert(depthSwapchainState.images[depthSwapchainState.acquiredImageIndex]
                                                   .chain.size() == 1);
                                        depthBuffer =
                                            depthSwapchainState.images[depthSwapchainState.acquiredImageIndex].chain[0];
                                        nearFar.Near = depth->nearZ;
                                        nearFar.Far = depth->farZ;
                                    }
                                    break;
                                }
                                entry = entry->next;
                            }

                            textureForOverlay[eye] = swapchainImages.chain.back();
                            sliceForOverlay[eye] = view.subImage.imageArrayIndex;
                            auto ownDepthBufferIt = m_OwnDepthBuffers.find(swapchainIt->first);
                            depthForOverlay[eye] = depthBuffer ? depthBuffer
                                                   : ownDepthBufferIt != m_OwnDepthBuffers.cend()
                                                       ? ownDepthBufferIt->second
                                                       : nullptr;
                            viewForOverlay[eye].Pose = view.pose;
                            viewForOverlay[eye].Fov = view.fov;
                            viewForOverlay[eye].NearFar = nearFar;
                        }
                    }
                }

                if (textureForOverlay[0])
                {
                    const bool useVPRT = textureForOverlay[1] == textureForOverlay[0];

                    // render the tracker pose(s)
                    for (uint32_t eye = 0; eye < graphics::ViewCount; eye++)
                    {
                        m_GraphicsDevice->setRenderTargets(1,
                                                           &textureForOverlay[eye],
                                                           useVPRT ? reinterpret_cast<int32_t*>(&sliceForOverlay[eye])
                                                                   : nullptr,
                                                           depthForOverlay[eye],
                                                           useVPRT ? eye : -1);
                        m_GraphicsDevice->setViewProjection(viewForOverlay[eye]);
                        m_GraphicsDevice->clearDepth(1.f);

                        XrVector3f scaling{0.02f, 0.02f, 0.02f};
                        if (mcActivated)
                        {
                            m_GraphicsDevice->draw(
                                m_MeshRGB,
                                xr::math::Pose::Multiply(referenceTrackerPose,
                                                         xr::math::Pose::Invert(reversedManipulation)),
                                scaling);
                        }

                        m_GraphicsDevice->draw(mcActivated ? m_MeshCMY : m_MeshRGB, referenceTrackerPose, scaling);
                    }

                    m_GraphicsDevice->unsetRenderTargets();
                }
                m_GraphicsDevice->restoreContext();
                m_GraphicsDevice->flushContext(false, true);  
            }

            // Release the swapchain images now, as we are really done this time.
            for (auto& swapchain : m_Swapchains)
            {
                if (swapchain.second.delayedRelease)
                {
                    TraceLoggingWrite(g_traceProvider, "DelayedSwapchainRelease", TLPArg(swapchain.first, "Swapchain"));

                    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    swapchain.second.delayedRelease = false;
                    CHECK_XRCMD(layer->OpenXrApi::xrReleaseSwapchainImage(swapchain.first, &releaseInfo));
                }
            }
        }
        else
        {
            ErrorLog("%s: unable to cast instance to OpenXrLayer\n", __FUNCTION__);
        }
    }

    void Overlay::UnblockCallbacks()
    {
        if (m_GraphicsDevice)
        {
            m_GraphicsDevice->unblockCallbacks();
        }
    }

    std::vector<SimpleMeshVertex> Overlay::CreateMarker(bool rgb)
    {
        std::vector<SimpleMeshVertex> vertices;
        vertices = CreateConeMesh({-4.f, 0.f, 0.f},
                                  {-1.5f, 0.5f, 0.f},
                                  {0.f, 0.f, 0.f},
                                  rgb ? DarkRed : DarkMagenta,
                                  rgb ? Red : Magenta,
                                  rgb ? LightRed : LightMagenta);
        std::vector<SimpleMeshVertex> top = CreateConeMesh({0.f, 4.f, 0.f},
                                                            {0.f, 1.5f, 0.5f},
                                                            {0.f, 0.f, 0.f},
                                                            rgb ? DarkBlue : DarkCyan,
                                                            rgb ? Blue : Cyan,
                                                            rgb ? LightBlue : LightCyan);
        vertices.insert(vertices.end(), top.begin(), top.end());
        std::vector<SimpleMeshVertex> front = CreateConeMesh({0.f, 0.f, 4.f},
                                                           {0.5f, 0.f, 1.5f},
                                                           {0.f, 0.f, 0.f},
                                                           rgb ? DarkGreen : DarkYellow,
                                                           rgb ? Green : Yellow,
                                                             rgb ? LightGreen : LightYellow);
        vertices.insert(vertices.end(), front.begin(), front.end());
        return vertices;
    }

    std::vector<SimpleMeshVertex> Overlay::CreateConeMesh(XrVector3f top,
                                                          XrVector3f side,
                                                          XrVector3f offset,
                                                          XrVector3f topColor,
                                                          XrVector3f sideColor,
                                                          XrVector3f bottomColor)
    {
        std::vector<SimpleMeshVertex> vertices;
        const DirectX::XMVECTOR dxTop = xr::math::LoadXrVector3(top);
        const DirectX::XMVECTOR dxOffset = xr::math::LoadXrVector3(offset);
        XrVector3f xrTop;;
        xr::math::StoreXrVector3(&xrTop, DirectX::XMVectorAdd(dxTop, dxOffset));float angleIncrement = DirectX::XM_2PI / 32.f;
        xr::math::StoreXrVector3(&xrTop, DirectX::XMVectorAdd(dxTop, dxOffset));angleIncrement = DirectX::XM_2PI / 32.f;
        DirectX::XMVECTOR rotation = DirectX::XMQuaternionRotationAxis(dxTop, angleIncrement);
        DirectX::XMVECTOR side1 = xr::math::LoadXrVector3(side);
        XrVector3f xrSide0, xrSide1;
        for (int i = 0; i < 32; i++)
        {
            DirectX::XMVECTOR side0 = side1;
            side1 = DirectX::XMVector3Rotate(side0, rotation);
            xr::math::StoreXrVector3(&xrSide0, DirectX::XMVectorAdd(side0, dxOffset));
            xr::math::StoreXrVector3(&xrSide1, DirectX::XMVectorAdd(side1, dxOffset));
            //xr::math::Pose::Multiply DirectX::XMVECTOR side1 = base;
  
            // bottom
            vertices.push_back({offset, bottomColor});
            vertices.push_back({xrSide0, sideColor});
            vertices.push_back({xrSide1, sideColor});

            // top 
            vertices.push_back({xrTop, topColor});
            vertices.push_back({xrSide1, sideColor});
            vertices.push_back({xrSide0, sideColor});
        }
        return vertices;
    }

    std::vector<unsigned short> Overlay::CreateIndices(size_t amount)
    {
        std::vector<unsigned short> indices;
        for (unsigned short i = 0; i < amount; i++)
        {
            indices.push_back(i);
        }
        return indices;
    }

    namespace shader
    {
        void CompileShader(const std::filesystem::path& shaderFile,
                           const char* entryPoint,
                           ID3DBlob** blob,
                           const D3D_SHADER_MACRO* defines /*= nullptr*/,
                           ID3DInclude* includes /* = nullptr*/,
                           const char* target /* = "cs_5_0"*/)
        {
            DWORD flags =
                D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
            flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
            flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
            if (!includes)
            {
                includes = D3D_COMPILE_STANDARD_FILE_INCLUDE;
            }

            ComPtr<ID3DBlob> cdErrorBlob;
            const HRESULT hr = D3DCompileFromFile(shaderFile.c_str(),
                                                  defines,
                                                  includes,
                                                  entryPoint,
                                                  target,
                                                  flags,
                                                  0,
                                                  blob,
                                                  &cdErrorBlob);

            if (FAILED(hr))
            {
                if (cdErrorBlob)
                {
                    Log("%s", (char*)cdErrorBlob->GetBufferPointer());
                }
                CHECK_HRESULT(hr, "Failed to compile shader file");
            }
        }

        void CompileShader(const void* data,
                           size_t size,
                           const char* entryPoint,
                           ID3DBlob** blob,
                           const D3D_SHADER_MACRO* defines /*= nullptr*/,
                           ID3DInclude* includes /* = nullptr*/,
                           const char* target /* = "cs_5_0"*/)
        {
            DWORD flags =
                D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
            flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
            flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
            if (!includes)
            {
                // TODO: pSourceName must be a file name to derive relative paths from.
                includes = D3D_COMPILE_STANDARD_FILE_INCLUDE;
            }

            ComPtr<ID3DBlob> cdErrorBlob;
            const HRESULT hr =
                D3DCompile(data, size, nullptr, defines, includes, entryPoint, target, flags, 0, blob, &cdErrorBlob);

            if (FAILED(hr))
            {
                if (cdErrorBlob)
                {
                    Log("%s", (char*)cdErrorBlob->GetBufferPointer());
                }
                CHECK_HRESULT(hr, "Failed to compile shader");
            }
        }

        HRESULT IncludeHeader::Open(D3D_INCLUDE_TYPE /*includeType*/,
                                    LPCSTR pFileName,
                                    LPCVOID /*pParentData*/,
                                    LPCVOID* ppData,
                                    UINT* pBytes)
        {
            for (auto& it : m_includePaths)
            {
                auto path = it / pFileName;
                auto file = std::ifstream(path, std::ios_base::binary);
                if (file.is_open())
                {
                    assert(ppData && pBytes);
                    m_data.push_back({});
                    auto& buf = m_data.back();
                    buf.resize(static_cast<size_t>(std::filesystem::file_size(path)));
                    file.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                    buf.erase(std::remove(buf.begin(), buf.end(), '\0'), buf.end());
                    *ppData = buf.data();
                    *pBytes = static_cast<UINT>(buf.size());
                    return S_OK;
                }
            }
            throw std::runtime_error("Error opening shader file include header");
        }

        HRESULT IncludeHeader::Close(LPCVOID pData)
        {
            return S_OK;
        }

        const D3D_SHADER_MACRO* Defines::get() const
        {
            static const D3D_SHADER_MACRO kEmpty = {nullptr, nullptr};
            if (!m_definesVector.empty())
            {
                m_defines = std::make_unique<D3D_SHADER_MACRO[]>(m_definesVector.size() + 1);
                for (size_t i = 0; i < m_definesVector.size(); ++i)
                    m_defines[i] = {m_definesVector[i].first.c_str(), m_definesVector[i].second.c_str()};
                m_defines[m_definesVector.size()] = kEmpty;
                return m_defines.get();
            }
            m_defines = nullptr;
            return &kEmpty;
        }
    } // namespace shader
} // namespace graphics