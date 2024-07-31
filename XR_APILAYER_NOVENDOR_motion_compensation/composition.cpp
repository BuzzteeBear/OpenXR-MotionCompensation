// MIT License
//
// Copyright(c) 2022-2023 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
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

#include "graphics.h"
#include "log.h"
#include "dxgidebug.h"

namespace xr {

    using namespace openxr_api_layer::graphics;

    static inline std::string ToString(Api api) {
        switch (api) {
        case Api::D3D11:
            return "D3D11";
        case Api::D3D12:
            return "D3D12";
        };

        return "";
    }

    static inline std::string ToString(CompositionApi api) {
        switch (api) {
        case CompositionApi::D3D11:
            return "D3D11";
        };

        return "";
    }

} // namespace xr

namespace
{
    using namespace openxr_api_layer::log;
    using namespace openxr_api_layer::graphics;

    bool isSRGBFormat(DXGI_FORMAT format)
    {
        return format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
               format == DXGI_FORMAT_B8G8R8X8_UNORM_SRGB || format == DXGI_FORMAT_BC1_UNORM_SRGB ||
               format == DXGI_FORMAT_BC2_UNORM_SRGB || format == DXGI_FORMAT_BC3_UNORM_SRGB ||
               format == DXGI_FORMAT_BC7_UNORM_SRGB;
    }

    bool isDepthFormat(DXGI_FORMAT format)
    {
        return format == DXGI_FORMAT_D16_UNORM || format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
               format == DXGI_FORMAT_D32_FLOAT || format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    }

    struct SwapchainImage : ISwapchainImage
    {
        SwapchainImage(std::shared_ptr<IGraphicsTexture> textureOnApplicationDevice,
                       std::shared_ptr<IGraphicsTexture> textureOnCompositionDevice,
                       uint32_t index)
            : m_textureOnApplicationDevice(textureOnApplicationDevice), m_textureForRead(textureOnCompositionDevice),
              m_textureForWrite(textureOnCompositionDevice), m_index(index)
        {}

        IGraphicsTexture* getApplicationTexture() const override
        {
            return m_textureOnApplicationDevice.get();
        }

        IGraphicsTexture* getTextureForRead() const override
        {
            return m_textureForRead.get();
        }

        IGraphicsTexture* getTextureForWrite() const override
        {
            return m_textureForWrite.get();
        }

        uint32_t getIndex() const override
        {
            return m_index;
        }

        const std::shared_ptr<IGraphicsTexture> m_textureOnApplicationDevice;
        const std::shared_ptr<IGraphicsTexture> m_textureForRead;
        const std::shared_ptr<IGraphicsTexture> m_textureForWrite;
        const uint32_t m_index;
    };

    struct SubmittableSwapchain : ISwapchain
    {
        SubmittableSwapchain(PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr,
                             XrInstance instance,
                             XrSwapchain swapchain,
                             const XrSwapchainCreateInfo& infoOnApplicationDevice,
                             IGraphicsDevice* applicationDevice,
                             IGraphicsDevice* compositionDevice,
                             SwapchainMode mode,
                             std::optional<bool> overrideShareable = {},
                             bool hasOwnership = true)
            : m_swapchain(swapchain), m_formatOnApplicationDevice(infoOnApplicationDevice.format),
              m_compositionDevice(compositionDevice), m_applicationDevice(applicationDevice),
              m_accessForRead((mode & SwapchainMode::Read) == SwapchainMode::Read),
              m_accessForWrite((mode & SwapchainMode::Write) == SwapchainMode::Write),
              m_infoOnCompositionDevice(infoOnApplicationDevice)
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "Swapchain_Create",
                                   TLArg("Submittable", "Type"),
                                   TLArg(hasOwnership, "HasOwnership"));

