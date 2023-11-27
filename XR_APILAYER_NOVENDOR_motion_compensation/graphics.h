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

#include "log.h"
#include "utility.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

namespace openxr_api_layer::graphics {

    namespace d3dcommon {
        struct ModelConstantBuffer {
            DirectX::XMFLOAT4X4 Model;
        };

        struct ViewProjectionConstantBuffer {
            DirectX::XMFLOAT4X4 ViewProjection;
        };

        const std::string_view MeshShaders = R"_(
struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Color : COLOR0;
};
struct VSInput {
    float3 Pos : POSITION;
    float3 Color : COLOR0;
};
cbuffer ModelConstantBuffer : register(b0) {
    float4x4 Model;
};
cbuffer ViewProjectionConstantBuffer : register(b1) {
    float4x4 ViewProjection;
};

VSOutput vsMain(VSInput input) {
    VSOutput output;
    output.Pos = mul(mul(float4(input.Pos, 1), Model), ViewProjection);
    output.Color = input.Color;
    return output;
}

float4 psMain(VSOutput input) : SV_TARGET {
    return float4(input.Color, 1);
}
)_";
    } // namespace d3dcommon

    // Quick and dirty API helper
    template <class T, class... Types>
    inline constexpr bool is_any_of_v = std::disjunction_v<std::is_same<T, Types>...>;

    enum class Api {
#ifdef XR_USE_GRAPHICS_API_D3D11
        D3D11,
#endif
#ifdef XR_USE_GRAPHICS_API_D3D12
        D3D12,
#endif
    };
    enum class CompositionApi {
#ifdef XR_USE_GRAPHICS_API_D3D11
        D3D11,
#endif
    };

    // Type traits for all graphics APIs.

#ifdef XR_USE_GRAPHICS_API_D3D11
    struct D3D11 {
        static constexpr Api Api = Api::D3D11;

        using Device = ID3D11Device*;
        using Context = ID3D11DeviceContext*;
        using Texture = ID3D11Texture2D*;
        using Fence = ID3D11Fence*;
        using Buffer = ID3D11Buffer*;
        struct MeshData
        {
            ID3D11Buffer* vertexBuffer;
            ID3D11Buffer* indexBuffer;
            UINT stride;
            UINT numIndices;
        };
        using Mesh = MeshData*;
        using PixelShader = ID3D11PixelShader*;       
        using RenderTargetView = ID3D11RenderTargetView*;
        using DepthStencilView = ID3D11DepthStencilView*;

        // clang-format off
            template <typename T>
            static constexpr bool is_concrete_api_v = is_any_of_v<T,
              Device, Context, Texture, Buffer, Mesh, PixelShader, RenderTargetView, DepthStencilView>;
        // clang-format on
    };
#endif

#ifdef XR_USE_GRAPHICS_API_D3D12
    struct D3D12 {
        static constexpr Api Api = Api::D3D12;

        using Device = ID3D12Device*;
        using Context = ID3D12CommandQueue*;
        using Texture = ID3D12Resource*;
        using Fence = ID3D12Fence*;
        using Buffer = ID3D12Resource*;
        struct MeshData
        {
            D3D12_VERTEX_BUFFER_VIEW* vertexBuffer;
            D3D12_INDEX_BUFFER_VIEW* indexBuffer;
            UINT numIndices;
        };
        using Mesh = MeshData*;
        struct ShaderData {
            ID3D12RootSignature* rootSignature;
            ID3D12PipelineState* pipelineState;
        };
        using PixelShader = ShaderData*;
        using RenderTargetView = D3D12_CPU_DESCRIPTOR_HANDLE*;
        using DepthStencilView = D3D12_CPU_DESCRIPTOR_HANDLE*;

        // clang-format off
            template <typename T>
            static constexpr bool is_concrete_api_v = is_any_of_v<T,
              Device, Context, Texture, Buffer, Mesh, PixelShader, RenderTargetView, DepthStencilView>;
        // clang-format on
    };
