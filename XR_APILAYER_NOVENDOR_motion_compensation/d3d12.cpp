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

#ifdef XR_USE_GRAPHICS_API_D3D12

#include "log.h"
#include "graphics.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

namespace {

    using namespace openxr_api_layer::log;
    using namespace openxr_api_layer::graphics;

    struct D3D12Fence : IGraphicsFence {
        D3D12Fence(ID3D12Fence* fence, ID3D12CommandQueue* commandQueue, bool shareable)
            : m_fence(fence), m_commandQueue(commandQueue), m_isShareable(shareable) {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(
                local, "D3D12Fence_Create", TLPArg(fence, "D3D12Fence"), TLArg(shareable, "Shareable"));

            m_fence->GetDevice(IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf()));

            TraceLoggingWriteStop(local, "D3D12Fence_Create", TLPArg(this, "Fence"));
        }

        ~D3D12Fence() override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Fence_Destroy", TLPArg(this, "Fence"));
            TraceLoggingWriteStop(local, "D3D12Fence_Destroy");
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        void* getNativeFencePtr() const override {
            return m_fence.Get();
        }

        ShareableHandle getFenceHandle() const override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Fence_Export", TLPArg(this, "Fence"));

            if (!m_isShareable) {
                throw std::runtime_error("Fence is not shareable");
            }

            ShareableHandle handle{};
            CHECK_HRCMD(
                m_device->CreateSharedHandle(m_fence.Get(), nullptr, GENERIC_ALL, nullptr, handle.ntHandle.put()));
            handle.isNtHandle = true;
            handle.origin = Api::D3D12;

            TraceLoggingWriteStop(local, "D3D12Fence_Export", TLPArg(handle.ntHandle.get(), "Handle"));