            CHECK_XRCMD(xrGetInstanceProcAddr(instance,
                                              "xrAcquireSwapchainImage",
                                              reinterpret_cast<PFN_xrVoidFunction*>(&xrAcquireSwapchainImage)));
            CHECK_XRCMD(xrGetInstanceProcAddr(instance,
                                              "xrWaitSwapchainImage",
                                              reinterpret_cast<PFN_xrVoidFunction*>(&xrWaitSwapchainImage)));
            CHECK_XRCMD(xrGetInstanceProcAddr(instance,
                                              "xrReleaseSwapchainImage",
                                              reinterpret_cast<PFN_xrVoidFunction*>(&xrReleaseSwapchainImage)));
            CHECK_XRCMD(xrGetInstanceProcAddr(instance,
                                              "xrEnumerateSwapchainImages",
                                              reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateSwapchainImages)));
            if (hasOwnership)
            {
                CHECK_XRCMD(xrGetInstanceProcAddr(instance,
                                                  "xrDestroySwapchain",
                                                  reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroySwapchain)));
            }

            // Translate from the app device format to the composition device format.
            m_infoOnCompositionDevice.format = m_compositionDevice->translateFromGenericFormat(
                m_applicationDevice->translateToGenericFormat(m_infoOnCompositionDevice.format));

            // Enumerate all the swapchain images.
            uint32_t imagesCount;
            CHECK_XRCMD(xrEnumerateSwapchainImages(m_swapchain, 0, &imagesCount, nullptr));
            std::vector<std::shared_ptr<IGraphicsTexture>> textures;
            switch (m_applicationDevice->getApi())
            {
            case Api::D3D11: {
                std::vector<XrSwapchainImageD3D11KHR> images(imagesCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
                CHECK_XRCMD(xrEnumerateSwapchainImages(m_swapchain,
                                                       imagesCount,
                                                       &imagesCount,
                                                       reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
                for (const XrSwapchainImageD3D11KHR& image : images)
                {
                    textures.push_back(m_applicationDevice->openTexture<D3D11>(image.texture, infoOnApplicationDevice));
                }
            }
            break;
            case Api::D3D12: {
                std::vector<XrSwapchainImageD3D12KHR> images(imagesCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
                CHECK_XRCMD(xrEnumerateSwapchainImages(m_swapchain,
                                                       imagesCount,
                                                       &imagesCount,
                                                       reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
                for (const XrSwapchainImageD3D12KHR& image : images)
                {
                    textures.push_back(m_applicationDevice->openTexture<D3D12>(image.texture, infoOnApplicationDevice));
                }
            }
            break;
            default:
                throw std::runtime_error("Composition graphics API is not supported");
            }

            // Make the images available on the composition device.
            uint32_t index = 0;
            for (std::shared_ptr<IGraphicsTexture>& textureOnApplicationDevice : textures)
            {
                std::unique_ptr<SwapchainImage> image;
                if (overrideShareable.value_or(true) && textureOnApplicationDevice->isShareable())
                {
                    const std::shared_ptr<IGraphicsTexture> textureOnCompositionDevice =
                        m_compositionDevice->openTexture(textureOnApplicationDevice->getTextureHandle());
                    image =
                        std::make_unique<SwapchainImage>(textureOnApplicationDevice, textureOnCompositionDevice, index);
                }
                else
                {
                    // If the swapchain image isn't shareable, we will need to create a copy accessible on both the
                    // application and composition device, and make sure to perform copy operations as needed.
                    // TODO: Reduce memory occupation by using a shared texture at the IGraphicsDevice level.
                    if (!m_bounceBufferOnApplicationDevice)
                    {
                        m_bounceBufferOnCompositionDevice =
                            m_compositionDevice->createTexture(m_infoOnCompositionDevice, true /* shareable */);
                        m_bounceBufferOnApplicationDevice =
                            m_applicationDevice->openTexture(m_bounceBufferOnCompositionDevice->getTextureHandle());
                    }
                    image = std::make_unique<SwapchainImage>(textureOnApplicationDevice,
                                                             m_bounceBufferOnCompositionDevice,
                                                             index);
                }

                TraceLoggingWriteTagged(local, "Swapchain_Create", TLPArg(image.get(), "Image"));

                m_images.push_back(std::move(image));
                index++;
            }

            // A fence to be used to synchronize between the application/runtime and the composition device.
            m_fenceOnCompositionDevice = m_compositionDevice->createFence();
            m_fenceOnApplicationDevice = m_applicationDevice->openFence(m_fenceOnCompositionDevice->getFenceHandle());

            TraceLoggingWriteStop(local, "Swapchain_Create", TLPArg(this, "Swapchain"));
        }

        ~SubmittableSwapchain() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_Destroy", TLPArg(this, "Swapchain"));

            if (m_fenceOnApplicationDevice)
            {
                m_fenceOnApplicationDevice->waitOnCpu(m_fenceValue);
            }
            if (m_fenceOnCompositionDevice)
            {
                m_fenceOnCompositionDevice->waitOnCpu(m_fenceValue);
            }
            if (xrDestroySwapchain)
            {
                xrDestroySwapchain(m_swapchain);
            }

            TraceLoggingWriteStop(local, "Swapchain_Destroy");
        }

        ISwapchainImage* acquireImage(bool wait) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_AcquireImage", TLPArg(this, "Swapchain"));

            std::unique_lock lock(m_mutex);

            uint32_t index;
            CHECK_XRCMD(xrAcquireSwapchainImage(m_swapchain, nullptr, &index));
            if (wait)
            {
                XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitInfo.timeout = XR_INFINITE_DURATION;
                CHECK_XRCMD(xrWaitSwapchainImage(m_swapchain, &waitInfo));
            }

            // Serialize the operations on the application device that might have occurred when acquiring the swapchain
            // image.
            m_fenceValue++;
            m_fenceOnApplicationDevice->signal(m_fenceValue);
            m_fenceOnCompositionDevice->waitOnDevice(m_fenceValue);

            m_acquiredImages.push_back(index);

            ISwapchainImage* const image = m_images[index].get();

            TraceLoggingWriteStop(local,
                                  "Swapchain_AcquireImage",
                                  TLArg(index, "AcquiredIndex"),
                                  TLPArg(image, "Image"));

            return image;
        }

        ISwapchainImage* getAcquiredImage() override
        {
            if (m_acquiredImages.empty())
            {
                throw std::runtime_error("No image was acquired");
            }
            return m_images[m_acquiredImages.front()].get();
        }

        void waitImage() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_WaitImage", TLPArg(this, "Swapchain"));

            // We don't need to check that an image was acquired since OpenXR will do it for us and throw an error
            // below.
            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            CHECK_XRCMD(xrWaitSwapchainImage(m_swapchain, &waitInfo));

            TraceLoggingWriteStop(local, "Swapchain_WaitImage");
        }

        void releaseImage() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_ReleaseImage", TLPArg(this, "Swapchain"));

            std::unique_lock lock(m_mutex);

            // We defer release of the OpenXR swapchain to ensure that we will have an opportunity to peek and/or poke
            // its content. If the same swapchain is released multiple times, then only defer the most recent call.
            if (!(m_accessForRead || m_accessForWrite) || m_lastReleasedImage.has_value())
            {
                CHECK_XRCMD(xrReleaseSwapchainImage(m_swapchain, nullptr));
            }
            else if (m_acquiredImages.empty())
            {
                throw std::runtime_error("No image was acquired");
            }

            m_lastReleasedImage = m_acquiredImages.front();
            m_acquiredImages.pop_front();

            TraceLoggingWriteStop(local, "Swapchain_ReleaseImage", TLArg(m_lastReleasedImage.value(), "ReleasedIndex"));
        }

        ISwapchainImage* getLastReleasedImage() const override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "Swapchain_GetLastReleasedImage",
                                   TLPArg(this, "Swapchain"),
                                   TLArg(m_lastReleasedImage.value_or(-1), "Index"));

            if (!m_accessForRead)
            {
                throw std::runtime_error("Not a readable swapchain");
            }

            ISwapchainImage* image = nullptr;
            if (m_lastReleasedImage.has_value())
            {
                if (m_bounceBufferOnApplicationDevice)
                {
                    // The swapchain image wasn't shareable and we must perform a copy to a shareable texture accessible
                    // on the composition device.
                    m_applicationDevice->copyTexture(m_images[m_lastReleasedImage.value()]->getApplicationTexture(),
                                                     m_bounceBufferOnApplicationDevice.get());
                }

                // Serialize the operations on the application device before accessing from the composition device.
                m_fenceValue++;
                m_fenceOnApplicationDevice->signal(m_fenceValue);
                m_fenceOnCompositionDevice->waitOnDevice(m_fenceValue);

                image = m_images[m_lastReleasedImage.value()].get();
            }

            TraceLoggingWriteStop(local, "Swapchain_GetLastReleasedImage", TLPArg(image, "Image"));

            return image;
        }

        void commitLastReleasedImage() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "Swapchain_CommitLastReleasedImage",
                                   TLPArg(this, "Swapchain"),
                                   TLArg(m_lastReleasedImage.value_or(-1), "Index"));

            if (!m_accessForWrite)
            {
                throw std::runtime_error("Not a writable swapchain");
            }

            if (m_lastReleasedImage.has_value())
            {
                // Serialize the operations on the composition device before copying to the application device or
                // releasing the swapchain image.
                m_fenceValue++;
                m_fenceOnCompositionDevice->signal(m_fenceValue);
                m_fenceOnApplicationDevice->waitOnDevice(m_fenceValue);

                if (m_bounceBufferOnApplicationDevice)
                {
                    // The swapchain image wasn't shareable and we must perform a copy from a shareable texture written
                    // on the composition device.
                    m_applicationDevice->copyTexture(m_bounceBufferOnApplicationDevice.get(),
                                                     m_images[m_lastReleasedImage.value()]->getApplicationTexture());
                }

                CHECK_XRCMD(xrReleaseSwapchainImage(m_swapchain, nullptr));
                m_lastReleasedImage = {};
            }

            TraceLoggingWriteStop(local, "Swapchain_CommitLastReleasedImage");
        }

        const XrSwapchainCreateInfo& getInfoOnCompositionDevice() const override
        {
            return m_infoOnCompositionDevice;
        }

        int64_t getFormatOnApplicationDevice() const override
        {
            return m_formatOnApplicationDevice;
        }

        ISwapchainImage* getImage(uint32_t index) const override
        {
            return m_images[index].get();
        }

        uint32_t getLength() const override
        {
            return static_cast<uint32_t>(m_images.size());
        }

        XrSwapchain getSwapchainHandle() const override
        {
            return m_swapchain;
        }

        XrSwapchainSubImage getSubImage() const override
        {
            XrSwapchainSubImage subImage;
            subImage.swapchain = m_swapchain;
            subImage.imageArrayIndex = 0;
            subImage.imageRect.offset.x = subImage.imageRect.offset.y = 0;
            subImage.imageRect.extent.width = m_infoOnCompositionDevice.width;
            subImage.imageRect.extent.height = m_infoOnCompositionDevice.height;
            return subImage;
        }

        const XrSwapchain m_swapchain;
        const int64_t m_formatOnApplicationDevice;
        IGraphicsDevice* const m_compositionDevice;
        IGraphicsDevice* const m_applicationDevice;
        const bool m_accessForRead;
        const bool m_accessForWrite;

        PFN_xrAcquireSwapchainImage xrAcquireSwapchainImage{nullptr};
        PFN_xrWaitSwapchainImage xrWaitSwapchainImage{nullptr};
        PFN_xrReleaseSwapchainImage xrReleaseSwapchainImage{nullptr};
        PFN_xrEnumerateSwapchainImages xrEnumerateSwapchainImages{nullptr};
        PFN_xrDestroySwapchain xrDestroySwapchain{nullptr};

        XrSwapchainCreateInfo m_infoOnCompositionDevice;

        std::vector<std::unique_ptr<ISwapchainImage>> m_images;
        std::shared_ptr<IGraphicsTexture> m_bounceBufferOnApplicationDevice;
        std::shared_ptr<IGraphicsTexture> m_bounceBufferOnCompositionDevice;
        std::shared_ptr<IGraphicsFence> m_fenceOnApplicationDevice;
        std::shared_ptr<IGraphicsFence> m_fenceOnCompositionDevice;
        mutable uint64_t m_fenceValue{0};

        std::mutex m_mutex;
        std::deque<uint32_t> m_acquiredImages;
        std::optional<uint32_t> m_lastReleasedImage{};
    };

    // A non-submittable swapchain must be accessible on both the application & composition device, however because it
    // does not need to be submitted, we can create the textures ourselves to ensure shareability and avoid extra
    // copies.
    struct NonSubmittableSwapchain : ISwapchain
    {
        NonSubmittableSwapchain(const XrSwapchainCreateInfo& infoOnApplicationDevice,
                                IGraphicsDevice* applicationDevice,
                                IGraphicsDevice* compositionDevice,
                                SwapchainMode mode)
            : m_formatOnApplicationDevice(infoOnApplicationDevice.format),
              m_accessForRead((mode & SwapchainMode::Read) == SwapchainMode::Read),
              m_accessForWrite((mode & SwapchainMode::Write) == SwapchainMode::Write),
              m_infoOnCompositionDevice(infoOnApplicationDevice)
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_Create", TLArg("Non-Submittable", "Type"));

            // Translate from the app device format to the composition device format.
            m_infoOnCompositionDevice.format = compositionDevice->translateFromGenericFormat(
                applicationDevice->translateToGenericFormat(infoOnApplicationDevice.format));

            // We only need 2 textures since OpenXR only allows for 1 frame in-flight and we won't submit textures to a
            // compositor that might need >2 images of history.
            // Make the textures available on the composition device.
            for (uint32_t i = 0; i < 2; i++)
            {
                const std::shared_ptr<IGraphicsTexture> textureOnCompositionDevice =
                    compositionDevice->createTexture(m_infoOnCompositionDevice, true /* shareable */);
                const std::shared_ptr<IGraphicsTexture> textureOnApplicationDevice =
                    applicationDevice->openTexture(textureOnCompositionDevice->getTextureHandle());
                std::unique_ptr<SwapchainImage> image =
                    std::make_unique<SwapchainImage>(textureOnApplicationDevice, textureOnCompositionDevice, i);

                TraceLoggingWriteTagged(local, "Swapchain_Create", TLPArg(image.get(), "Image"));

                m_images.push_back(std::move(image));
            }

            TraceLoggingWriteStop(local, "Swapchain_Create", TLPArg(this, "Swapchain"));
        }

        ~NonSubmittableSwapchain() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_Destroy", TLPArg(this, "Swapchain"));
            TraceLoggingWriteStop(local, "Swapchain_Destroy");
        }

        ISwapchainImage* acquireImage(bool wait) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_AcquireImage", TLPArg(this, "Swapchain"));

            std::unique_lock lock(m_mutex);

            if (m_acquiredImages.size() == m_images.size())
            {
                throw std::runtime_error("No image available to acquire");
            }

            const uint32_t index = m_nextImage++;
            m_acquiredImages.push_back(index);

            ISwapchainImage* const image = m_images[index].get();

            TraceLoggingWriteStop(local,
                                  "Swapchain_AcquireImage",
                                  TLArg(index, "AcquiredIndex"),
                                  TLPArg(image, "Image"));

            return image;
        }

        ISwapchainImage* getAcquiredImage() override
        {
            if (m_acquiredImages.empty())
            {
                throw std::runtime_error("No image was acquired");
            }
            return m_images[m_acquiredImages.front()].get();
        }

        void waitImage() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_WaitImage", TLPArg(this, "Swapchain"));

            std::unique_lock lock(m_mutex);

            if (m_acquiredImages.empty())
            {
                throw std::runtime_error("No image was acquired");
            }

            TraceLoggingWriteStop(local, "Swapchain_WaitImage");
        }

        void releaseImage() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "Swapchain_ReleaseImage", TLPArg(this, "Swapchain"));

            std::unique_lock lock(m_mutex);

            if (m_acquiredImages.empty())
            {
                throw std::runtime_error("No image was acquired");
            }

            m_lastReleasedImage = m_acquiredImages.front();
            m_acquiredImages.pop_front();

            TraceLoggingWriteStop(local, "Swapchain_ReleaseImage", TLArg(m_lastReleasedImage, "ReleasedIndex"));
        }

        ISwapchainImage* getLastReleasedImage() const override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "Swapchain_GetLastReleasedImage",
                                   TLPArg(this, "Swapchain"),
                                   TLArg(m_lastReleasedImage, "Index"));

            if (!m_accessForRead)
            {
                throw std::runtime_error("Not a readable swapchain");
            }

            ISwapchainImage* const image = m_images[m_lastReleasedImage].get();

            TraceLoggingWriteStop(local, "Swapchain_GetLastReleasedImage", TLPArg(image, "Image"));

            return image;
        }

        void commitLastReleasedImage() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "Swapchain_CommitLastReleasedImage",
                                   TLPArg(this, "Swapchain"),
                                   TLArg(m_lastReleasedImage, "Index"));

            if (!m_accessForWrite)
            {
                throw std::runtime_error("Not a writable swapchain");
            }

            TraceLoggingWriteStop(local, "Swapchain_CommitLastReleasedImage");
        }

        const XrSwapchainCreateInfo& getInfoOnCompositionDevice() const override
        {
            return m_infoOnCompositionDevice;
        }

        int64_t getFormatOnApplicationDevice() const override
        {
            return m_formatOnApplicationDevice;
        }

        ISwapchainImage* getImage(uint32_t index) const override
        {
            return m_images[index].get();
        }

        uint32_t getLength() const override
        {
            return (uint32_t)m_images.size();
        }

        XrSwapchain getSwapchainHandle() const override
        {
            throw std::runtime_error("Not a submittable swapchain");
        }

        XrSwapchainSubImage getSubImage() const override
        {
            throw std::runtime_error("Not a submittable swapchain");
        }

        const int64_t m_formatOnApplicationDevice;
        const bool m_accessForRead;
        const bool m_accessForWrite;

        XrSwapchainCreateInfo m_infoOnCompositionDevice;

        std::vector<std::unique_ptr<ISwapchainImage>> m_images;

        std::mutex m_mutex;
        uint32_t m_nextImage{0};
        std::deque<uint32_t> m_acquiredImages;
        uint32_t m_lastReleasedImage{};
    };

    struct CompositionFramework : ICompositionFramework
    {
        CompositionFramework(const XrInstanceCreateInfo& instanceInfo,
                             XrInstance instance,
                             PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr_,
                             XrBaseInStructure* binding,
                             XrSession session,
                             CompositionApi compositionApi)
            : m_instance(instance), xrGetInstanceProcAddr(xrGetInstanceProcAddr_), m_session(session)
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFramework_Create", TLXArg(session, "Session"));

            CHECK_XRCMD(xrGetInstanceProcAddr(m_instance,
                                              "xrCreateSwapchain",
                                              reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateSwapchain)));

            // Wrap the application device.
            if (binding->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR)
            {
                m_applicationDevice =
                    internal::wrapApplicationDevice(*reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(binding));
            }
            else if (binding->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR)
            {
                m_applicationDevice =
                    internal::wrapApplicationDevice(*reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(binding));
            }

            if (!m_applicationDevice)
            {
                throw std::runtime_error("Application graphics API is not supported");
            }

            // Create the device for composition according to the API layer's request.
            switch (compositionApi)
            {
            case CompositionApi::D3D11:
                m_compositionDevice = internal::createD3D11CompositionDevice(m_applicationDevice->getAdapterLuid());
                break;
            default:
                throw std::runtime_error("Composition graphics API is not supported");
            }

            m_fenceOnCompositionDevice = m_compositionDevice->createFence();
            m_fenceOnApplicationDevice = m_applicationDevice->openFence(m_fenceOnCompositionDevice->getFenceHandle());

            // Check for quirks.
            PFN_xrGetInstanceProperties xrGetInstanceProperties;
            CHECK_XRCMD(xrGetInstanceProcAddr(m_instance,
                                              "xrGetInstanceProperties",
                                              reinterpret_cast<PFN_xrVoidFunction*>(&xrGetInstanceProperties)));
            XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
            CHECK_XRCMD(xrGetInstanceProperties(instance, &instanceProperties));
            if (m_applicationDevice->getApi() == Api::D3D12)
            {
                // despite of D3D12 textures having the shareable flag, they are not shareable with D3D11.
                m_overrideShareable = false;
            }

            // Get the preferred formats for swapchains.
            PFN_xrEnumerateSwapchainFormats xrEnumerateSwapchainFormats;
            CHECK_XRCMD(xrGetInstanceProcAddr(m_instance,
                                              "xrEnumerateSwapchainFormats",
                                              reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateSwapchainFormats)));
            uint32_t formatsCount;
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &formatsCount, nullptr));
            std::vector<int64_t> formats(formatsCount);
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, formatsCount, &formatsCount, formats.data()));
            for (const int64_t formatOnApplicationDevice : formats)
            {
                const DXGI_FORMAT format = m_applicationDevice->translateToGenericFormat(formatOnApplicationDevice);
                const bool isDepth = isDepthFormat(format);
                const bool isColor = !isDepth;
                const bool isSRGB = isColor && isSRGBFormat(format);

                if (m_preferredColorFormat == DXGI_FORMAT_UNKNOWN && isColor && !isSRGB)
                {
                    m_preferredColorFormat = format;
                }
                if (m_preferredSRGBColorFormat == DXGI_FORMAT_UNKNOWN && isColor && isSRGB)
                {
                    m_preferredSRGBColorFormat = format;
                }
                if (m_preferredDepthFormat == DXGI_FORMAT_UNKNOWN && isDepth)
                {
                    m_preferredDepthFormat = format;
                }
            }
            TraceLoggingWriteTagged(local,
                                    "CompositionFramework_Create",
                                    TLArg((int64_t)m_preferredColorFormat, "PreferredColorFormat"),
                                    TLArg((int64_t)m_preferredSRGBColorFormat, "PreferredSRGBColorFormat"),
                                    TLArg((int64_t)m_preferredDepthFormat, "PreferredDepthFormat"));

            TraceLoggingWriteStop(local, "CompositionFramework_Create", TLPArg(this, "CompositionFramework"));
        }

        ~CompositionFramework() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFramework_Destroy", TLXArg(m_session, "Session"));

            if (m_fenceOnCompositionDevice)
            {
                m_fenceOnCompositionDevice->waitOnCpu(m_fenceValue);
            }