#endif

    // Graphics API helper
    template <typename ConcreteType, typename InterfaceType>
    inline auto GetAs(const InterfaceType* pInterface) {
#ifndef XR_USE_GRAPHICS_API_D3D12
        constexpr auto api = Api::D3D11;
#else 
        constexpr auto api = D3D12::is_concrete_api_v<ConcreteType> ? Api::D3D12 : Api::D3D11;
#endif 
        return reinterpret_cast<ConcreteType>(api == pInterface->getApi() ? pInterface->getNativePtr() : nullptr);
    }

    // We (arbitrarily) use DXGI as a common conversion point for all graphics APIs.
    using GenericFormat = DXGI_FORMAT;

    struct ShareableHandle {
        wil::unique_handle ntHandle;
        HANDLE handle{nullptr};
        bool isNtHandle{};
        Api origin{};
    };

    // A fence.
    struct IGraphicsFence {
        virtual ~IGraphicsFence() = default;

        virtual Api getApi() const = 0;
        virtual void* getNativeFencePtr() const = 0;
        virtual ShareableHandle getFenceHandle() const = 0;

        virtual void signal(uint64_t value) = 0;
        virtual void waitOnDevice(uint64_t value) = 0;
        virtual void waitOnCpu(uint64_t value) = 0;

        virtual bool isShareable() const = 0;

        template <typename ApiTraits>
        typename ApiTraits::Fence getNativeFence() const {
            if (ApiTraits::Api != getApi()) {
                throw std::runtime_error("Api mismatch");
            }
            return reinterpret_cast<typename ApiTraits::Fence>(getNativeFencePtr());
        }
    };

    // A texture.
    struct IGraphicsTexture {
        virtual ~IGraphicsTexture() = default;

        virtual Api getApi() const = 0;
        virtual void* getNativeTexturePtr() const = 0;
        virtual ShareableHandle getTextureHandle() const = 0;

        virtual const XrSwapchainCreateInfo& getInfo() const = 0;
        virtual bool isShareable() const = 0;

        template <typename ApiTraits>
        typename ApiTraits::Texture getNativeTexture() const {
            if (ApiTraits::Api != getApi()) {
                throw std::runtime_error("Api mismatch");
            }
            return reinterpret_cast<typename ApiTraits::Texture>(getNativeTexturePtr());
        }
    };

    // A buffer to be used with shaders.
    struct IShaderBuffer {
        virtual ~IShaderBuffer() = default;

        virtual Api getApi() const = 0;

        virtual void uploadData(const void* buffer, size_t count) = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const {
            return GetAs<typename ApiTraits::Buffer>(this);
        }
    };

    struct SimpleMeshVertex {
        XrVector3f Position;
        XrVector3f Color;
    };

    struct IGraphicsDevice;


    // A simple (unskinned) mesh.
    struct ISimpleMesh {
        virtual ~ISimpleMesh() = default;

        virtual Api getApi() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const {
            return GetAs<typename ApiTraits::Mesh>(this);
        }
    };

    // A graphics device and execution context.
    struct IGraphicsDevice {
        virtual ~IGraphicsDevice() = default;

        virtual Api getApi() const = 0;
        virtual void* getNativeDevicePtr() const = 0;
        virtual void* getNativeContextPtr() const = 0;

        virtual std::shared_ptr<IGraphicsFence> createFence(bool shareable = true) = 0;
        virtual std::shared_ptr<IGraphicsFence> openFence(const ShareableHandle& handle) = 0;
        virtual std::shared_ptr<IGraphicsTexture> createTexture(const XrSwapchainCreateInfo& info,
                                                                bool shareable = true) = 0;
        virtual std::shared_ptr<IGraphicsTexture> openTexture(const ShareableHandle& handle,
                                                              const XrSwapchainCreateInfo& info) = 0;
        virtual std::shared_ptr<IGraphicsTexture> openTexturePtr(void* nativeTexturePtr,
                                                                 const XrSwapchainCreateInfo& info) = 0;

        virtual void copyTexture(IGraphicsTexture* from, IGraphicsTexture* to) = 0;

        virtual std::shared_ptr<IShaderBuffer> createBuffer(size_t size,
                                                            std::string_view debugName,
                                                            const void* initialData = nullptr,
                                                            bool immutable = false) = 0;

        virtual std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                              std::vector<uint16_t>& indices,
                                                              std::string_view debugName) = 0;

        virtual void setViewProjection(const xr::math::ViewProjection& view) = 0;
        virtual void draw(std::shared_ptr<ISimpleMesh> mesh,
                          const XrPosef& pose,
                          XrVector3f scaling = {1.0f, 1.0f, 1.0f}) = 0;
        virtual void UnsetDrawResources() const = 0;

        virtual GenericFormat translateToGenericFormat(int64_t format) const = 0;
        virtual int64_t translateFromGenericFormat(GenericFormat format) const = 0;

        virtual LUID getAdapterLuid() const = 0;

        template <typename ApiTraits>
        typename ApiTraits::Device getNativeDevice() const {
            if (ApiTraits::Api != getApi()) {
                throw std::runtime_error("Api mismatch");
            }
            return reinterpret_cast<typename ApiTraits::Device>(getNativeDevicePtr());
        }

        template <typename ApiTraits>
        typename ApiTraits::Context getNativeContext() const {
            if (ApiTraits::Api != getApi()) {
                throw std::runtime_error("Api mismatch");
            }
            return reinterpret_cast<typename ApiTraits::Context>(getNativeContextPtr());
        }

        template <typename ApiTraits>
        std::shared_ptr<IGraphicsTexture> openTexture(typename ApiTraits::Texture nativeTexture,
                                                      const XrSwapchainCreateInfo& info) {
            if (ApiTraits::Api != getApi()) {
                throw std::runtime_error("Api mismatch");
            }
            return openTexturePtr(reinterpret_cast<void*>(nativeTexture), info);
        }
    };

    // Modes of use of wrapped swapchains.
    enum class SwapchainMode {
        // The swapchain must be submittable to the upstream xrEndFrame() implementation.
        // A non-submittable swapchain does not have an XrSwapchain handle.
        Submit = (1 << 0),

        // The swapchain will be accessed for reading during composition in the layer's xrEndFrame() implementation.
        // A readable swapchain might require a copy to the composition device before composition.
        Read = (1 << 1),

        // The swapchain will be access for writing during composition in the layer's xrEndFrame() implementation.
        // A writable swapchain might require a copy from the composition device after composition.
        Write = (1 << 2),
    };
    DEFINE_ENUM_FLAG_OPERATORS(SwapchainMode);

    struct ISwapchainImage;

    // A swapchain.
    struct ISwapchain {
        virtual ~ISwapchain() = default;

        // Only for manipulating swapchains created through createSwapchain().
        virtual ISwapchainImage* acquireImage(bool wait = true) = 0;
        virtual void waitImage() = 0;
        virtual void releaseImage() = 0;

        virtual ISwapchainImage* getLastReleasedImage() const = 0;
        virtual void commitLastReleasedImage() = 0;

        virtual const XrSwapchainCreateInfo& getInfoOnCompositionDevice() const = 0;
        virtual int64_t getFormatOnApplicationDevice() const = 0;
        virtual ISwapchainImage* getImage(uint32_t index) const = 0;
        virtual uint32_t getLength() const = 0;

        // Can only be called if the swapchain is submittable.
        virtual XrSwapchain getSwapchainHandle() const = 0;
        virtual XrSwapchainSubImage getSubImage() const = 0;
    };

    // A swapchain image.
    struct ISwapchainImage {
        virtual ~ISwapchainImage() = default;

        virtual IGraphicsTexture* getApplicationTexture() const = 0;
        virtual IGraphicsTexture* getTextureForRead() const = 0;
        virtual IGraphicsTexture* getTextureForWrite() const = 0;

        virtual uint32_t getIndex() const = 0;
    };

    // A container for user session data.
    // This class is meant to be extended by a caller before use with ICompositionFramework::setSessionData() and
    // ICompositionFramework::getSessionData().
    struct ICompositionSessionData {
        virtual ~ICompositionSessionData() = default;
    };

    // A collection of hooks and utilities to perform composition in the layer.
    struct ICompositionFramework {
        virtual ~ICompositionFramework() = default;

        virtual XrSession getSessionHandle() const = 0;

        virtual void setSessionData(std::unique_ptr<ICompositionSessionData> sessionData) = 0;
        virtual ICompositionSessionData* getSessionDataPtr() const = 0;

        // Create a swapchain without an XrSwapchain handle.
        virtual std::shared_ptr<ISwapchain> createSwapchain(const XrSwapchainCreateInfo& infoOnApplicationDevice,
                                                            SwapchainMode mode) = 0;

        // Must be called at the beginning of the layer's xrEndFrame() implementation to serialize application commands
        // prior to composition.
        virtual void serializePreComposition() = 0;

        // Must be called before chaining to the upstream xrEndFrame() implementation to serialize composition commands
        // prior to submission.
        virtual void serializePostComposition() = 0;

        virtual IGraphicsDevice* getCompositionDevice() const = 0;
        virtual IGraphicsDevice* getApplicationDevice() const = 0;
        virtual int64_t getPreferredSwapchainFormatOnApplicationDevice(XrSwapchainUsageFlags usageFlags,
                                                                       bool preferSRGB = true) const = 0;

        template <typename SessionData>
        SessionData* getSessionData() const {
            return reinterpret_cast<SessionData*>(getSessionDataPtr());
        }
    };

    // A factory to create composition frameworks for each session.
    struct ICompositionFrameworkFactory {
        virtual ~ICompositionFrameworkFactory() = default;

        virtual void CreateSession(const XrSessionCreateInfo* createInfo, XrSession session) = 0;
        virtual void DestroySession(XrSession session) = 0;

        virtual ICompositionFramework* getCompositionFramework(XrSession session) = 0;
    };

    std::shared_ptr<ICompositionFrameworkFactory>
    createCompositionFrameworkFactory(const XrInstanceCreateInfo& info,
                                      XrInstance instance,
                                      PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr,
                                      CompositionApi compositionApi);

    namespace internal {

#ifdef XR_USE_GRAPHICS_API_D3D11
        std::shared_ptr<IGraphicsDevice> createD3D11CompositionDevice(LUID adapterLuid);
        std::shared_ptr<IGraphicsDevice> wrapApplicationDevice(const XrGraphicsBindingD3D11KHR& bindings);
#endif

#ifdef XR_USE_GRAPHICS_API_D3D12
        std::shared_ptr<IGraphicsDevice> wrapApplicationDevice(const XrGraphicsBindingD3D12KHR& bindings);
#endif

    } // namespace internal

} // namespace openxr_api_layer::graphics