            return handle;
        }

        void signal(uint64_t value) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Fence_Signal", TLPArg(this, "Fence"), TLArg(value, "Value"));

            CHECK_HRCMD(m_commandQueue->Signal(m_fence.Get(), value));

            TraceLoggingWriteStop(local, "D3D12Fence_Signal");
        }

        void waitOnDevice(uint64_t value) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(
                local, "D3D12Fence_Wait", TLPArg(this, "Fence"), TLArg("Device", "WaitType"), TLArg(value, "Value"));

            CHECK_HRCMD(m_commandQueue->Wait(m_fence.Get(), value));

            TraceLoggingWriteStop(local, "D3D12Fence_Wait");
        }

        void waitOnCpu(uint64_t value) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(
                local, "D3D12Fence_Wait", TLPArg(this, "Fence"), TLArg("Host", "WaitType"), TLArg(value, "Value"));

            wil::unique_handle eventHandle;
            CHECK_HRCMD(m_commandQueue->Signal(m_fence.Get(), value));
            *eventHandle.put() = CreateEventEx(nullptr, "D3D Fence", 0, EVENT_ALL_ACCESS);
            CHECK_HRCMD(m_fence->SetEventOnCompletion(value, eventHandle.get()));
            WaitForSingleObject(eventHandle.get(), INFINITE);
            ResetEvent(eventHandle.get());

            TraceLoggingWriteStop(local, "D3D12Fence_Wait");
        }

        bool isShareable() const override {
            return m_isShareable;
        }

        const ComPtr<ID3D12Fence> m_fence;
        const ComPtr<ID3D12CommandQueue> m_commandQueue;
        const bool m_isShareable;

        ComPtr<ID3D12Device> m_device;
    };

    struct D3D12Texture : IGraphicsTexture {
        D3D12Texture(ID3D12Resource* texture) : m_texture(texture) {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Texture_Create", TLPArg(texture, "D3D12Texture"));

            m_texture->GetDevice(IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf()));

            D3D12_RESOURCE_DESC desc = m_texture->GetDesc();
            TraceLoggingWriteTagged(local,
                                    "D3D12Texture_Create",
                                    TLArg(desc.Width, "Width"),
                                    TLArg(desc.Height, "Height"),
                                    TLArg(desc.DepthOrArraySize, "ArraySize"),
                                    TLArg(desc.MipLevels, "MipCount"),
                                    TLArg(desc.SampleDesc.Count, "SampleCount"),
                                    TLArg((int)desc.Format, "Format"),
                                    TLArg((int)desc.Flags, "Flags"));

            // Construct the API-agnostic info descriptor.
            m_info.format = (int64_t)desc.Format;
            m_info.width = (uint32_t)desc.Width;
            m_info.height = desc.Height;
            m_info.arraySize = desc.DepthOrArraySize;
            m_info.mipCount = desc.MipLevels;
            m_info.sampleCount = desc.SampleDesc.Count;
            m_info.faceCount = 1;
            m_info.usageFlags = 0;
            if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
                m_info.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            }
            if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
                m_info.usageFlags |= XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            if (!(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
                m_info.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            }
            if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
                m_info.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
            }

            // Identify the shareability.
            D3D12_HEAP_FLAGS heapFlags;
            CHECK_HRCMD(m_texture->GetHeapProperties(nullptr, &heapFlags));
            m_isShareable = heapFlags & D3D12_HEAP_FLAG_SHARED;

            TraceLoggingWriteStop(
                local, "D3D12Texture_Create", TLPArg(this, "Texture"), TLArg(m_isShareable, "Shareable"));
        }

        ~D3D12Texture() override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Texture_Destroy", TLPArg(this, "Texture"));
            TraceLoggingWriteStop(local, "D3D12Texture_Destroy");
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        void* getNativeTexturePtr() const override {
            return m_texture.Get();
        }

        ShareableHandle getTextureHandle() const override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Texture_Export", TLPArg(this, "Texture"));

            if (!m_isShareable) {
                throw std::runtime_error("Texture is not shareable");
            }

            ShareableHandle handle{};
            CHECK_HRCMD(
                m_device->CreateSharedHandle(m_texture.Get(), nullptr, GENERIC_ALL, nullptr, handle.ntHandle.put()));
            handle.isNtHandle = true;
            handle.origin = Api::D3D12;

            TraceLoggingWriteStop(local, "D3D12Texture_Export", TLPArg(handle.ntHandle.get(), "Handle"));

            return handle;
        }

        const XrSwapchainCreateInfo& getInfo() const override {
            return m_info;
        }

        bool isShareable() const override {
            return m_isShareable;
        }

        const ComPtr<ID3D12Resource> m_texture;
        ComPtr<ID3D12Device> m_device;

        XrSwapchainCreateInfo m_info{};
        bool m_isShareable{false};
    };

    struct D3D12ReusableCommandList {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> commandList;
        uint32_t completedFenceValue{0};
    };

    struct D3D12GraphicsDevice : IGraphicsDevice {
        D3D12GraphicsDevice(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
            : m_device(device), m_commandQueue(commandQueue) {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(
                local, "D3D12GraphicsDevice_Create", TLPArg(device, "D3D12Device"), TLPArg(commandQueue, "Queue"));

            {
                const LUID adapterLuid = m_device->GetAdapterLuid();

                ComPtr<IDXGIFactory1> dxgiFactory;
                CHECK_HRCMD(CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.ReleaseAndGetAddressOf())));
                ComPtr<IDXGIAdapter1> dxgiAdapter;
                for (UINT adapterIndex = 0;; adapterIndex++) {
                    // EnumAdapters1 will fail with DXGI_ERROR_NOT_FOUND when there are no more adapters to
                    // enumerate.
                    CHECK_HRCMD(dxgiFactory->EnumAdapters1(adapterIndex, dxgiAdapter.ReleaseAndGetAddressOf()));

                    DXGI_ADAPTER_DESC1 desc;
                    CHECK_HRCMD(dxgiAdapter->GetDesc1(&desc));
                    if (!memcmp(&desc.AdapterLuid, &adapterLuid, sizeof(LUID))) {
                        TraceLoggingWriteTagged(
                            local,
                            "D3D12GraphicsDevice_Create",
                            TLArg(desc.Description, "Adapter"),
                            TLArg(fmt::format("{}:{}", adapterLuid.HighPart, adapterLuid.LowPart).c_str(), " Luid"));
                        break;
                    }
                }
            }

            CHECK_HRCMD(m_device->CreateFence(
                0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_commandListPoolFence.ReleaseAndGetAddressOf())));

            TraceLoggingWriteStop(local, "D3D12GraphicsDevice_Create", TLPArg(this, "Device"));
        }

        ~D3D12GraphicsDevice() override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12GraphicsDevice_Destroy", TLPArg(this, "Device"));
            TraceLoggingWriteStop(local, "D3D12GraphicsDevice_Destroy");
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        void* getNativeDevicePtr() const override {
            return m_device.Get();
        }

        void* getNativeContextPtr() const override {
            return m_commandQueue.Get();
        }

        std::shared_ptr<IGraphicsFence> createFence(bool shareable) override {
            ComPtr<ID3D12Fence> fence;
            CHECK_HRCMD(m_device->CreateFence(0,
                                              shareable ? D3D12_FENCE_FLAG_SHARED : D3D12_FENCE_FLAG_NONE,
                                              IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));
            return std::make_shared<D3D12Fence>(fence.Get(), m_commandQueue.Get(), shareable);
        }

        std::shared_ptr<IGraphicsFence> openFence(const ShareableHandle& handle) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "D3D12Fence_Import",
                                   TLArg(!handle.isNtHandle ? handle.handle : handle.ntHandle.get(), "Handle"),
                                   TLArg(handle.isNtHandle, "IsNTHandle"));

            if (!handle.isNtHandle) {
                throw std::runtime_error("Must be NTHANDLE");
            }

            ComPtr<ID3D12Fence> fence;
            CHECK_HRCMD(m_device->OpenSharedHandle(handle.isNtHandle ? handle.ntHandle.get() : handle.handle,
                                                   IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));

            std::shared_ptr<IGraphicsFence> result =
                std::make_shared<D3D12Fence>(fence.Get(), m_commandQueue.Get(), false /* shareable */);

            TraceLoggingWriteStop(local, "D3D12Fence_Import", TLPArg(result.get(), "Fence"));

            return result;
        }

        std::shared_ptr<IGraphicsTexture> createTexture(const XrSwapchainCreateInfo& info,
                                                        bool shareable) override
        {
            D3D12_RESOURCE_DESC desc{};
            desc.Format = (DXGI_FORMAT)info.format;
            desc.Width = info.width;
            desc.Height = info.height;
            desc.DepthOrArraySize = info.arraySize;
            desc.MipLevels = info.mipCount;
            desc.SampleDesc.Count = info.sampleCount;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) {
                desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
                initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
                initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
            }
            if (!(info.usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT)) {
                desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT) {
                desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            ComPtr<ID3D12Resource> texture;
            D3D12_HEAP_PROPERTIES heapType{};
            heapType.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapType.CreationNodeMask = heapType.VisibleNodeMask = 1;
            CHECK_HRCMD(m_device->CreateCommittedResource(&heapType,
                                                          shareable ? D3D12_HEAP_FLAG_SHARED : D3D12_HEAP_FLAG_NONE,
                                                          &desc,
                                                          initialState,
                                                          nullptr,
                                                          IID_PPV_ARGS(texture.ReleaseAndGetAddressOf())));
            return std::make_shared<D3D12Texture>(texture.Get());
        }

        std::shared_ptr<IGraphicsTexture> openTexture(const ShareableHandle& handle) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "D3D12Texture_Import",
                                   TLArg(!handle.isNtHandle ? handle.handle : handle.ntHandle.get(), "Handle"),
                                   TLArg(handle.isNtHandle, "IsNTHandle"));

            ComPtr<ID3D12Resource> texture;
            CHECK_HRCMD(m_device->OpenSharedHandle(handle.isNtHandle ? handle.ntHandle.get() : handle.handle,
                                                   IID_PPV_ARGS(texture.ReleaseAndGetAddressOf())));

            std::shared_ptr<IGraphicsTexture> result = std::make_shared<D3D12Texture>(texture.Get());

            TraceLoggingWriteStop(local, "D3D12Texture_Import", TLPArg(result.get(), "Texture"));

            return result;
        }

        std::shared_ptr<IGraphicsTexture> openTexturePtr(void* nativeTexturePtr,
                                                         const XrSwapchainCreateInfo& info) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Texture_Import", TLPArg(nativeTexturePtr, "D3D12Texture"));

            ID3D12Resource* texture = reinterpret_cast<ID3D12Resource*>(nativeTexturePtr);

            std::shared_ptr<IGraphicsTexture> result = std::make_shared<D3D12Texture>(texture);

            TraceLoggingWriteStop(local, "D3D12Texture_Import", TLPArg(result.get(), "Texture"));

            return result;
        }

        void copyTexture(IGraphicsTexture* from, IGraphicsTexture* to) override {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D12Texture_Copy", TLPArg(from, "Source"), TLPArg(to, "Destination"));

            D3D12ReusableCommandList commandList = getCommandList();
            commandList.commandList->CopyResource(to->getNativeTexture<D3D12>(), from->getNativeTexture<D3D12>());
            submitCommandList(std::move(commandList));

            TraceLoggingWriteStop(local, "D3D12Texture_Copy");
        }

        std::shared_ptr<IShaderBuffer> createBuffer(size_t size,
                                                            std::string_view debugName,
                                                            const void* initialData = nullptr,
                                                            bool immutable = false) override
        {
            ErrorLog("%s: function not implemented!", __FUNCTION__);
            return std::shared_ptr<IShaderBuffer>();
        }

        std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                      std::vector<uint16_t>& indices,
                                                      std::string_view debugName) override
        {
            ErrorLog("%s: function not implemented!", __FUNCTION__);
            return std::shared_ptr<ISimpleMesh>();
        }

        bool CopyAppTexture(const SwapchainState& swapchainState,
                            std::shared_ptr<IGraphicsTexture> target,
                            bool fromApp) override
        {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local,
                                   "D3D11GraphicsDevice_CopyAppTexture",
                                   TLArg(swapchainState.index, "Index"),
                                   TLArg(static_cast<uint32_t>(swapchainState.format), "Format"),
                                   TLArg(swapchainState.doRelease, "DoRelease"),
                                   TLArg(swapchainState.texturesD3D12.size(), "Size"),
                                   TLPArg(target.get(), "Target"));

            if (swapchainState.index >= swapchainState.texturesD3D12.size())
            {
                ErrorLog("%s: invalid to texture index %u, max: %u",
                         __FUNCTION__,
                         swapchainState.index,
                         swapchainState.texturesD3D12.size() - 1);
                TraceLoggingWriteStop(local, "D3D11GraphicsDevice_CopyAppTexture", TLArg(false, "Index_In_Range"));
                return false;
            }

            if (!m_SwapchainTextures.contains(swapchainState.swapchain))
            {
                const auto handle = target->getTextureHandle();
                m_SwapchainTextures[swapchainState.swapchain] = openTexture(handle);
            }
            const auto sharedTexture = m_SwapchainTextures[swapchainState.swapchain];
            D3D12ReusableCommandList commandList = getCommandList();
            if (fromApp)
            {
                commandList.commandList->CopyResource(sharedTexture->getNativeTexture<D3D12>(),
                                                      swapchainState.texturesD3D12[swapchainState.index]);
            }
            else
            {
                commandList.commandList->CopyResource(swapchainState.texturesD3D12[swapchainState.index],
                                                      sharedTexture->getNativeTexture<D3D12>());
            }
            submitCommandList(std::move(commandList));
            return true;
        }
        void setViewProjection(const xr::math::ViewProjection& view) override
        {
            ErrorLog("%s: function not implemented!", __FUNCTION__);
        }
        void draw(std::shared_ptr<ISimpleMesh> mesh,
                  const XrPosef& pose,
                  XrVector3f scaling = {1.0f, 1.0f, 1.0f}) override
        {
            ErrorLog("%s: function not implemented!", __FUNCTION__);
        }

        void UnsetDrawResources() const override
        {
            ErrorLog("%s: function not implemented!", __FUNCTION__);
        }

        GenericFormat translateToGenericFormat(int64_t format) const override {
            return static_cast<DXGI_FORMAT>(format);
        }

        int64_t translateFromGenericFormat(GenericFormat format) const override {
            return (int64_t)format;
        }

        LUID getAdapterLuid() const override {
            return m_device->GetAdapterLuid();
        }

        D3D12ReusableCommandList getCommandList() {
            std::unique_lock lock(m_commandListPoolMutex);

            if (m_availableCommandList.empty()) {
                // Recycle completed command lists.
                while (!m_pendingCommandList.empty() && m_commandListPoolFence->GetCompletedValue() >=
                                                            m_pendingCommandList.front().completedFenceValue) {
                    m_availableCommandList.push_back(std::move(m_pendingCommandList.front()));
                    m_pendingCommandList.pop_front();
                }
            }

            D3D12ReusableCommandList commandList;
            if (m_availableCommandList.empty()) {
                // Allocate a new command list if needed.
                CHECK_HRCMD(m_device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandList.allocator.ReleaseAndGetAddressOf())));
                CHECK_HRCMD(
                    m_device->CreateCommandList(0,
                                                D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                commandList.allocator.Get(),
                                                nullptr,
                                                IID_PPV_ARGS(commandList.commandList.ReleaseAndGetAddressOf())));
            } else {
                commandList = m_availableCommandList.front();
                m_availableCommandList.pop_front();

                // Reset the command list before reuse.
                CHECK_HRCMD(commandList.commandList->Reset(commandList.allocator.Get(), nullptr));
            }
            return commandList;
        }

        void submitCommandList(D3D12ReusableCommandList commandList) {
            std::unique_lock lock(m_commandListPoolMutex);

            CHECK_HRCMD(commandList.commandList->Close());
            m_commandQueue->ExecuteCommandLists(
                1, reinterpret_cast<ID3D12CommandList**>(commandList.commandList.GetAddressOf()));
            commandList.completedFenceValue = m_commandListPoolFenceValue + 1;
            m_commandQueue->Signal(m_commandListPoolFence.Get(), commandList.completedFenceValue);
            m_pendingCommandList.push_back(std::move(commandList));
        }

        const ComPtr<ID3D12Device> m_device;
        const ComPtr<ID3D12CommandQueue> m_commandQueue;

        std::mutex m_commandListPoolMutex;
        std::deque<D3D12ReusableCommandList> m_availableCommandList;
        std::deque<D3D12ReusableCommandList> m_pendingCommandList;
        ComPtr<ID3D12Fence> m_commandListPoolFence;
        uint32_t m_commandListPoolFenceValue{0};

        std::map<XrSwapchain, std::shared_ptr<IGraphicsTexture>> m_SwapchainTextures;
    };

} // namespace

namespace openxr_api_layer::graphics::internal {

    std::shared_ptr<IGraphicsDevice> wrapApplicationDevice(const XrGraphicsBindingD3D12KHR& bindings) {
        return std::make_shared<D3D12GraphicsDevice>(bindings.device, bindings.queue);
    }

} // namespace openxr_api_layer::utils::graphics::internal

#endif