#ifdef _DEBUG
            auto device = m_compositionDevice->getNativeDevice<D3D11>();
             
            auto debugDev = std::make_unique<ComPtr<ID3D11Debug>>();
            CHECK_HRCMD(device->QueryInterface(IID_PPV_ARGS(debugDev->ReleaseAndGetAddressOf())));

            auto dxgiDebugDev = std::make_unique<ComPtr<IDXGIDebug>>();
            CHECK_HRCMD(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiDebugDev->ReleaseAndGetAddressOf())));

            m_compositionDevice.reset();
            m_applicationDevice.reset();

            debugDev->Get()->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            dxgiDebugDev->Get()->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
#endif

            TraceLoggingWriteStop(local, "CompositionFramework_Destroy");
        }

        XrSession getSessionHandle() const override
        {
            return m_session;
        }

        void setSessionData(std::unique_ptr<ICompositionSessionData> sessionData) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "CompositionFramework_SetSessionData",
                                   TLXArg(m_session, "Session"),
                                   TLPArg(sessionData.get(), "SessionData"));

            m_sessionData = std::move(sessionData);

            TraceLoggingWriteStop(local, "CompositionFramework_SetSessionData");
        }

        ICompositionSessionData* getSessionDataPtr() const override
        {
            return m_sessionData.get();
        }

        std::shared_ptr<ISwapchain> createSwapchain(const XrSwapchainCreateInfo& infoOnApplicationDevice,
                                                    SwapchainMode mode) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "CompositionFramework_CreateSwapchain",
                                   TLXArg(m_session, "Session"),
                                   TLArg(infoOnApplicationDevice.arraySize, "ArraySize"),
                                   TLArg(infoOnApplicationDevice.width, "Width"),
                                   TLArg(infoOnApplicationDevice.height, "Height"),
                                   TLArg(infoOnApplicationDevice.createFlags, "CreateFlags"),
                                   TLArg(infoOnApplicationDevice.format, "Format"),
                                   TLArg(infoOnApplicationDevice.faceCount, "FaceCount"),
                                   TLArg(infoOnApplicationDevice.mipCount, "MipCount"),
                                   TLArg(infoOnApplicationDevice.sampleCount, "SampleCount"),
                                   TLArg(infoOnApplicationDevice.usageFlags, "UsageFlags"),
                                   TLArg((int)mode, "Mode"));

            std::shared_ptr<ISwapchain> result;
            if ((mode & SwapchainMode::Submit) == SwapchainMode::Submit)
            {
                XrSwapchain swapchain;
                CHECK_XRCMD(xrCreateSwapchain(m_session, &infoOnApplicationDevice, &swapchain));
                result = std::make_shared<SubmittableSwapchain>(xrGetInstanceProcAddr,
                                                                m_instance,
                                                                swapchain,
                                                                infoOnApplicationDevice,
                                                                m_applicationDevice.get(),
                                                                m_compositionDevice.get(),
                                                                mode,
                                                                m_overrideShareable);
            }
            else
            {
                result = std::make_shared<NonSubmittableSwapchain>(infoOnApplicationDevice,
                                                                   m_applicationDevice.get(),
                                                                   m_compositionDevice.get(),
                                                                   mode);
            }

            TraceLoggingWriteStop(local, "CompositionFramework_CreateSwapchain", TLPArg(result.get(), "Swapchain"));

            return result;
        }

        void serializePreComposition() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFramework_SerializePreComposition", TLXArg(m_session, "Session"));

            std::unique_lock lock(m_fenceMutex);

            m_fenceValue++;
            m_fenceOnApplicationDevice->signal(m_fenceValue);
            m_fenceOnCompositionDevice->waitOnDevice(m_fenceValue);

            TraceLoggingWriteStop(local, "CompositionFramework_SerializePreComposition");
        }

        void serializePostComposition() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "CompositionFramework_SerializePostComposition",
                                   TLXArg(m_session, "Session"));

            std::unique_lock lock(m_fenceMutex);

            m_fenceValue++;
            m_fenceOnCompositionDevice->signal(m_fenceValue);
            m_fenceOnApplicationDevice->waitOnDevice(m_fenceValue);

            TraceLoggingWriteStop(local, "CompositionFramework_SerializePostComposition");
        }

        IGraphicsDevice* getCompositionDevice() const override
        {
            return m_compositionDevice.get();
        }

        IGraphicsDevice* getApplicationDevice() const override
        {
            return m_applicationDevice.get();
        }

        int64_t getPreferredSwapchainFormatOnApplicationDevice(XrSwapchainUsageFlags usageFlags,
                                                               bool preferSRGB) const override
        {
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            if (usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT)
            {
                format = preferSRGB ? m_preferredSRGBColorFormat : m_preferredColorFormat;
            }
            else if (usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                format = m_preferredDepthFormat;
            }
            return m_applicationDevice->translateFromGenericFormat(format);
        }

        const XrInstance m_instance;
        const PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr;
        const XrSession m_session;

        std::unique_ptr<ICompositionSessionData> m_sessionData;

        std::shared_ptr<IGraphicsDevice> m_compositionDevice;
        std::shared_ptr<IGraphicsDevice> m_applicationDevice;
        DXGI_FORMAT m_preferredColorFormat{DXGI_FORMAT_UNKNOWN};
        DXGI_FORMAT m_preferredSRGBColorFormat{DXGI_FORMAT_UNKNOWN};
        DXGI_FORMAT m_preferredDepthFormat{DXGI_FORMAT_UNKNOWN};

        std::mutex m_fenceMutex;
        std::shared_ptr<IGraphicsFence> m_fenceOnApplicationDevice;
        std::shared_ptr<IGraphicsFence> m_fenceOnCompositionDevice;
        uint64_t m_fenceValue{0};

        std::optional<bool> m_overrideShareable;

        PFN_xrCreateSwapchain xrCreateSwapchain{nullptr};
    };

    struct CompositionFrameworkFactory : ICompositionFrameworkFactory
    {
        CompositionFrameworkFactory(const XrInstanceCreateInfo& instanceInfo,
                                    XrInstance instance,
                                    PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr_,
                                    CompositionApi compositionApi)
            : m_instance(instance), xrGetInstanceProcAddr(xrGetInstanceProcAddr_), m_compositionApi(compositionApi),
              m_instanceInfo(instanceInfo)
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "CompositionFrameworkFactory_Create",
                                   TLArg(xr::ToString(compositionApi).c_str(), "CompositionApi"));

            {
                std::unique_lock lock(factoryMutex);
                if (factory)
                {
                    throw std::runtime_error("There can only be one CompositionFramework factory");
                }
                factory = this;
            }

            // Deep-copy the instance extensions strings.
            m_instanceExtensions.reserve(m_instanceInfo.enabledExtensionCount);
            for (uint32_t i = 0; i < m_instanceInfo.enabledExtensionCount; i++)
            {
                m_instanceExtensions.push_back(m_instanceInfo.enabledExtensionNames[i]);
                m_instanceExtensionsArray.push_back(m_instanceExtensions.back().c_str());
            }
            m_instanceInfo.enabledExtensionNames = m_instanceExtensionsArray.data();

            TraceLoggingWriteStop(local,
                                  "CompositionFrameworkFactory_Create",
                                  TLPArg(this, "CompositionFrameworkFactory"));
        }

        ~CompositionFrameworkFactory() override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFrameworkFactory_Destroy");

            std::unique_lock lock(factoryMutex);

            factory = nullptr;

            TraceLoggingWriteStop(local, "CompositionFrameworkFactory_Destroy");
        }

        ICompositionFramework* getCompositionFramework(XrSession session) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFrameworkFactory_getCompositionFramework");
            std::unique_lock lock(m_sessionsMutex);

            auto it = m_sessions.find(session);
            if (it == m_sessions.end())
            {
                auto bindingIt = m_applicationBindings.find(session);
                if (bindingIt != m_applicationBindings.end())
                {
                    try
                    {
                        m_sessions.insert_or_assign(
                            session,
                            std::move(std::make_unique<CompositionFramework>(m_instanceInfo,
                                                                             m_instance,
                                                                             xrGetInstanceProcAddr,
                                                                             bindingIt->second,
                                                                             session,
                                                                             m_compositionApi)));
                        it = m_sessions.find(session);
                        if (it != m_sessions.end())
                        {
                            Log("created composition framework for overlay");
                        }
                        else
                        {
                            ErrorLog("%s: creating composition framework failed, for session %llu",
                                     __FUNCTION__,
                                     session);
                            TraceLoggingWriteStop(local,
                                                  "CompositionFrameworkFactory_getCompositionFramework",
                                                  TLArg(false, "FrameworkCreation"));
                            return nullptr;
                        }
                    }
                    catch (std::exception& exc)
                    {
                        TraceLoggingWriteTagged(local,
                                                "CompositionFrameworkFactory_getCompositionFramework",
                                                TLArg(exc.what(), "Error"));
                        ErrorLog("%s: exception on framework creation: %s", __FUNCTION__, exc.what());
                        return nullptr;
                    }
                }
                else
                {
                    // The session (likely) could not be handled.
                    ErrorLog("%s: no graphics binding found for session %llu, graphical overlay will not work", __FUNCTION__, session);
                    TraceLoggingWriteStop(local,
                                          "CompositionFrameworkFactory_getCompositionFramework",
                                          TLArg(false, "SessionKnown"));
                    return nullptr;
                }
            }
            TraceLoggingWriteStop(local, "CompositionFrameworkFactory_getCompositionFramework", TLArg(true, "Success"));
            return it->second.get();
        }

        bool IsUsingD3D12(const XrSession session) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFrameworkFactory_IsUsingD3D12", TLXArg(session, "Session"));

            auto binding = m_applicationBindings.find(session);
            if (m_applicationBindings.end() == binding)
            {
                ErrorLog("%s: no suitable d3d binding found, defaulting to d3d11", __FUNCTION__);
                TraceLoggingWriteStop(local,
                                      "CompositionFrameworkFactory_IsUsingD3D12",
                                      TLArg(false, "Binding_Found"));
                return false;
            }
            if (binding->second->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR)
            {
                TraceLoggingWriteStop(local,
                                      "CompositionFrameworkFactory_IsUsingD3D12",
                                      TLArg(true, "D3D12_Binding_Found"));
                return true;
            }
            else if (binding->second->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR)
            {
                TraceLoggingWriteStop(local,
                                      "CompositionFrameworkFactory_IsUsingD3D12",
                                      TLArg(true, "D3D11_Binding_Found"));
                return false;
            }

            TraceLoggingWriteStop(local, "CompositionFrameworkFactory_IsUsingD3D12", TLArg(false, "Binding_Type"));
            return false;
        }

        void CreateSession(const XrSessionCreateInfo* createInfo, XrSession session) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFrameworkFactory_CreateSession");

            std::unique_lock lock(m_sessionsMutex);

            // Detect which graphics bindings to look for.
            bool has_XR_KHR_D3D11_enable = false;
            bool has_XR_KHR_D3D12_enable = false;
            for (uint32_t i = 0; i < m_instanceInfo.enabledExtensionCount; i++)
            {
                const std::string_view extensionName(m_instanceInfo.enabledExtensionNames[i]);

                if (extensionName == XR_KHR_D3D11_ENABLE_EXTENSION_NAME)
                {
                    has_XR_KHR_D3D11_enable = true;
                    Log("session %llu has D3D11 extension enabled", session);
                }
                if (extensionName == XR_KHR_D3D12_ENABLE_EXTENSION_NAME)
                {
                    has_XR_KHR_D3D12_enable = true;
                    Log("session %llu has D3D12 extension enabled", session);
                }
            }

            // save binding for later use
            auto entry = static_cast<const XrBaseInStructure*>(createInfo->next);
            while (entry)
            {
                if (has_XR_KHR_D3D11_enable && entry->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR)
                {
                    Log("session %llu is using D3D11 graphics binding", session);
                    auto* binding = new XrGraphicsBindingD3D11KHR{
                        XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
                        nullptr,
                        reinterpret_cast<const XrGraphicsBindingD3D11KHR*>(entry)->device};
                    m_applicationBindings[session] = reinterpret_cast<XrBaseInStructure*> (binding);
                    break;

                }
                if (has_XR_KHR_D3D12_enable && entry->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR)
                {
                    Log("session %llu is using D3D12 graphics binding", session);
                    auto* binding =
                        new XrGraphicsBindingD3D12KHR{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR,
                                                      nullptr,
                                                      reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(entry)->device,
                                                      reinterpret_cast<const XrGraphicsBindingD3D12KHR*>(entry)->queue};
                    m_applicationBindings[session] = reinterpret_cast<XrBaseInStructure*>(binding);
                    break;
                }
                entry = entry->next;
            }
            if (!m_applicationBindings.contains(session))
            {
                Log("session %llu is using neither D3D11 nor D3D12 graphics binding", session);
            }

            TraceLoggingWriteStop(local, "CompositionFrameworkFactory_CreateSession", TLXArg(session, "Session"));
        }

        void DestroySession(XrSession session) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "CompositionFrameworkFactory_DestroySession", TLXArg(session, "Session"));

            {
                std::unique_lock lock(m_sessionsMutex);

                auto it = m_sessions.find(session);
                if (it != m_sessions.end())
                {
                    m_sessions.erase(it);
                }
                auto binding = m_applicationBindings.find(session);
                if (binding != m_applicationBindings.end())
                {
                    delete binding->second;
                    m_applicationBindings.erase(binding);
                }
            }
            

            TraceLoggingWriteStop(local, "CompositionFrameworkFactory_DestroySession");
        }

        const XrInstance m_instance;
        const PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr;
        const CompositionApi m_compositionApi;
        XrInstanceCreateInfo m_instanceInfo;
        std::vector<std::string> m_instanceExtensions;
        std::vector<const char*> m_instanceExtensionsArray;

        std::mutex m_sessionsMutex;
        std::unordered_map<XrSession, std::unique_ptr<CompositionFramework>> m_sessions{};
        std::unordered_map<XrSession, XrBaseInStructure*> m_applicationBindings{};

        static inline std::mutex factoryMutex;
        static inline CompositionFrameworkFactory* factory{nullptr};
    };

} // namespace

namespace openxr_api_layer::graphics {

    std::shared_ptr<ICompositionFrameworkFactory>
    createCompositionFrameworkFactory(const XrInstanceCreateInfo& instanceInfo,
                                      XrInstance instance,
                                      PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr,
                                      CompositionApi compositionApi) {
        return std::make_shared<CompositionFrameworkFactory>(
            instanceInfo, instance, xrGetInstanceProcAddr, compositionApi);
    }

} // namespace openxr_api_layer::graphics