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
#include <log.h>

#include <wincodec.h>

namespace {

    using namespace graphics;
    using namespace graphics::d3dcommon;
    using namespace motion_compensation_layer::log;

    inline void SetDebugName(ID3D11DeviceChild* resource, std::string_view name) {
        if (resource && !name.empty())
            resource->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
    }

    struct D3D11ContextState {
        ComPtr<ID3D11InputLayout> inputLayout;
        D3D11_PRIMITIVE_TOPOLOGY topology;
        ComPtr<ID3D11Buffer> vertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        UINT vertexBufferStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        UINT vertexBufferOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

        ComPtr<ID3D11Buffer> indexBuffer;
        DXGI_FORMAT indexBufferFormat;
        UINT indexBufferOffset;

        ComPtr<ID3D11RenderTargetView> renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        ComPtr<ID3D11DepthStencilView> depthStencil;
        ComPtr<ID3D11DepthStencilState> depthStencilState;
        UINT stencilRef;
        ComPtr<ID3D11BlendState> blendState;
        float blendFactor[4];
        UINT blendMask;

#define SHADER_STAGE_STATE(stage, programType)                                                                         \
    ComPtr<programType> stage##Program;                                                                                \
    ComPtr<ID3D11Buffer> stage##ConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];                    \
    ComPtr<ID3D11SamplerState> stage##Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];                                 \
    ComPtr<ID3D11ShaderResourceView> stage##ShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

        SHADER_STAGE_STATE(VS, ID3D11VertexShader);
        SHADER_STAGE_STATE(PS, ID3D11PixelShader);
        SHADER_STAGE_STATE(GS, ID3D11GeometryShader);
        SHADER_STAGE_STATE(DS, ID3D11DomainShader);
        SHADER_STAGE_STATE(HS, ID3D11HullShader);
        SHADER_STAGE_STATE(CS, ID3D11ComputeShader);

#undef SHADER_STAGE_STATE

        ComPtr<ID3D11UnorderedAccessView> CSUnorderedResources[D3D11_1_UAV_SLOT_COUNT];

        ComPtr<ID3D11RasterizerState> rasterizerState;
        D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        UINT numViewports;
        D3D11_RECT scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        UINT numScissorRects;

