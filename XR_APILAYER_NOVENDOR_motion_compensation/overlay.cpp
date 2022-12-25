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
        m_MenuSwapchainImages.clear();
    }
    void Overlay::CreateSession(const XrSessionCreateInfo* createInfo,
                                XrSession* session,
                                const std::string& runtimeName)
    {
        m_Initialized = true;
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
                        // Workaround: On Varjo, we must use intermediate textures with D3D11on12.
                        const bool enableVarjoQuirk = runtimeName.find("Varjo") != std::string::npos;
                        m_GraphicsDevice =
                            graphics::WrapD3D12Device(d3dBindings->device, d3dBindings->queue, enableVarjoQuirk);
                        break;
                    }

                    entry = entry->next;
                }

                if (m_GraphicsDevice)
                {
                    std::vector<uint16_t> indices;
                    graphics::copyFromArray(indices, graphics::c_cubeIndices);
                    std::vector<graphics::SimpleMeshVertex> vertices;
                    // graphics::copyFromArray(vertices, graphics::c_cubeBrightVertices);
                    graphics::copyFromArray(vertices, graphics::c_MarkerRGB);
                    m_MeshRGB = m_GraphicsDevice->createSimpleMesh(vertices, indices, "RGB Mesh");

                    graphics::copyFromArray(vertices, graphics::c_MarkerCMY);
                    m_MeshCMY = m_GraphicsDevice->createSimpleMesh(vertices, indices, "CMY Mesh");

                    uint32_t formatCount = 0;
                    CHECK_XRCMD(layer->xrEnumerateSwapchainFormats(*session, 0, &formatCount, nullptr));
                    std::vector<int64_t> formats(formatCount);
                    CHECK_XRCMD(
                        layer->xrEnumerateSwapchainFormats(*session, formatCount, &formatCount, formats.data()));

                    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                    swapchainInfo.width = swapchainInfo.height =
                        2048; // Let's hope the menu doesn't get bigger than that.
                    swapchainInfo.arraySize = 1;
                    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                    swapchainInfo.format = formats[0];
                    swapchainInfo.sampleCount = 1;
                    swapchainInfo.faceCount = 1;
                    swapchainInfo.mipCount = 1;
                    CHECK_XRCMD(layer->OpenXrApi::xrCreateSwapchain(*session, &swapchainInfo, &m_MenuSwapchain));
                    TraceLoggingWrite(g_traceProvider, "MenuSwapchain", TLPArg(m_MenuSwapchain, "Swapchain"));

                    uint32_t imageCount;
                    CHECK_XRCMD(layer->OpenXrApi::xrEnumerateSwapchainImages(m_MenuSwapchain, 0, &imageCount, nullptr));

                    graphics::SwapchainState swapchainState;
                    int64_t overrideFormat = 0;
                    if (m_GraphicsDevice->getApi() == graphics::Api::D3D11)
                    {
                        std::vector<XrSwapchainImageD3D11KHR> d3dImages(imageCount,
                                                                        {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                        CHECK_XRCMD(layer->OpenXrApi::xrEnumerateSwapchainImages(
                            m_MenuSwapchain,
                            imageCount,
                            &imageCount,
                            reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

                        for (uint32_t i = 0; i < imageCount; i++)
                        {
                            m_MenuSwapchainImages.push_back(
                                graphics::WrapD3D11Texture(m_GraphicsDevice,
                                                           swapchainInfo,
                                                           d3dImages[i].texture,
                                                           fmt::format("Menu swapchain {} TEX2D", i)));
                        }
                    }
                    else if (m_GraphicsDevice->getApi() == graphics::Api::D3D12)
                    {
                        std::vector<XrSwapchainImageD3D12KHR> d3dImages(imageCount,
                                                                        {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                        CHECK_XRCMD(layer->OpenXrApi::xrEnumerateSwapchainImages(
                            m_MenuSwapchain,
                            imageCount,
                            &imageCount,
                            reinterpret_cast<XrSwapchainImageBaseHeader*>(d3dImages.data())));

                        for (uint32_t i = 0; i < imageCount; i++)
                        {
                            m_MenuSwapchainImages.push_back(
                                graphics::WrapD3D12Texture(m_GraphicsDevice,
                                                           swapchainInfo,
                                                           d3dImages[i].texture,
                                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                           fmt::format("Menu swapchain {} TEX2D", i)));
                        }
                    }
                    else
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

    void Overlay::CreateSwapchain(XrSession session,
                                  const XrSwapchainCreateInfo* chainCreateInfo,
                                  const XrSwapchainCreateInfo* createInfo,
                                  XrSwapchain* swapchain,
                                  bool isDepth)
    {
        OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
        if (layer)
        {
            try
            {
                uint32_t imageCount;
                CHECK_XRCMD(layer->xrEnumerateSwapchainImages(*swapchain, 0, &imageCount, nullptr));

                graphics::SwapchainState swapchainState;
                int64_t overrideFormat = 0;
                if (m_GraphicsDevice->getApi() == graphics::Api::D3D11)
                {
                    std::vector<XrSwapchainImageD3D11KHR> d3dImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
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
                        images.runtimeTexture =
                            graphics::WrapD3D11Texture(m_GraphicsDevice,
                                                       *chainCreateInfo,
                                                       d3dImages[i].texture,
                                                       fmt::format("Runtime swapchain {} TEX2D", i));

                        swapchainState.images.push_back(std::move(images));
                    }
                }
                else if (m_GraphicsDevice->getApi() == graphics::Api::D3D12)
                {
                    std::vector<XrSwapchainImageD3D12KHR> d3dImages(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
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
                        if ((chainCreateInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT))
                        {
                            initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
                        }
                        else if ((chainCreateInfo->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
                        {
                            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
                        }

                        // Store the runtime images into the state (last entry in the processing chain).
                        images.runtimeTexture =
                            graphics::WrapD3D12Texture(m_GraphicsDevice,
                                                       *chainCreateInfo,
                                                       d3dImages[i].texture,
                                                       initialState,
                                                       fmt::format("Runtime swapchain {} TEX2D", i));

                        swapchainState.images.push_back(std::move(images));
                    }
                }
                else
                {
                    throw std::runtime_error("Unsupported graphics runtime");
                }

                for (uint32_t i = 0; i < imageCount; i++)
                {
                    graphics::SwapchainImages& images = swapchainState.images[i];

                    if (!isDepth)
                    {
                        // Create an app texture with the exact specification requested
                        XrSwapchainCreateInfo inputCreateInfo = *createInfo;

                        // Both post-processor and upscalers need to do sampling.
                        inputCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

                        images.appTexture = m_GraphicsDevice->createTexture(inputCreateInfo,
                                                                            fmt::format("App swapchain {} TEX2D", i),
                                                                            overrideFormat);
                    }
                    else
                    {
                        images.appTexture = images.runtimeTexture;
                    }
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
        else
        {
            ErrorLog("%s: unable to cast instance to OpenXrLayer\n", __FUNCTION__);
            m_Initialized = false;
        }
    }

    void Overlay::DestroySwapchain(XrSwapchain swapchain)
    {
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
            return false;
        }
        m_OverlayActive = !m_OverlayActive;
        return true;
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
                uint32_t sliceForOverlay[graphics::ViewCount];
                std::shared_ptr<graphics::ITexture> depthForOverlay[graphics::ViewCount]{};
                xr::math::ViewProjection viewForOverlay[graphics::ViewCount];
                XrRect2Di viewportForOverlay[graphics::ViewCount];

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

                                        depthBuffer = depthSwapchainState.images[depthSwapchainState.acquiredImageIndex]
                                                          .appTexture;
                                        nearFar.Near = depth->nearZ;
                                        nearFar.Far = depth->farZ;
                                    }
                                    break;
                                }
                                entry = entry->next;
                            }

                            textureForOverlay[eye] = swapchainImages.runtimeTexture;
                            sliceForOverlay[eye] = view.subImage.imageArrayIndex;
                            depthForOverlay[eye] = depthBuffer;

                            viewForOverlay[eye].Pose = view.pose;
                            viewForOverlay[eye].Fov = view.fov;
                            viewForOverlay[eye].NearFar = nearFar;
                            viewportForOverlay[eye] = view.subImage.imageRect;
                        }
                    }
                }

                if (textureForOverlay[0])
                {
                    const bool useTextureArrays =
                        textureForOverlay[1] == textureForOverlay[0] && sliceForOverlay[0] != sliceForOverlay[1];

                    // render the tracker pose(s)
                    for (uint32_t eye = 0; eye < graphics::ViewCount; eye++)
                    {
                        m_GraphicsDevice->setRenderTargets(
                            1,
                            &textureForOverlay[eye],
                            useTextureArrays ? reinterpret_cast<int32_t*>(&sliceForOverlay[eye]) : nullptr,
                            &viewportForOverlay[eye],
                            depthForOverlay[eye] ,
                            useTextureArrays ? eye : -1);

                        m_GraphicsDevice->setViewProjection(viewForOverlay[eye]);

                        XrVector3f scaling{0.01f, 0.01f, 0.01f};
                        if (mcActivated)
                        {
                            m_GraphicsDevice->draw(
                                m_MeshCMY,
                                xr::math::Pose::Multiply(referenceTrackerPose,
                                                         xr::math::Pose::Invert(reversedManipulation)),
                                scaling);
                        }

                        m_GraphicsDevice->draw(m_MeshRGB, referenceTrackerPose, scaling);
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