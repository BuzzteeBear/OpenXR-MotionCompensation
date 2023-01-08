// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
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

#include "d3dcommon.h"
#include "shader_utilities.h"
#include "interfaces.h"
#include "log.h"

#include <wincodec.h>

namespace {

    using namespace graphics;
    using namespace graphics::d3dcommon;
    using namespace motion_compensation_layer::log;

    constexpr size_t MaxGpuTimers = 128;
    constexpr size_t MaxModelBuffers = 128;

    inline void SetDebugName(ID3D12Object* resource, std::string_view name) {
        if (resource && !name.empty())
            resource->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
    }

    auto descriptorCompare = [](const D3D12_CPU_DESCRIPTOR_HANDLE& left, const D3D12_CPU_DESCRIPTOR_HANDLE& right) {
        return left.ptr < right.ptr;
    };

    struct D3D12Heap {
        void initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors = 32) {
            D3D12_DESCRIPTOR_HEAP_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            heapSize = desc.NumDescriptors = numDescriptors;
            desc.Type = type;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER || type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
                desc.Flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            }
            CHECK_HRCMD(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(set(heap))));
            heapStartCPU = heap->GetCPUDescriptorHandleForHeapStart();
            heapStartGPU = heap->GetGPUDescriptorHandleForHeapStart();
            heapOffset = 0;
            descSize = device->GetDescriptorHandleIncrementSize(type);
        }

        void allocate(D3D12_CPU_DESCRIPTOR_HANDLE& desc) {
            assert((UINT)heapOffset < heapSize);
            desc = CD3DX12_CPU_DESCRIPTOR_HANDLE(heapStartCPU, heapOffset++, descSize);
        }

        // TODO: Implement freeing a descriptor

        D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const {
            INT64 offset = (cpuHandle.ptr - heapStartCPU.ptr) / descSize;
            return CD3DX12_GPU_DESCRIPTOR_HANDLE(heapStartGPU, (INT)offset, descSize);
        }

        UINT heapSize{0};
        ComPtr<ID3D12DescriptorHeap> heap;
        D3D12_CPU_DESCRIPTOR_HANDLE heapStartCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE heapStartGPU;
        INT heapOffset{0};
        UINT descSize;
    };

    // Wrap a resource view. Obtained from D3D12Texture.
    class D3D12ResourceView : public IRenderTargetView,
                              public IDepthStencilView {
      public:
        D3D12ResourceView(std::shared_ptr<IDevice> device, D3D12_CPU_DESCRIPTOR_HANDLE resourceView)
            : m_device(device), m_resourceView(resourceView) {
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return reinterpret_cast<void*>(const_cast<D3D12_CPU_DESCRIPTOR_HANDLE*>(&m_resourceView));
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const D3D12_CPU_DESCRIPTOR_HANDLE m_resourceView;
    };

    // Wrap a texture resource. Obtained from D3D12Device.
    class D3D12Texture : public ITexture {
      public:
        D3D12Texture(std::shared_ptr<IDevice> device,
                     const XrSwapchainCreateInfo& info,
                     const D3D12_RESOURCE_DESC& textureDesc,
                     ID3D12Resource* texture,
                     D3D12Heap& rtvHeap,
                     D3D12Heap& dsvHeap,
                     D3D12Heap& rvHeap)
            : m_device(device), m_info(info), m_textureDesc(textureDesc), m_texture(texture), m_rtvHeap(rtvHeap),
              m_dsvHeap(dsvHeap), m_rvHeap(rvHeap) {
            m_shaderResourceSubView.resize(info.arraySize);
            m_unorderedAccessSubView.resize(info.arraySize);
            m_renderTargetSubView.resize(info.arraySize);
            m_depthStencilSubView.resize(info.arraySize);
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        const XrSwapchainCreateInfo& getInfo() const override {
            return m_info;
        }

        bool isArray() const override {
            return m_textureDesc.DepthOrArraySize > 1;
        }

        std::shared_ptr<IRenderTargetView> getRenderTargetView(int32_t slice) const override {
            assert(slice < 0 || m_renderTargetSubView.size() > size_t(slice));
            auto& view = slice < 0 ? m_renderTargetView : m_renderTargetSubView[slice];
            if (!view)
                view = makeRenderTargetViewInternal(std::max(slice, 0));
            return view;
        }

        std::shared_ptr<IDepthStencilView> getDepthStencilView(int32_t slice) const override {
            assert(slice < 0 || m_depthStencilSubView.size() > size_t(slice));
            auto& view = slice < 0 ? m_depthStencilView : m_depthStencilSubView[slice];
            if (!view)
                view = makeDepthStencilViewInternal(std::max(slice, 0));
            return view;
        }

        void uploadData(const void* buffer, uint32_t rowPitch, int32_t slice = -1) override {
            assert(!(rowPitch % m_device->getTextureAlignmentConstraint()));

            // Create an upload buffer if we don't have one already
            if (!m_uploadBuffer) {
                m_uploadSize = alignTo((UINT)m_textureDesc.Width, m_device->getTextureAlignmentConstraint()) *
                               m_textureDesc.Height;
                const auto& heapType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
                const auto stagingDesc = CD3DX12_RESOURCE_DESC::Buffer(m_uploadSize);
                CHECK_HRCMD(m_device->getAs<D3D12>()->CreateCommittedResource(&heapType,
                                                                              D3D12_HEAP_FLAG_NONE,
                                                                              &stagingDesc,
                                                                              D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                              nullptr,
                                                                              IID_PPV_ARGS(set(m_uploadBuffer))));
            }

            // Copy to the upload buffer.
            {
                void* mappedBuffer = nullptr;
                m_uploadBuffer->Map(0, nullptr, &mappedBuffer);
                memcpy(mappedBuffer, buffer, m_uploadSize);
                m_uploadBuffer->Unmap(0, nullptr);
            }

            // Do the upload now.
            if (auto context = m_device->getContextAs<D3D12>()) {
                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        get(m_texture), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                    context->ResourceBarrier(1, &barrier);
                }
                {
                    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
                    ZeroMemory(&footprint, sizeof(footprint));
                    footprint.Footprint.Width = (UINT)m_textureDesc.Width;
                    footprint.Footprint.Height = m_textureDesc.Height;
                    footprint.Footprint.Depth = 1;
                    footprint.Footprint.RowPitch = rowPitch;
                    footprint.Footprint.Format = m_textureDesc.Format;
                    CD3DX12_TEXTURE_COPY_LOCATION src(get(m_uploadBuffer), footprint);
                    CD3DX12_TEXTURE_COPY_LOCATION dst(get(m_texture), 0);
                    context->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                }
                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        get(m_texture), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
                    context->ResourceBarrier(1, &barrier);
                }
            }
        }

        void* getNativePtr() const override {
            return get(m_texture);
        }

      private:
        std::shared_ptr<D3D12ResourceView> makeRenderTargetViewInternal(uint32_t slice) const {
            if (!(m_textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
                throw std::runtime_error("Texture was not created with D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET");
            }
            if (auto device = m_device->getAs<D3D12>()) {
                D3D12_RENDER_TARGET_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = (DXGI_FORMAT)m_info.format;
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D12_RTV_DIMENSION_TEXTURE2D : D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipSlice = D3D12CalcSubresource(0, 0, 0, m_info.mipCount, m_info.arraySize);

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                m_rtvHeap.allocate(handle);
                device->CreateRenderTargetView(get(m_texture), &desc, handle);
                return std::make_shared<D3D12ResourceView>(m_device, handle);
            }
            return nullptr;
        }

        std::shared_ptr<D3D12ResourceView> makeDepthStencilViewInternal(uint32_t slice) const {
            if (!(m_textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) {
                throw std::runtime_error("Texture was not created with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL");
            }
            if (auto device = m_device->getAs<D3D12>()) {
                D3D12_DEPTH_STENCIL_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = m_textureDesc.Format;
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D12_DSV_DIMENSION_TEXTURE2D : D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipSlice = D3D12CalcSubresource(0, 0, 0, m_info.mipCount, m_info.arraySize);

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                m_dsvHeap.allocate(handle);
                device->CreateDepthStencilView(get(m_texture), &desc, handle);
                return std::make_shared<D3D12ResourceView>(m_device, handle);
            }
            return nullptr;
        }

        void setInteropTexture(std::shared_ptr<ITexture> interopTexture) {
            m_interopTexture = interopTexture;
        }

        std::shared_ptr<ITexture> getInteropTexture() {
            return m_interopTexture;
        }

        const std::shared_ptr<IDevice> m_device;
        const XrSwapchainCreateInfo m_info;
        const D3D12_RESOURCE_DESC m_textureDesc;
        const ComPtr<ID3D12Resource> m_texture;

        ComPtr<ID3D12Resource> m_uploadBuffer;
        UINT m_uploadSize{0};
        std::shared_ptr<ITexture> m_interopTexture;

        D3D12Heap& m_rtvHeap;
        D3D12Heap& m_dsvHeap;
        D3D12Heap& m_rvHeap;

        mutable std::shared_ptr<D3D12ResourceView> m_shaderResourceView;
        mutable std::vector<std::shared_ptr<D3D12ResourceView>> m_shaderResourceSubView;
        mutable std::shared_ptr<D3D12ResourceView> m_unorderedAccessView;
        mutable std::vector<std::shared_ptr<D3D12ResourceView>> m_unorderedAccessSubView;
        mutable std::shared_ptr<D3D12ResourceView> m_renderTargetView;
        mutable std::vector<std::shared_ptr<D3D12ResourceView>> m_renderTargetSubView;
        mutable std::shared_ptr<D3D12ResourceView> m_depthStencilView;
        mutable std::vector<std::shared_ptr<D3D12ResourceView>> m_depthStencilSubView;

        friend class D3D12Device;
    };

    class D3D12Buffer : public IShaderBuffer {
      public:
        D3D12Buffer(std::shared_ptr<IDevice> device,
                    D3D12_RESOURCE_DESC bufferDesc,
                    ID3D12Resource* buffer,
                    D3D12Heap& rvHeap,
                    ID3D12Resource* uploadBuffer = nullptr)
            : m_device(device), m_bufferDesc(bufferDesc), m_buffer(buffer), m_rvHeap(rvHeap),
              m_uploadBuffer(uploadBuffer) {
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void uploadData(const void* buffer, size_t count, ID3D12Resource* uploadBuffer) {
            D3D12_SUBRESOURCE_DATA subresourceData;
            subresourceData.pData = buffer;
            subresourceData.RowPitch = subresourceData.SlicePitch = count;

            if (auto context = m_device->getContextAs<D3D12>()) {
                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        get(m_buffer), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
                    context->ResourceBarrier(1, &barrier);
                }
                UpdateSubresources<1>(context, get(m_buffer), uploadBuffer, 0, 0, 1, &subresourceData);
                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        get(m_buffer), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
                    context->ResourceBarrier(1, &barrier);
                }
            }
        }

        void uploadData(const void* buffer, size_t count) override {
            if (!m_uploadBuffer) {
                throw std::runtime_error("Buffer is immutable");
            }
            uploadData(buffer, count, get(m_uploadBuffer));
        }

        // TODO: Consider moving this operation up to IShaderBuffer. Will prevent the need for dynamic_cast below.
        D3D12_CPU_DESCRIPTOR_HANDLE getConstantBufferView() const {
            if (!m_constantBufferView) {
                {
                    D3D12_CPU_DESCRIPTOR_HANDLE handle;
                    m_rvHeap.allocate(handle);
                    m_constantBufferView = handle;
                }

                if (auto device = m_device->getAs<D3D12>()) {
                    D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
                    desc.BufferLocation = m_buffer->GetGPUVirtualAddress();
                    desc.SizeInBytes =
                        alignTo(static_cast<UINT>(m_bufferDesc.Width), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
                    device->CreateConstantBufferView(&desc, m_constantBufferView.value());
                }
            }
            return m_constantBufferView.value();
        }

        void* getNativePtr() const override {
            return get(m_buffer);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const D3D12_RESOURCE_DESC m_bufferDesc;
        const ComPtr<ID3D12Resource> m_buffer;

        D3D12Heap& m_rvHeap;

        const ComPtr<ID3D12Resource> m_uploadBuffer;

        mutable std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> m_constantBufferView;
    };

    // Wrap a vertex+indices buffers. Obtained from D3D12Device.
    class D3D12SimpleMesh : public ISimpleMesh {
      public:
        D3D12SimpleMesh(std::shared_ptr<IDevice> device,
                        ID3D12Resource* vertexBuffer,
                        size_t stride,
                        ID3D12Resource* indexBuffer,
                        size_t numIndices)
            : m_device(device), m_vertexBuffer(vertexBuffer), m_indexBuffer(indexBuffer) {
            vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
            vbView.StrideInBytes = (UINT)stride;
            vbView.SizeInBytes = (UINT)vertexBuffer->GetDesc().Width;
            m_meshData.vertexBuffer = &vbView;
            ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
            ibView.Format = DXGI_FORMAT_R16_UINT;
            ibView.SizeInBytes = (UINT)indexBuffer->GetDesc().Width;
            m_meshData.indexBuffer = &ibView;
            m_meshData.numIndices = (UINT)numIndices;
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return reinterpret_cast<void*>(&m_meshData);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D12Resource> m_vertexBuffer;
        const ComPtr<ID3D12Resource> m_indexBuffer;

        D3D12_VERTEX_BUFFER_VIEW vbView;
        D3D12_INDEX_BUFFER_VIEW ibView;
        mutable struct D3D12::MeshData m_meshData;
    };

    // Wrap a device context.
    class D3D12Context : public graphics::IContext {
      public:
        D3D12Context(std::shared_ptr<IDevice> device, ID3D12GraphicsCommandList* context)
            : m_device(device), m_context(context) {
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return get(m_context);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D12GraphicsCommandList> m_context;
    };

    class D3D12Device : public IDevice, public std::enable_shared_from_this<D3D12Device> {
      private:
        // OpenXR will not allow more than 2 frames in-flight, so 2 would be sufficient, however we might split the
        // processing in two due to text rendering, so multiply this number by 2. Oh and also we have the app GPU
        // timer, so multiply again by 2.
       
        static constexpr size_t NumInflightContexts = 8;

      public:
        D3D12Device(ID3D12Device* device, ID3D12CommandQueue* queue)
            : m_device(device), m_queue(queue)
        {
            {
                // store a reference to the command queue for easier retrieval
                m_device->SetPrivateDataInterface(IID_ID3D12CommandQueue, get(m_queue));

                ComPtr<IDXGIFactory1> dxgiFactory;
                CHECK_HRCMD(CreateDXGIFactory1(IID_PPV_ARGS(set(dxgiFactory))));
                const LUID adapterLuid = m_device->GetAdapterLuid();

                for (UINT adapterIndex = 0;; adapterIndex++)
                {
                    // EnumAdapters1 will fail with DXGI_ERROR_NOT_FOUND when there are no more adapters to enumerate.
                    ComPtr<IDXGIAdapter1> dxgiAdapter;
                    CHECK_HRCMD(dxgiFactory->EnumAdapters1(adapterIndex, set(dxgiAdapter)));

                    DXGI_ADAPTER_DESC1 adapterDesc;
                    CHECK_HRCMD(dxgiAdapter->GetDesc1(&adapterDesc));
                    if (!memcmp(&adapterDesc.AdapterLuid, &adapterLuid, sizeof(adapterLuid)))
                    {
                        const std::wstring wadapterDescription(adapterDesc.Description);
                        std::transform(wadapterDescription.begin(),
                                       wadapterDescription.end(),
                                       std::back_inserter(m_deviceName),
                                       [](wchar_t c) { return (char)c; });

                        // Log the adapter name to help debugging customer issues.
                        Log("Using Direct3D 12 on adapter: %s\n", m_deviceName.c_str());
                        break;
                    }
                }
            }

#ifdef _DEBUG
            // Initialize Debug layer logging.
            if (SUCCEEDED(m_device->QueryInterface(set(m_infoQueue))))
            {
                Log("D3D12 Debug layer is enabled\n");

                // Disable some common warnings.
                D3D12_MESSAGE_ID messages[] = {
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_CREATERESOURCE_CLEARVALUEDENORMFLUSH,
                    D3D12_MESSAGE_ID_REFLECTSHAREDPROPERTIES_INVALIDOBJECT, // Caused by D3D11on12.
                };
                D3D12_INFO_QUEUE_FILTER filter;
                ZeroMemory(&filter, sizeof(filter));
                filter.DenyList.NumIDs = ARRAYSIZE(messages);
                filter.DenyList.pIDList = messages;
                m_infoQueue->AddStorageFilterEntries(&filter);
            }
            else
            {
                Log("Failed to enable debug layer - please check that the 'Graphics Tools' feature of Windows is "
                    "installed\n");
            }
#endif
        
            // Initialize the command lists and heaps.
            m_rtvHeap.initialize(get(m_device), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            m_dsvHeap.initialize(get(m_device), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            m_rvHeap.initialize(get(m_device), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32 + MaxModelBuffers);
           
            {
                for (uint32_t i = 0; i < NumInflightContexts; i++) {
                    CHECK_HRCMD(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                 IID_PPV_ARGS(&m_commandAllocator[i])));
                    CHECK_HRCMD(m_device->CreateCommandList(0,
                                                            D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                            get(m_commandAllocator[i]),
                                                            nullptr,
                                                            IID_PPV_ARGS(&m_commandList[i])));

                    // Set to a known state.
                    if (i != 0) {
                        CHECK_HRCMD(m_commandList[i]->Close());
                    }
                }
                m_currentContext = 0;
                m_context = m_commandList[0];
            }

            CHECK_HRCMD(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(set(m_fence))));
            initializeMeshResources();
        }

        ~D3D12Device() override {
            Log("D3D12Device destroyed\n");
        }

        void shutdown() override {
            // Log some statistics for sizing.
            DebugLog("heap statistics:rtv=%u/%u, dsv=%u/%u, rv=%u/%u\n",
                     m_rtvHeap.heapOffset,
                     m_rtvHeap.heapSize,
                     m_dsvHeap.heapOffset,
                     m_dsvHeap.heapSize,
                     m_rvHeap.heapOffset,
                     m_rvHeap.heapSize);

            // Clear all references that could hold a cyclic reference themselves.
            m_currentDrawRenderTarget.reset();
            m_currentDrawDepthBuffer.reset();

            m_currentMesh.reset();
            for (uint32_t i = 0; i < ARRAYSIZE(m_meshViewProjectionBuffer); i++) {
                m_meshViewProjectionBuffer[i].reset();
            }
            for (uint32_t i = 0; i < ARRAYSIZE(m_meshModelBuffer); i++) {
                m_meshModelBuffer[i].reset();
            }

            m_device->SetPrivateDataInterface(IID_ID3D12CommandQueue, nullptr);
        }

        Api getApi() const override {
            return Api::D3D12;
        }

        void saveContext(bool clear) override {
            // We make the assumption that the context saving/restoring is only used once per xrEndFrame() to avoid
            // trashing the application state. In D3D12, there is no such issue since the command list is separate from
            // the command queue.
        }

        void restoreContext() override {
            // We make the assumption that the context saving/restoring is only used once per xrEndFrame() to avoid
            // trashing the application state. In D3D12, there is no such issue since the command list is separate from
            // the command queue.
        }

        void flushContext(bool blocking, bool isEndOfFrame = false) override {

            CHECK_HRCMD(m_context->Close());

            ID3D12CommandList* const lists[] = {get(m_context)};
            m_queue->ExecuteCommandLists(ARRAYSIZE(lists), lists);

            if (blocking) {
                m_queue->Signal(get(m_fence), ++m_fenceValue);
                if (m_fence->GetCompletedValue() < m_fenceValue) {
                    HANDLE eventHandle = CreateEventEx(nullptr, "flushContext Fence", 0, EVENT_ALL_ACCESS);
                    CHECK_HRCMD(m_fence->SetEventOnCompletion(m_fenceValue, eventHandle));
                    WaitForSingleObject(eventHandle, INFINITE);
                    CloseHandle(eventHandle);
                }
            }

            if (++m_currentContext == NumInflightContexts) {
                m_currentContext = 0;
            }
            CHECK_HRCMD(m_commandAllocator[m_currentContext]->Reset());
            CHECK_HRCMD(m_commandList[m_currentContext]->Reset(get(m_commandAllocator[m_currentContext]), nullptr));
            m_context = m_commandList[m_currentContext];

            // Log any messages from the Debug layer.
            if (auto count = m_infoQueue ? m_infoQueue->GetNumStoredMessages() : 0) {
                LogInfoQueueMessage(get(m_infoQueue), count);
                m_infoQueue->ClearStoredMessages();
            }
        }

        std::shared_ptr<ITexture> createTexture(const XrSwapchainCreateInfo& info,
                                                std::string_view debugName,
                                                int64_t overrideFormat = 0,
                                                uint32_t rowPitch = 0,
                                                uint32_t imageSize = 0,
                                                const void* initialData = nullptr) override {
            assert(!(rowPitch % getTextureAlignmentConstraint()));

            auto desc = CD3DX12_RESOURCE_DESC::Tex2D((DXGI_FORMAT)(!overrideFormat ? info.format : overrideFormat),
                                                     info.width,
                                                     info.height,
                                                     info.arraySize,
                                                     info.mipCount,
                                                     info.sampleCount);

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
            const auto& heapType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            // Use the shared flag to allow cross-API interop (eg: Vulkan).
            CHECK_HRCMD(m_device->CreateCommittedResource(
                &heapType, D3D12_HEAP_FLAG_SHARED, &desc, initialState, nullptr, IID_PPV_ARGS(set(texture))));

            if (initialData) {
                // Create an upload buffer.
                ComPtr<ID3D12Resource> uploadBuffer;
                {
                    const auto& heapType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
                    const auto stagingDesc = CD3DX12_RESOURCE_DESC::Buffer(imageSize);
                    CHECK_HRCMD(m_device->CreateCommittedResource(&heapType,
                                                                  D3D12_HEAP_FLAG_NONE,
                                                                  &stagingDesc,
                                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                  nullptr,
                                                                  IID_PPV_ARGS(set(uploadBuffer))));
                }
                {
                    void* mappedBuffer = nullptr;
                    uploadBuffer->Map(0, nullptr, &mappedBuffer);
                    memcpy(mappedBuffer, initialData, imageSize);
                    uploadBuffer->Unmap(0, nullptr);
                }

                // Do the upload now.
                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        get(texture), initialState, D3D12_RESOURCE_STATE_COPY_DEST);
                    m_context->ResourceBarrier(1, &barrier);
                }
                {
                    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
                    ZeroMemory(&footprint, sizeof(footprint));
                    footprint.Footprint.Width = (UINT)desc.Width;
                    footprint.Footprint.Height = desc.Height;
                    footprint.Footprint.Depth = 1;
                    footprint.Footprint.RowPitch = rowPitch;
                    footprint.Footprint.Format = desc.Format;
                    CD3DX12_TEXTURE_COPY_LOCATION src(get(uploadBuffer), footprint);
                    CD3DX12_TEXTURE_COPY_LOCATION dst(get(texture), 0);
                    m_context->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                }
                {
                    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                        get(texture), D3D12_RESOURCE_STATE_COPY_DEST, initialState);
                    m_context->ResourceBarrier(1, &barrier);
                }
                flushContext(true);
            }
            SetDebugName(get(texture), debugName);

            return std::make_shared<D3D12Texture>(
                shared_from_this(), info, desc, get(texture), m_rtvHeap, m_dsvHeap, m_rvHeap);
        }

        std::shared_ptr<IShaderBuffer>
        createBuffer(size_t size, std::string_view debugName, const void* initialData, bool immutable) override {
            const auto desc =
                CD3DX12_RESOURCE_DESC::Buffer(alignTo(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));

            ComPtr<ID3D12Resource> buffer;
            {
                const auto& heapType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                CHECK_HRCMD(m_device->CreateCommittedResource(&heapType,
                                                              D3D12_HEAP_FLAG_NONE,
                                                              &desc,
                                                              D3D12_RESOURCE_STATE_COMMON,
                                                              nullptr,
                                                              IID_PPV_ARGS(set(buffer))));
            }

            // Create an upload buffer.
            ComPtr<ID3D12Resource> uploadBuffer;
            if (initialData || !immutable) {
                const auto& heapType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
                CHECK_HRCMD(m_device->CreateCommittedResource(&heapType,
                                                              D3D12_HEAP_FLAG_NONE,
                                                              &desc,
                                                              D3D12_RESOURCE_STATE_GENERIC_READ,
                                                              nullptr,
                                                              IID_PPV_ARGS(set(uploadBuffer))));
            }
            SetDebugName(get(buffer), debugName);

            auto result = std::make_shared<D3D12Buffer>(shared_from_this(),
                                                        desc,
                                                        get(buffer),
                                                        m_rvHeap,
                                                        !immutable ? get(uploadBuffer) : nullptr);

            if (initialData) {
                result->uploadData(initialData, size, get(uploadBuffer));
                flushContext(true);
            }

            return result;
        }

        std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                      std::vector<uint16_t>& indices,
                                                      std::string_view debugName) override {
            std::shared_ptr<IShaderBuffer> vertexBuffer =
                createBuffer(vertices.size() * sizeof(SimpleMeshVertex), debugName, vertices.data(), true);

            std::shared_ptr<IShaderBuffer> indexBuffer =
                createBuffer(indices.size() * sizeof(uint16_t), debugName, indices.data(), true);

            return std::make_shared<D3D12SimpleMesh>(shared_from_this(),
                                                     vertexBuffer->getAs<D3D12>(),
                                                     sizeof(SimpleMeshVertex),
                                                     indexBuffer->getAs<D3D12>(),
                                                     indices.size());
        }

        void unsetRenderTargets() override
        {
            m_context->OMSetRenderTargets(0, nullptr, true, nullptr);
            m_currentDrawRenderTarget.reset();
            m_currentDrawDepthBuffer.reset();
            m_currentMesh.reset();
        }

        void setRenderTargets(size_t numRenderTargets,
                              std::shared_ptr<ITexture>* renderTargets,
                              int32_t* renderSlices = nullptr,
                              const XrRect2Di* viewport0 = nullptr,
                              std::shared_ptr<ITexture> depthBuffer = nullptr,
                              int32_t depthSlice = -1) override {
            assert(renderTargets || !numRenderTargets);
            assert(depthBuffer || depthSlice < 0);

            D3D12_CPU_DESCRIPTOR_HANDLE rtvs[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};

            if (numRenderTargets > size_t(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT))
                numRenderTargets = size_t(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

            for (size_t i = 0; i < numRenderTargets; i++) {
                auto slice = renderSlices ? renderSlices[i] : -1;
                rtvs[i] = *renderTargets[i]->getRenderTargetView(slice)->getAs<D3D12>();

                // We assume that the resource is always in the expected state.
                // barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[i]->getAs<D3D12>(),
                //                                                         D3D12_RESOURCE_STATE_COMMON,
                //                                                         D3D12_RESOURCE_STATE_RENDER_TARGET));
            }

            // if (depthBuffer) {
            // We assume that the resource is always in the expected state.
            // barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(depthBuffer[0]->getAs<D3D12>(),
            //                                                         D3D12_RESOURCE_STATE_COMMON,
            //                                                         D3D12_RESOURCE_STATE_DEPTH_WRITE));
            //}

            // if (!barriers.empty()) {
            //    m_context->ResourceBarrier((UINT)barriers.size(), barriers.data());
            //}

            auto pDepthStencilView =
                depthBuffer ? depthBuffer->getDepthStencilView(depthSlice)->getAs<D3D12>() : nullptr;

            m_context->OMSetRenderTargets(static_cast<UINT>(numRenderTargets), rtvs, false, pDepthStencilView);

            if (numRenderTargets) {
                m_currentDrawRenderTarget = renderTargets[0];
                m_currentDrawRenderTargetSlice = renderSlices ? renderSlices[0] : -1;
                m_currentDrawDepthBuffer = std::move(depthBuffer);
                m_currentDrawDepthBufferSlice = depthSlice;

                XrRect2Di viewportRect;
                if (viewport0)
                {
                    viewportRect = *viewport0;
                }
                else
                {
                    viewportRect.offset = {0, 0};
                    viewportRect.extent.width = m_currentDrawRenderTarget->getInfo().width;
                    viewportRect.extent.height = m_currentDrawRenderTarget->getInfo().height;
                }

                const auto viewport = CD3DX12_VIEWPORT((float)viewportRect.offset.x,
                                                       (float)viewportRect.offset.y,
                                                       (float)viewportRect.extent.width,
                                                       (float)viewportRect.extent.height);
                m_context->RSSetViewports(1, &viewport);

                const auto scissorRect = CD3DX12_RECT(
                    0, 0, m_currentDrawRenderTarget->getInfo().width, m_currentDrawRenderTarget->getInfo().height);

                m_context->RSSetScissorRects(1, &scissorRect);
            } else {
                m_currentDrawRenderTarget.reset();
                m_currentDrawDepthBuffer.reset();
            }
            m_currentMesh.reset();
        }

        void clearDepth(float value) override {
            if (m_currentDrawDepthBuffer) {
                auto depthStencilView =
                    m_currentDrawDepthBuffer->getDepthStencilView(m_currentDrawDepthBufferSlice)->getAs<D3D12>();

                m_context->ClearDepthStencilView(*depthStencilView, D3D12_CLEAR_FLAG_DEPTH, value, 0, 0, nullptr);
            }
        }

        void setViewProjection(const xr::math::ViewProjection& view) override {
            ViewProjectionConstantBuffer staging;

            // viewMatrix* projMatrix
            DirectX::XMStoreFloat4x4(
                &staging.ViewProjection,
                DirectX::XMMatrixTranspose(xr::math::LoadInvertedXrPose(view.Pose) *
                                           xr::math::ComposeProjectionMatrix(view.Fov, view.NearFar)));

            m_currentMeshViewProjectionBuffer++;
            if (m_currentMeshViewProjectionBuffer >= ARRAYSIZE(m_meshViewProjectionBuffer)) {
                m_currentMeshViewProjectionBuffer = 0;
            }
            if (!m_meshViewProjectionBuffer[m_currentMeshViewProjectionBuffer]) {
                m_meshViewProjectionBuffer[m_currentMeshViewProjectionBuffer] =
                    createBuffer(sizeof(ViewProjectionConstantBuffer), "ViewProjection CB", nullptr, false);
            }
            m_meshViewProjectionBuffer[m_currentMeshViewProjectionBuffer]->uploadData(&staging, sizeof(staging));

            m_currentDrawDepthBufferIsInverted = view.NearFar.Near > view.NearFar.Far;
        }

        void draw(std::shared_ptr<ISimpleMesh> mesh, const XrPosef& pose, XrVector3f scaling) override {
            auto meshData = mesh->getAs<D3D12>();
            if (!meshData)
                return;

            if (mesh != m_currentMesh) {
                // Lazily construct the pipeline state now that we know the format for the render target and whether
                // depth is inverted.
                // TODO: We must support the RTV format changing.
                if (!m_meshRendererPipelineState) {
                    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
                    ZeroMemory(&desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
                    desc.InputLayout = {m_meshRendererInputLayout.data(), (UINT)m_meshRendererInputLayout.size()};
                    desc.pRootSignature = get(m_meshRendererRootSignature);
                    desc.VS = {reinterpret_cast<BYTE*>(m_meshRendererVertexShaderBytes->GetBufferPointer()),
                               m_meshRendererVertexShaderBytes->GetBufferSize()};
                    desc.PS = {reinterpret_cast<BYTE*>(m_meshRendererPixelShaderBytes->GetBufferPointer()),
                               m_meshRendererPixelShaderBytes->GetBufferSize()};
                    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
                    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
                    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
                    if (m_currentDrawDepthBufferIsInverted) {
                        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
                        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
                    }
                    desc.SampleMask = UINT_MAX;
                    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                    desc.NumRenderTargets = 1;
                    desc.RTVFormats[0] = (DXGI_FORMAT)m_currentDrawRenderTarget->getInfo().format;
                    desc.SampleDesc.Count = m_currentDrawRenderTarget->getInfo().sampleCount;
                    if (desc.SampleDesc.Count > 1) {
                        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
                        qualityLevels.Format = desc.RTVFormats[0];
                        qualityLevels.SampleCount = desc.SampleDesc.Count;
                        qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
                        CHECK_HRCMD(m_device->CheckFeatureSupport(
                            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualityLevels, sizeof(qualityLevels)));

                        // Setup for highest quality multisampling if requested.
                        desc.SampleDesc.Quality = qualityLevels.NumQualityLevels - 1;
                        desc.RasterizerState.MultisampleEnable = true;
                    }
                    if (m_currentDrawDepthBuffer) {
                        desc.DSVFormat = (DXGI_FORMAT)m_currentDrawDepthBuffer->getInfo().format;
                    }
                    CHECK_HRCMD(
                        m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(set(m_meshRendererPipelineState))));
                }

                m_context->SetPipelineState(get(m_meshRendererPipelineState));
                m_context->SetGraphicsRootSignature(get(m_meshRendererRootSignature));
                m_context->IASetVertexBuffers(0, 1, meshData->vertexBuffer);
                m_context->IASetIndexBuffer(meshData->indexBuffer);
                m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                ID3D12DescriptorHeap* const heaps[] = {
                    get(m_rvHeap.heap),
                };
                m_context->SetDescriptorHeaps(ARRAYSIZE(heaps), heaps);

                {
                    auto d3d12Buffer =
                        dynamic_cast<D3D12Buffer*>(m_meshViewProjectionBuffer[m_currentMeshViewProjectionBuffer].get());
                    const auto& handle = d3d12Buffer->getConstantBufferView();
                    m_context->SetGraphicsRootDescriptorTable(1, m_rvHeap.getGPUHandle(handle));
                }

                m_currentMesh = mesh;
            }

            ModelConstantBuffer model;
            const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(scaling.x, scaling.y, scaling.z);
            DirectX::XMStoreFloat4x4(&model.Model,
                                     DirectX::XMMatrixTranspose(scaleMatrix * xr::math::LoadXrPose(pose)));

            m_currentMeshModelBuffer++;
            if (m_currentMeshModelBuffer >= ARRAYSIZE(m_meshModelBuffer)) {
                m_currentMeshModelBuffer = 0;
            }

            if (!m_meshModelBuffer[m_currentMeshModelBuffer]) {
                m_meshModelBuffer[m_currentMeshModelBuffer] =
                    createBuffer(sizeof(ModelConstantBuffer), "Model CB", nullptr, false);
            }
            m_meshModelBuffer[m_currentMeshModelBuffer]->uploadData(&model, sizeof(model));

            {
                auto d3d12Buffer = dynamic_cast<D3D12Buffer*>(m_meshModelBuffer[m_currentMeshModelBuffer].get());
                const auto& handle = d3d12Buffer->getConstantBufferView();
                m_context->SetGraphicsRootDescriptorTable(0, m_rvHeap.getGPUHandle(handle));
            }

            m_context->DrawIndexedInstanced(meshData->numIndices, 1, 0, 0, 0);
        }

        uint32_t getTextureAlignmentConstraint() const override {
            return D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
        }

        void* getNativePtr() const override {
            return get(m_device);
        }

        void* getContextPtr() const override {
            return get(m_context);
        }

      private:

        // Initialize the calls needed for draw() and related calls.
        void initializeMeshResources() {
            {
                ComPtr<ID3DBlob> errors;
                const HRESULT hr = D3DCompile(MeshShaders.data(),
                                              MeshShaders.length(),
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              "vsMain",
                                              "vs_5_0",
                                              D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
                                              0,
                                              set(m_meshRendererVertexShaderBytes),
                                              set(errors));
                if (FAILED(hr)) {
                    if (errors) {
                        Log("%s", (char*)errors->GetBufferPointer());
                    }
                    CHECK_HRESULT(hr, "Failed to compile shader");
                }
            }
            {
                ComPtr<ID3DBlob> errors;
                const HRESULT hr = D3DCompile(MeshShaders.data(),
                                              MeshShaders.length(),
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              "psMain",
                                              "ps_5_0",
                                              D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
                                              0,
                                              set(m_meshRendererPixelShaderBytes),
                                              set(errors));
                if (FAILED(hr)) {
                    if (errors) {
                        Log("%s", (char*)errors->GetBufferPointer());
                    }
                    CHECK_HRESULT(hr, "Failed to compile shader");
                }
            }
            {
                m_meshRendererInputLayout.push_back(
                    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
                m_meshRendererInputLayout.push_back(
                    {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
            }
            {
                CD3DX12_ROOT_PARAMETER parametersDescriptors[2];
                const auto rangeParam1 = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
                parametersDescriptors[0].InitAsDescriptorTable(1, &rangeParam1);
                const auto rangeParam2 = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
                parametersDescriptors[1].InitAsDescriptorTable(1, &rangeParam2);

                CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(parametersDescriptors),
                                                 parametersDescriptors,
                                                 0,
                                                 nullptr,
                                                 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

                ComPtr<ID3DBlob> serializedRootSignature;
                ComPtr<ID3DBlob> errors;
                const HRESULT hr = D3D12SerializeRootSignature(
                    &desc, D3D_ROOT_SIGNATURE_VERSION_1, set(serializedRootSignature), set(errors));
                if (FAILED(hr)) {
                    if (errors) {
                        Log("%s", (char*)errors->GetBufferPointer());
                    }
                    CHECK_HRESULT(hr, "Failed to serialize root signature");
                }

                CHECK_HRCMD(m_device->CreateRootSignature(0,
                                                          serializedRootSignature->GetBufferPointer(),
                                                          serializedRootSignature->GetBufferSize(),
                                                          IID_PPV_ARGS(set(m_meshRendererRootSignature))));
            }
        }

        const ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12CommandQueue> m_queue;
        std::string m_deviceName;

        ComPtr<ID3D12CommandAllocator> m_commandAllocator[NumInflightContexts];
        ComPtr<ID3D12GraphicsCommandList> m_commandList[NumInflightContexts];
        size_t m_currentContext{0};

        ComPtr<ID3D12GraphicsCommandList> m_context;
        D3D12Heap m_rtvHeap;
        D3D12Heap m_dsvHeap;
        D3D12Heap m_rvHeap;
        std::shared_ptr<IShaderBuffer> m_meshViewProjectionBuffer[4];
        uint32_t m_currentMeshViewProjectionBuffer{0};
        std::shared_ptr<IShaderBuffer> m_meshModelBuffer[MaxModelBuffers];
        uint32_t m_currentMeshModelBuffer{0};
        ComPtr<ID3DBlob> m_meshRendererVertexShaderBytes;
        std::vector<D3D12_INPUT_ELEMENT_DESC> m_meshRendererInputLayout;
        ComPtr<ID3DBlob> m_meshRendererPixelShaderBytes;
        ComPtr<ID3D12RootSignature> m_meshRendererRootSignature;
        ComPtr<ID3D12PipelineState> m_meshRendererPipelineState;
        ComPtr<ID3D12Fence> m_fence;
        UINT64 m_fenceValue{0};

        std::shared_ptr<ITexture> m_currentDrawRenderTarget;
        int32_t m_currentDrawRenderTargetSlice;
        std::shared_ptr<ITexture> m_currentDrawDepthBuffer;
        int32_t m_currentDrawDepthBufferSlice;
        bool m_currentDrawDepthBufferIsInverted;

        std::shared_ptr<ISimpleMesh> m_currentMesh;

        ComPtr<ID3D12InfoQueue> m_infoQueue;

        friend std::shared_ptr<ITexture> graphics::WrapD3D12Texture(std::shared_ptr<IDevice> device,
                                                                    const XrSwapchainCreateInfo& info,
                                                                    ID3D12Resource* texture,
                                                                    std::string_view debugName);

        static XrSwapchainCreateInfo getTextureInfo(const D3D12_RESOURCE_DESC& resourceDesc) {
            XrSwapchainCreateInfo info;
            ZeroMemory(&info, sizeof(info));
            info.format = (int64_t)resourceDesc.Format;
            info.width = (uint32_t)resourceDesc.Width;
            info.height = resourceDesc.Height;
            info.arraySize = resourceDesc.DepthOrArraySize;
            info.mipCount = resourceDesc.MipLevels;
            info.sampleCount = resourceDesc.SampleDesc.Count;
            if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            }
            if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            if (!(resourceDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            }
            if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
            }

            return info;
        }

        static void LogInfoQueueMessage(ID3D12InfoQueue* infoQueue, UINT64 messageCount) {
            assert(infoQueue);
            for (UINT64 i = 0u; i < messageCount; i++) {
                SIZE_T size = 0;
                infoQueue->GetMessage(i, nullptr, &size);
                if (size) {
                    auto message_data = std::make_unique<char[]>(size);
                    auto message = reinterpret_cast<D3D12_MESSAGE*>(message_data.get());
                    CHECK_HRCMD(infoQueue->GetMessage(i, message, &size));
                    Log("D3D12: %.*s\n", message->DescriptionByteLength, message->pDescription);
                }
            }
        }  
    };

} // namespace

namespace graphics {

    std::shared_ptr<IDevice> WrapD3D12Device(ID3D12Device* device,
                                             ID3D12CommandQueue* queue) {
        return std::make_shared<D3D12Device>(device, queue);
    }

    std::shared_ptr<ITexture> WrapD3D12Texture(std::shared_ptr<IDevice> device,
                                               const XrSwapchainCreateInfo& info,
                                               ID3D12Resource* texture,
                                               std::string_view debugName) {
        if (device->getApi() == Api::D3D12) {
            SetDebugName(texture, debugName);

            auto d3d12Device = dynamic_cast<D3D12Device*>(device.get());
            return std::make_shared<D3D12Texture>(device,
                                                  info,
                                                  texture->GetDesc(),
                                                  texture,
                                                  d3d12Device->m_rtvHeap,
                                                  d3d12Device->m_dsvHeap,
                                                  d3d12Device->m_rvHeap);
        }
        throw std::runtime_error("Not a D3D12 device");
    }

} // namespace graphics