        void save(ID3D11DeviceContext* context) {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D11ContextState_Save");

            context->IAGetInputLayout(set(inputLayout));
            context->IAGetPrimitiveTopology(&topology);
            {
                ID3D11Buffer* vbs[ARRAYSIZE(vertexBuffers)];
                context->IAGetVertexBuffers(0, ARRAYSIZE(vbs), vbs, vertexBufferStrides, vertexBufferOffsets);
                for (uint32_t i = 0; i < ARRAYSIZE(vbs); i++) {
                    attach(vertexBuffers[i], vbs[i]);
                }
            }
            context->IAGetIndexBuffer(set(indexBuffer), &indexBufferFormat, &indexBufferOffset);

            {
                ID3D11RenderTargetView* rtvs[ARRAYSIZE(renderTargets)];
                context->OMGetRenderTargets(ARRAYSIZE(rtvs), rtvs, set(depthStencil));
                for (uint32_t i = 0; i < ARRAYSIZE(rtvs); i++) {
                    attach(renderTargets[i], rtvs[i]);
                }
            }

            context->OMGetDepthStencilState(set(depthStencilState), &stencilRef);
            context->OMGetBlendState(set(blendState), blendFactor, &blendMask);

#define SHADER_STAGE_SAVE_CONTEXT(stage)                                                                               \
    context->stage##GetShader(set(stage##Program), nullptr, nullptr);                                                  \
    {                                                                                                                  \
        ID3D11Buffer* buffers[ARRAYSIZE(stage##ConstantBuffers)];                                                      \
        context->stage##GetConstantBuffers(0, ARRAYSIZE(buffers), buffers);                                            \
        for (uint32_t i = 0; i < ARRAYSIZE(buffers); i++) {                                                            \
            attach(stage##ConstantBuffers[i], buffers[i]);                                                             \
        }                                                                                                              \
    }                                                                                                                  \
    {                                                                                                                  \
        ID3D11SamplerState* samp[ARRAYSIZE(stage##Samplers)];                                                          \
        context->stage##GetSamplers(0, ARRAYSIZE(samp), samp);                                                         \
        for (uint32_t i = 0; i < ARRAYSIZE(samp); i++) {                                                               \
            attach(stage##Samplers[i], samp[i]);                                                                       \
        }                                                                                                              \
    }                                                                                                                  \
    {                                                                                                                  \
        ID3D11ShaderResourceView* srvs[ARRAYSIZE(stage##ShaderResources)];                                             \
        context->stage##GetShaderResources(0, ARRAYSIZE(srvs), srvs);                                                  \
        for (uint32_t i = 0; i < ARRAYSIZE(srvs); i++) {                                                               \
            attach(stage##ShaderResources[i], srvs[i]);                                                                \
        }                                                                                                              \
    }

            SHADER_STAGE_SAVE_CONTEXT(VS);
            SHADER_STAGE_SAVE_CONTEXT(PS);
            SHADER_STAGE_SAVE_CONTEXT(GS);
            SHADER_STAGE_SAVE_CONTEXT(DS);
            SHADER_STAGE_SAVE_CONTEXT(HS);
            SHADER_STAGE_SAVE_CONTEXT(CS);

#undef SHADER_STAGE_SAVE_CONTEXT

            {
                ID3D11UnorderedAccessView* uavs[ARRAYSIZE(CSUnorderedResources)];
                context->CSGetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs);
                for (uint32_t i = 0; i < ARRAYSIZE(uavs); i++) {
                    attach(CSUnorderedResources[i], uavs[i]);
                }
            }

            context->RSGetState(set(rasterizerState));
            numViewports = ARRAYSIZE(viewports);
            context->RSGetViewports(&numViewports, viewports);
            numScissorRects = ARRAYSIZE(scissorRects);
            context->RSGetScissorRects(&numScissorRects, scissorRects);

            m_isValid = true;

            TraceLoggingWriteStop(local, "D3D11ContextState_Save");
        }

        void restore(ID3D11DeviceContext* context) const {
            TraceLocalActivity(local);
            TraceLoggingWriteStart(local, "D3D11ContextState_Restore");

            context->IASetInputLayout(get(inputLayout));
            context->IASetPrimitiveTopology(topology);
            {
                ID3D11Buffer* vbs[ARRAYSIZE(vertexBuffers)];
                for (uint32_t i = 0; i < ARRAYSIZE(vbs); i++) {
                    vbs[i] = get(vertexBuffers[i]);
                }
                context->IASetVertexBuffers(0, ARRAYSIZE(vbs), vbs, vertexBufferStrides, vertexBufferOffsets);
            }
            context->IASetIndexBuffer(get(indexBuffer), indexBufferFormat, indexBufferOffset);

            {
                ID3D11RenderTargetView* rtvs[ARRAYSIZE(renderTargets)];
                for (uint32_t i = 0; i < ARRAYSIZE(rtvs); i++) {
                    rtvs[i] = get(renderTargets[i]);
                }
                context->OMSetRenderTargets(ARRAYSIZE(rtvs), rtvs, get(depthStencil));
            }
            context->OMSetDepthStencilState(get(depthStencilState), stencilRef);
            context->OMSetBlendState(get(blendState), blendFactor, blendMask);

#define SHADER_STAGE_RESTORE_CONTEXT(stage)                                                                            \
    context->stage##SetShader(get(stage##Program), nullptr, 0);                                                        \
    {                                                                                                                  \
        ID3D11Buffer* buffers[ARRAYSIZE(stage##ConstantBuffers)];                                                      \
        for (uint32_t i = 0; i < ARRAYSIZE(buffers); i++) {                                                            \
            buffers[i] = get(stage##ConstantBuffers[i]);                                                               \
        }                                                                                                              \
        context->stage##SetConstantBuffers(0, ARRAYSIZE(buffers), buffers);                                            \
    }                                                                                                                  \
    {                                                                                                                  \
        ID3D11SamplerState* samp[ARRAYSIZE(stage##Samplers)];                                                          \
        for (uint32_t i = 0; i < ARRAYSIZE(samp); i++) {                                                               \
            samp[i] = get(stage##Samplers[i]);                                                                         \
        }                                                                                                              \
        context->stage##SetSamplers(0, ARRAYSIZE(samp), samp);                                                         \
    }                                                                                                                  \
    {                                                                                                                  \
        ID3D11ShaderResourceView* srvs[ARRAYSIZE(stage##ShaderResources)];                                             \
        for (uint32_t i = 0; i < ARRAYSIZE(srvs); i++) {                                                               \
            srvs[i] = get(stage##ShaderResources[i]);                                                                  \
        }                                                                                                              \
        context->stage##SetShaderResources(0, ARRAYSIZE(srvs), srvs);                                                  \
    }

            SHADER_STAGE_RESTORE_CONTEXT(VS);
            SHADER_STAGE_RESTORE_CONTEXT(PS);
            SHADER_STAGE_RESTORE_CONTEXT(GS);
            SHADER_STAGE_RESTORE_CONTEXT(DS);
            SHADER_STAGE_RESTORE_CONTEXT(HS);
            SHADER_STAGE_RESTORE_CONTEXT(CS);

#undef SHADER_STAGE_RESTORE_CONTEXT

            {
                ID3D11UnorderedAccessView* uavs[ARRAYSIZE(CSUnorderedResources)];
                for (uint32_t i = 0; i < ARRAYSIZE(uavs); i++) {
                    uavs[i] = get(CSUnorderedResources[i]);
                }
                context->CSGetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs);
            }

            context->RSSetState(get(rasterizerState));
            context->RSSetViewports(numViewports, viewports);
            context->RSSetScissorRects(numScissorRects, scissorRects);

            TraceLoggingWriteStop(local, "D3D11ContextState_Restore");
        }

        void clear() {
#define RESET_ARRAY(array)                                                                                             \
    for (uint32_t i = 0; i < ARRAYSIZE(array); i++) {                                                                  \
        array[i].Reset();                                                                                              \
    }

            inputLayout.Reset();
            RESET_ARRAY(vertexBuffers);
            indexBuffer.Reset();

            RESET_ARRAY(renderTargets);
            depthStencil.Reset();
            depthStencilState.Reset();
            blendState.Reset();

#define SHADER_STAGE_STATE(stage)                                                                                      \
    stage##Program.Reset();                                                                                            \
    RESET_ARRAY(stage##ConstantBuffers);                                                                               \
    RESET_ARRAY(stage##Samplers);                                                                                      \
    RESET_ARRAY(stage##ShaderResources);

            SHADER_STAGE_STATE(VS);
            SHADER_STAGE_STATE(PS);
            SHADER_STAGE_STATE(GS);
            SHADER_STAGE_STATE(DS);
            SHADER_STAGE_STATE(HS);
            SHADER_STAGE_STATE(CS);

            RESET_ARRAY(CSUnorderedResources);

            rasterizerState.Reset();

#undef RESET_ARRAY

            m_isValid = false;
        }

        bool isValid() const {
            return m_isValid;
        }

      private:
        bool m_isValid{false};
    };

    // Wrap a render target view. Obtained from D3D11Texture.
    class D3D11RenderTargetView : public IRenderTargetView {
      public:
        D3D11RenderTargetView(std::shared_ptr<IDevice> device, ID3D11RenderTargetView* renderTargetView)
            : m_device(device), m_renderTargetView(renderTargetView) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return get(m_renderTargetView);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    };

    // Wrap a depth/stencil buffer view. Obtained from D3D11Texture.
    class D3D11DepthStencilView : public IDepthStencilView {
      public:
        D3D11DepthStencilView(std::shared_ptr<IDevice> device, ID3D11DepthStencilView* depthStencilView)
            : m_device(device), m_depthStencilView(depthStencilView) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return get(m_depthStencilView);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    };

    // Wrap a texture resource. Obtained from D3D11Device.
    class D3D11Texture : public ITexture {
      public:
        D3D11Texture(std::shared_ptr<IDevice> device,
                     const XrSwapchainCreateInfo& info,
                     const D3D11_TEXTURE2D_DESC& textureDesc,
                     ID3D11Texture2D* texture)
            : m_device(device), m_info(info), m_textureDesc(textureDesc), m_texture(texture) {
            m_renderTargetSubView.resize(info.arraySize);
            m_depthStencilSubView.resize(info.arraySize);
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        const XrSwapchainCreateInfo& getInfo() const override {
            return m_info;
        }

        bool isArray() const override {
            return m_textureDesc.ArraySize > 1;
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

            m_device->getContextAs<D3D11>()->UpdateSubresource(
                get(m_texture),
                D3D11CalcSubresource(0, std::max(0, slice), m_textureDesc.MipLevels),
                nullptr,
                buffer,
                rowPitch,
                0);
        }

         void* getNativePtr() const override {
            return get(m_texture);
        }

      private:
        std::shared_ptr<D3D11RenderTargetView> makeRenderTargetViewInternal(uint32_t slice) const {
            if (!(m_textureDesc.BindFlags & D3D11_BIND_RENDER_TARGET)) {
                throw std::runtime_error("Texture was not created with D3D11_BIND_RENDER_TARGET");
            }
            if (auto device = m_device->getAs<D3D11>()) {
                D3D11_RENDER_TARGET_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = (DXGI_FORMAT)m_info.format;
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipSlice = D3D11CalcSubresource(0, 0, m_info.mipCount);

                ComPtr<ID3D11RenderTargetView> rtv;
                CHECK_HRCMD(device->CreateRenderTargetView(get(m_texture), &desc, set(rtv)));
                return std::make_shared<D3D11RenderTargetView>(m_device, get(rtv));
            }
            return nullptr;
        }

        std::shared_ptr<D3D11DepthStencilView> makeDepthStencilViewInternal(uint32_t slice) const {
            if (!(m_textureDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
                throw std::runtime_error("Texture was not created with D3D11_BIND_DEPTH_STENCIL");
            }
            if (auto device = m_device->getAs<D3D11>()) {
                D3D11_DEPTH_STENCIL_VIEW_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Format = m_textureDesc.Format; 
                desc.ViewDimension =
                    m_info.arraySize == 1 ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = slice;
                desc.Texture2DArray.MipSlice = D3D11CalcSubresource(0, 0, m_info.mipCount);

                ComPtr<ID3D11DepthStencilView> rtv;
                CHECK_HRCMD(device->CreateDepthStencilView(get(m_texture), &desc, set(rtv)));
                return std::make_shared<D3D11DepthStencilView>(m_device, get(rtv));
            }
            return nullptr;
        }

        const std::shared_ptr<IDevice> m_device;
        const XrSwapchainCreateInfo m_info;
        const D3D11_TEXTURE2D_DESC m_textureDesc;
        const ComPtr<ID3D11Texture2D> m_texture;

        mutable std::shared_ptr<D3D11RenderTargetView> m_renderTargetView;
        mutable std::vector<std::shared_ptr<D3D11RenderTargetView>> m_renderTargetSubView;
        mutable std::shared_ptr<D3D11DepthStencilView> m_depthStencilView;
        mutable std::vector<std::shared_ptr<D3D11DepthStencilView>> m_depthStencilSubView;
    };

    // Wrap a constant buffer. Obtained from D3D11Device.
    class D3D11Buffer : public IShaderBuffer {
      public:
        D3D11Buffer(std::shared_ptr<IDevice> device, D3D11_BUFFER_DESC bufferDesc, ID3D11Buffer* buffer)
            : m_device(device), m_bufferDesc(bufferDesc), m_buffer(buffer) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void uploadData(const void* buffer, size_t count) override {
            if (m_bufferDesc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) {
                if (auto context = m_device->getContextAs<D3D11>()) {
                    D3D11_MAPPED_SUBRESOURCE mappedResources;
                    CHECK_HRCMD(context->Map(get(m_buffer), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResources));
                    memcpy(mappedResources.pData, buffer, std::min(count, size_t(m_bufferDesc.ByteWidth)));
                    context->Unmap(get(m_buffer), 0);
                }
            } else {
                throw std::runtime_error("Texture is immutable");
            }
        }

        void* getNativePtr() const override {
            return get(m_buffer);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11Buffer> m_buffer;
        const D3D11_BUFFER_DESC m_bufferDesc;
    };

    // Wrap a vertex+indices buffers. Obtained from D3D11Device.
    class D3D11SimpleMesh : public ISimpleMesh {
      public:
        D3D11SimpleMesh(std::shared_ptr<IDevice> device,
                        ID3D11Buffer* vertexBuffer,
                        size_t stride,
                        ID3D11Buffer* indexBuffer,
                        size_t numIndices)
            : m_device(device), m_vertexBuffer(vertexBuffer), m_indexBuffer(indexBuffer) {
            m_meshData.vertexBuffer = get(m_vertexBuffer);
            m_meshData.stride = (UINT)stride;
            m_meshData.indexBuffer = get(m_indexBuffer);
            m_meshData.numIndices = (UINT)numIndices;
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return reinterpret_cast<void*>(&m_meshData);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11Buffer> m_vertexBuffer;
        const ComPtr<ID3D11Buffer> m_indexBuffer;

        mutable struct D3D11::MeshData m_meshData;
    };

    // Wrap a device context.
    class D3D11Context : public graphics::IContext {
      public:
        D3D11Context(std::shared_ptr<IDevice> device, ID3D11DeviceContext* context)
            : m_device(device), m_context(context) {
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        std::shared_ptr<IDevice> getDevice() const override {
            return m_device;
        }

        void* getNativePtr() const override {
            return get(m_context);
        }

      private:
        const std::shared_ptr<IDevice> m_device;
        const ComPtr<ID3D11DeviceContext> m_context;
    };

    class D3D11Device : public IDevice, public std::enable_shared_from_this<D3D11Device>
    {
      public:
        D3D11Device(ID3D11Device* device)
            : m_device(device)
        {
            m_device->GetImmediateContext(set(m_context));
            {
                ComPtr<IDXGIDevice> dxgiDevice;
                ComPtr<IDXGIAdapter> adapter;
                DXGI_ADAPTER_DESC desc;

                CHECK_HRCMD(m_device->QueryInterface(set(dxgiDevice)));
                CHECK_HRCMD(dxgiDevice->GetAdapter(set(adapter)));
                CHECK_HRCMD(adapter->GetDesc(&desc));

                const std::wstring wadapterDescription(desc.Description);
                std::string deviceName;
                std::transform(wadapterDescription.begin(),
                               wadapterDescription.end(),
                               std::back_inserter(deviceName),
                               [](wchar_t c) { return (char)c; });

                // Log the adapter name to help debugging customer issues.
                Log("Using Direct3D 11 on adapter: %s\n", deviceName.c_str());
            }

            // Create common resources.
            initializeMeshResources();
        }

        ~D3D11Device() override {
            Log("D3D11Device destroyed\n");
        }

        void shutdown() override {
            // Clear all references that could hold a cyclic reference themselves.
            m_currentDrawRenderTarget.reset();
            m_currentDrawDepthBuffer.reset();
            m_currentMesh.reset();

            m_meshModelBuffer.reset();
            m_meshViewProjectionBuffer.reset();
        }

        Api getApi() const override {
            return Api::D3D11;
        }

        void saveContext(bool clear) override {
            // Ensure we are not dropping an unfinished context.
            assert(!m_state.isValid());

            m_state.save(get(m_context));
            if (clear) {
                m_context->ClearState();
            }
        }

        void restoreContext() override {
            // Ensure saveContext() was called.
            assert(m_state.isValid());

            m_state.restore(get(m_context));
            m_state.clear();
        }

        void flushContext(bool blocking, bool isEndOfFrame = false) override {
            // Ensure we are not dropping an unfinished context.
            assert(!m_state.isValid());

            if (!blocking)
            {
                m_context->Flush();
            }
            else
            {
                ComPtr<ID3D11DeviceContext4> Context1;
                CHECK_HRCMD(m_context->QueryInterface(IID_PPV_ARGS(Context1.ReleaseAndGetAddressOf())));

                wil::unique_handle eventHandle;
                *eventHandle.put() = CreateEventEx(nullptr, "flushContext d3d11", 0, EVENT_ALL_ACCESS);
                Context1->Flush1(D3D11_CONTEXT_TYPE_ALL, eventHandle.get());
                WaitForSingleObject(eventHandle.get(), INFINITE);
            }
        }

        std::shared_ptr<ITexture> createTexture(const XrSwapchainCreateInfo& info,
                                                std::string_view debugName,
                                                int64_t overrideFormat = 0,
                                                uint32_t rowPitch = 0,
                                                uint32_t imageSize = 0,
                                                const void* initialData = nullptr) override {
            assert(!(rowPitch % getTextureAlignmentConstraint()));

            D3D11_TEXTURE2D_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Format = (DXGI_FORMAT)(!overrideFormat ? info.format : overrideFormat);
            desc.Width = info.width;
            desc.Height = info.height;
            desc.ArraySize = info.arraySize;
            desc.MipLevels = info.mipCount;
            desc.SampleDesc.Count = info.sampleCount;
            desc.Usage = D3D11_USAGE_DEFAULT;
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) {
                desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT) {
                desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            }
            if (info.usageFlags & XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT) {
                desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            }

            ComPtr<ID3D11Texture2D> texture;
            if (initialData) {
                D3D11_SUBRESOURCE_DATA data;
                ZeroMemory(&data, sizeof(data));
                data.pSysMem = initialData;
                data.SysMemPitch = static_cast<uint32_t>(rowPitch);
                data.SysMemSlicePitch = static_cast<uint32_t>(imageSize);

                CHECK_HRCMD(m_device->CreateTexture2D(&desc, &data, set(texture)));
            } else {
                CHECK_HRCMD(m_device->CreateTexture2D(&desc, nullptr, set(texture)));
            }

            SetDebugName(get(texture), debugName);

            return std::make_shared<D3D11Texture>(shared_from_this(), info, desc, get(texture));
        }

        std::shared_ptr<IShaderBuffer>
        createBuffer(size_t size, std::string_view debugName, const void* initialData, bool immutable) override {
            auto desc = CD3D11_BUFFER_DESC(static_cast<UINT>(size),
                                           D3D11_BIND_CONSTANT_BUFFER,
                                           (initialData && immutable) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC,
                                           immutable ? 0 : D3D11_CPU_ACCESS_WRITE);
            ComPtr<ID3D11Buffer> buffer;
            if (initialData) {
                D3D11_SUBRESOURCE_DATA data;
                ZeroMemory(&data, sizeof(data));
                data.pSysMem = initialData;

                CHECK_HRCMD(m_device->CreateBuffer(&desc, &data, set(buffer)));
            } else {
                CHECK_HRCMD(m_device->CreateBuffer(&desc, nullptr, set(buffer)));
            }

            SetDebugName(get(buffer), debugName);

            return std::make_shared<D3D11Buffer>(shared_from_this(), desc, get(buffer));
        }

        std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                      std::vector<uint16_t>& indices,
                                                      std::string_view debugName) override {
            D3D11_BUFFER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Usage = D3D11_USAGE_IMMUTABLE;

            D3D11_SUBRESOURCE_DATA data;
            ZeroMemory(&data, sizeof(data));

            desc.ByteWidth = (UINT)vertices.size() * sizeof(SimpleMeshVertex);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            data.pSysMem = vertices.data();
            ComPtr<ID3D11Buffer> vertexBuffer;
            CHECK_HRCMD(m_device->CreateBuffer(&desc, &data, set(vertexBuffer)));

            desc.ByteWidth = (UINT)indices.size() * sizeof(uint16_t);
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            data.pSysMem = indices.data();
            ComPtr<ID3D11Buffer> indexBuffer;
            CHECK_HRCMD(m_device->CreateBuffer(&desc, &data, set(indexBuffer)));

            if (!debugName.empty()) {
                SetDebugName(get(vertexBuffer), debugName);
                SetDebugName(get(indexBuffer), debugName);
            }

            return std::make_shared<D3D11SimpleMesh>(
                shared_from_this(), get(vertexBuffer), sizeof(SimpleMeshVertex), get(indexBuffer), indices.size());
        }

        void unsetRenderTargets() override {
            auto renderTargetViews = reinterpret_cast<ID3D11RenderTargetView* const*>(kClearResources);
            m_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargetViews, nullptr);
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

            ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {nullptr};

            if (numRenderTargets > size_t(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT))
                numRenderTargets = size_t(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);

            for (size_t i = 0; i < numRenderTargets; i++) {
                auto slice = renderSlices ? renderSlices[i] : -1;
                rtvs[i] = renderTargets[i]->getRenderTargetView(slice)->getAs<D3D11>();
            }

            auto pDepthStencilView =
                depthBuffer ? depthBuffer->getDepthStencilView(depthSlice)->getAs<D3D11>() : nullptr;

            m_context->OMSetRenderTargets(static_cast<UINT>(numRenderTargets), rtvs, pDepthStencilView);

            if (numRenderTargets) {
                m_currentDrawRenderTarget = renderTargets[0];
                m_currentDrawRenderTargetSlice = renderSlices ? renderSlices[0] : -1;
                m_currentDrawDepthBuffer = std::move(depthBuffer);
                m_currentDrawDepthBufferSlice = depthSlice;

                D3D11_VIEWPORT viewport;
                ZeroMemory(&viewport, sizeof(viewport));
                if (viewport0)
                {
                    viewport.TopLeftX = (float)viewport0->offset.x;
                    viewport.TopLeftY = (float)viewport0->offset.y;
                    viewport.Width = (float)viewport0->extent.width;
                    viewport.Height = (float)viewport0->extent.height;
                }
                else
                {
                    viewport.TopLeftX = 0.0f;
                    viewport.TopLeftY = 0.0f;
                    viewport.Width = (float)m_currentDrawRenderTarget->getInfo().width;
                    viewport.Height = (float)m_currentDrawRenderTarget->getInfo().height;
                }
                viewport.MaxDepth = 1.0f;
                m_context->RSSetViewports(1, &viewport);
            } else {
                m_currentDrawRenderTarget.reset();
                m_currentDrawDepthBuffer.reset();
            }
            m_currentMesh.reset();
        }

        void clearDepth(float value) override {
            if (m_currentDrawDepthBuffer) {
                auto depthStencilView =
                    m_currentDrawDepthBuffer->getDepthStencilView(m_currentDrawDepthBufferSlice)->getAs<D3D11>();

                m_context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, value, 0);
            }
        }

        void setViewProjection(const xr::math::ViewProjection& view) override {
            ViewProjectionConstantBuffer staging;

            // viewMatrix* projMatrix
            DirectX::XMStoreFloat4x4(
                &staging.ViewProjection,
                DirectX::XMMatrixTranspose(xr::math::LoadInvertedXrPose(view.Pose) *
                                           xr::math::ComposeProjectionMatrix(view.Fov, view.NearFar)));
            if (!m_meshViewProjectionBuffer) {
                m_meshViewProjectionBuffer =
                    createBuffer(sizeof(ViewProjectionConstantBuffer), "ViewProjection CB", nullptr, false);
            }
            m_meshViewProjectionBuffer->uploadData(&staging, sizeof(staging));

            m_context->OMSetDepthStencilState(get(m_DepthNoStencilTest), 0);
        }

        void draw(std::shared_ptr<ISimpleMesh> mesh, const XrPosef& pose, XrVector3f scaling) override {
            if (auto meshData = mesh->getAs<D3D11>()) {
                if (mesh != m_currentMesh) {
                    if (!m_meshModelBuffer) {
                        m_meshModelBuffer = createBuffer(sizeof(ModelConstantBuffer), "Model CB", nullptr, false);
                    }
                    ID3D11Buffer* const constantBuffers[] = {m_meshModelBuffer->getAs<D3D11>(),
                                                             m_meshViewProjectionBuffer->getAs<D3D11>()};
                    m_context->VSSetConstantBuffers(0, ARRAYSIZE(constantBuffers), constantBuffers);
                    m_context->VSSetShader(get(m_meshVertexShader), nullptr, 0);
                    m_context->PSSetShader(get(m_meshPixelShader), nullptr, 0);
                    m_context->GSSetShader(nullptr, nullptr, 0);

                    const UINT strides[] = {meshData->stride};
                    const UINT offsets[] = {0};
                    ID3D11Buffer* const vertexBuffers[] = {meshData->vertexBuffer};
                    m_context->IASetVertexBuffers(0, ARRAYSIZE(vertexBuffers), vertexBuffers, strides, offsets);
                    m_context->IASetIndexBuffer(meshData->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
                    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    m_context->IASetInputLayout(get(m_meshInputLayout));

                    m_currentMesh = mesh;
                }

                ModelConstantBuffer model;
                const DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(scaling.x, scaling.y, scaling.z);
                DirectX::XMStoreFloat4x4(&model.Model,
                                         DirectX::XMMatrixTranspose(scaleMatrix * xr::math::LoadXrPose(pose)));
                m_meshModelBuffer->uploadData(&model, sizeof(model));

                m_context->DrawIndexedInstanced(meshData->numIndices, 1, 0, 0, 0);
            }
        }

        uint32_t getTextureAlignmentConstraint() const override {
            return 16;
        }

        void* getNativePtr() const override {
            return get(m_device);
        }

        void* getContextPtr() const override {
            return get(m_context);
        }

      private:
        // Initialize the resources needed for draw() and related calls.
        void initializeMeshResources() {
            {
                ComPtr<ID3DBlob> vsBytes;
                graphics::shader::CompileShader(MeshShaders, "vsMain", set(vsBytes), "vs_5_0");

                CHECK_HRCMD(m_device->CreateVertexShader(
                    vsBytes->GetBufferPointer(), vsBytes->GetBufferSize(), nullptr, set(m_meshVertexShader)));

                SetDebugName(get(m_meshVertexShader), "SimpleMesh VS");

                const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = {
                    {"POSITION",
                     0,
                     DXGI_FORMAT_R32G32B32_FLOAT,
                     0,
                     D3D11_APPEND_ALIGNED_ELEMENT,
                     D3D11_INPUT_PER_VERTEX_DATA,
                     0},
                    {"COLOR",
                     0,
                     DXGI_FORMAT_R32G32B32_FLOAT,
                     0,
                     D3D11_APPEND_ALIGNED_ELEMENT,
                     D3D11_INPUT_PER_VERTEX_DATA,
                     0},
                };

                CHECK_HRCMD(m_device->CreateInputLayout(vertexDesc,
                                                        ARRAYSIZE(vertexDesc),
                                                        vsBytes->GetBufferPointer(),
                                                        vsBytes->GetBufferSize(),
                                                        set(m_meshInputLayout)));
            }
            {
                ComPtr<ID3DBlob> errors;
                ComPtr<ID3DBlob> psBytes;
                HRESULT hr = D3DCompile(MeshShaders.data(),
                                        MeshShaders.length(),
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        "psMain",
                                        "ps_5_0",
                                        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS,
                                        0,
                                        set(psBytes),
                                        set(errors));
                if (FAILED(hr)) {
                    if (errors) {
                        Log("%s\n", (char*)errors->GetBufferPointer());
                    }
                    CHECK_HRESULT(hr, "Failed to compile shader");
                }
                CHECK_HRCMD(m_device->CreatePixelShader(
                    psBytes->GetBufferPointer(), psBytes->GetBufferSize(), nullptr, set(m_meshPixelShader)));

                SetDebugName(get(m_meshPixelShader), "SimpleMesh PS");
            }
            {
                D3D11_DEPTH_STENCIL_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.DepthEnable = true;
                desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
                desc.DepthFunc = D3D11_COMPARISON_LESS;
                CHECK_HRCMD(m_device->CreateDepthStencilState(&desc, set(m_DepthNoStencilTest)));
            }
        }

        const ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;
        D3D11ContextState m_state;

        ComPtr<ID3D11DepthStencilState> m_DepthNoStencilTest;
        ComPtr<ID3D11VertexShader> m_meshVertexShader;
        ComPtr<ID3D11PixelShader> m_meshPixelShader;
        ComPtr<ID3D11InputLayout> m_meshInputLayout;
        std::shared_ptr<IShaderBuffer> m_meshViewProjectionBuffer;
        std::shared_ptr<IShaderBuffer> m_meshModelBuffer;

        std::shared_ptr<ITexture> m_currentDrawRenderTarget;
        int32_t m_currentDrawRenderTargetSlice;
        std::shared_ptr<ITexture> m_currentDrawDepthBuffer;
        int32_t m_currentDrawDepthBufferSlice;
        std::shared_ptr<ISimpleMesh> m_currentMesh;

        static XrSwapchainCreateInfo getTextureInfo(const D3D11_TEXTURE2D_DESC& textureDesc) {
            XrSwapchainCreateInfo info;
            ZeroMemory(&info, sizeof(info));
            info.format = (int64_t)textureDesc.Format;
            info.width = textureDesc.Width;
            info.height = textureDesc.Height;
            info.arraySize = textureDesc.ArraySize;
            info.mipCount = textureDesc.MipLevels;
            info.sampleCount = textureDesc.SampleDesc.Count;
            if (textureDesc.BindFlags & D3D11_BIND_RENDER_TARGET) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            }
            if (textureDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            if (textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            }
            if (textureDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
                info.usageFlags |= XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT;
            }

            return info;
        }

        static void LogInfoQueueMessage(ID3D11InfoQueue* infoQueue, UINT64 messageCount) {
            assert(infoQueue);
            for (UINT64 i = 0u; i < messageCount; i++) {
                SIZE_T size = 0;
                infoQueue->GetMessage(i, nullptr, &size);
                if (size) {
                    auto message_data = std::make_unique<char[]>(size);
                    auto message = reinterpret_cast<D3D11_MESSAGE*>(message_data.get());
                    CHECK_HRCMD(infoQueue->GetMessage(i, message, &size));
                    Log("D3D11: %.*s\n", message->DescriptionByteLength, message->pDescription);
                }
            }
        }

        // NB: Maximum resources possible are:
        // - D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT (128)
        // - D3D11_1_UAV_SLOT_COUNT (64)
        // - D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT (8)
        static inline void* const kClearResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {nullptr};
    };
} // namespace

namespace graphics
{
    std::shared_ptr<IDevice> WrapD3D11Device(ID3D11Device* device)
    {
        return std::make_shared<D3D11Device>(device);
    }

    std::shared_ptr<ITexture> WrapD3D11Texture(std::shared_ptr<IDevice> device,
                                               const XrSwapchainCreateInfo& info,
                                               ID3D11Texture2D* texture,
                                               std::string_view debugName)
    {
        if (device->getApi() == Api::D3D11)
        {
            SetDebugName(texture, debugName);

            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);
            return std::make_shared<D3D11Texture>(device, info, desc, texture);
        }
        throw std::runtime_error("Not a D3D11 device");
    }

} // namespace graphics
