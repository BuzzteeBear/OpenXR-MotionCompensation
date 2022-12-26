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

#pragma once
  
namespace
{
    // A generic timer.
    struct ITimer
    {
        virtual ~ITimer() = default;

        virtual void start() = 0;
        virtual void stop() = 0;

        virtual uint64_t query(bool reset = true) const = 0;
    };

    // Quick and dirty API helper
    template <class T, class... Types>
    inline constexpr bool is_any_of_v = std::disjunction_v<std::is_same<T, Types>...>;

    template <typename E>
    inline constexpr auto to_integral(E e) -> typename std::underlying_type_t<E>
    {
        return static_cast<std::underlying_type_t<E>>(e);
    }

} // namespace

namespace graphics
{
    constexpr uint32_t ViewCount = 2;

    enum class Api
    {
        D3D11,
        D3D12
    };

    // Type traits for D3D11.
    struct D3D11
    {
        static constexpr Api Api = Api::D3D11;

        using Device = ID3D11Device*;
        using Context = ID3D11DeviceContext*;
        using Texture = ID3D11Texture2D*;
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
        using ComputeShader = ID3D11ComputeShader*;
        using ShaderInputView = ID3D11ShaderResourceView*;
        using ComputeShaderOutputView = ID3D11UnorderedAccessView*;
        using RenderTargetView = ID3D11RenderTargetView*;
        using DepthStencilView = ID3D11DepthStencilView*;

        // clang-format off
            template <typename T>
            static constexpr bool is_concrete_api_v = is_any_of_v<T,
              Device, Context, Texture, Buffer, Mesh, PixelShader, ComputeShader, ShaderInputView, RenderTargetView, DepthStencilView>;
        // clang-format on
    };

    // Type traits for D3D12.
    struct D3D12
    {
        static constexpr Api Api = Api::D3D12;

        using Device = ID3D12Device*;
        using Context = ID3D12GraphicsCommandList*;
        using Texture = ID3D12Resource*;
        using Buffer = ID3D12Resource*;
        struct MeshData
        {
            D3D12_VERTEX_BUFFER_VIEW* vertexBuffer;
            D3D12_INDEX_BUFFER_VIEW* indexBuffer;
            UINT numIndices;
        };
        using Mesh = MeshData*;
        struct ShaderData
        {
            ID3D12RootSignature* rootSignature;
            ID3D12PipelineState* pipelineState;
        };
        using PixelShader = ShaderData*;
        using ComputeShader = ShaderData*;
        using ShaderInputView = D3D12_CPU_DESCRIPTOR_HANDLE*;
        using ComputeShaderOutputView = D3D12_CPU_DESCRIPTOR_HANDLE*;
        using RenderTargetView = D3D12_CPU_DESCRIPTOR_HANDLE*;
        using DepthStencilView = D3D12_CPU_DESCRIPTOR_HANDLE*;

        // clang-format off
            template <typename T>
            static constexpr bool is_concrete_api_v = is_any_of_v<T,
              Device, Context, Texture, Buffer, Mesh, PixelShader, ComputeShader, ShaderInputView, RenderTargetView, DepthStencilView>;
        // clang-format on
    };

    // Graphics API helper
    template <typename ConcreteType, typename InterfaceType>
    inline auto GetAs(const InterfaceType* pInterface)
    {
        constexpr auto api = D3D12::is_concrete_api_v<ConcreteType> ? Api::D3D12 : Api::D3D11;
        return reinterpret_cast<ConcreteType>(api == pInterface->getApi() ? pInterface->getNativePtr() : nullptr);
    }

    // A few handy texture formats.
    enum class TextureFormat
    {
        R32G32B32A32_FLOAT,
        R16G16B16A16_UNORM,
        R10G10B10A2_UNORM,
        R8G8B8A8_UNORM
    };

    enum class SamplerType
    {
        NearestClamp,
        LinearClamp
    };

    // A list of supported GPU Architectures.
    enum class GpuArchitecture
    {
        Unknown,
        AMD,
        Intel,
        NVidia
    };

    enum class TextStyle
    {
        Normal,
        Bold
    };

    enum class FrameAnalyzerHeuristic
    {
        Unknown,
        ForwardRender,
        DeferredCopy,
        Fallback
    };

    struct IDevice;
    struct ITexture;

    // A shader that will be rendered on a quad wrapping the entire target.
    struct IQuadShader
    {
        virtual ~IQuadShader() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::PixelShader>(this);
        }
    };

    // A compute shader.
    struct IComputeShader
    {
        virtual ~IComputeShader() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void updateThreadGroups(const std::array<unsigned int, 3>& threadGroups) = 0;
        virtual const std::array<unsigned int, 3>& getThreadGroups() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::ComputeShader>(this);
        }
    };

    // The view of a texture in input of a shader.
    struct IShaderInputTextureView
    {
        virtual ~IShaderInputTextureView() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::ShaderInputView>(this);
        }
    };

    // The view of a texture in output of a compute shader.
    struct IComputeShaderOutputView
    {
        virtual ~IComputeShaderOutputView() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::ComputeShaderOutputView>(this);
        }
    };

    // The view of a texture in output of a quad shader or used for rendering.
    struct IRenderTargetView
    {
        virtual ~IRenderTargetView() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::RenderTargetView>(this);
        }
    };

    // The view of a texture as a depth buffer.
    struct IDepthStencilView
    {
        virtual ~IDepthStencilView() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::DepthStencilView>(this);
        }
    };

    // A texture, plain and simple!
    struct ITexture
    {
        virtual ~ITexture() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;
        virtual const XrSwapchainCreateInfo& getInfo() const = 0;
        virtual bool isArray() const = 0;

        virtual std::shared_ptr<IShaderInputTextureView> getShaderResourceView(int32_t slice = -1) const = 0;
        virtual std::shared_ptr<IComputeShaderOutputView> getUnorderedAccessView(int32_t slice = -1) const = 0;
        virtual std::shared_ptr<IRenderTargetView> getRenderTargetView(int32_t slice = -1) const = 0;
        virtual std::shared_ptr<IDepthStencilView> getDepthStencilView(int32_t slice = -1) const = 0;

        virtual void uploadData(const void* buffer, uint32_t rowPitch, int32_t slice = -1) = 0;
        virtual void copyTo(std::shared_ptr<ITexture> destination) = 0;
        virtual void copyTo(uint32_t srcX, uint32_t srcY, int32_t srcSlice, std::shared_ptr<ITexture> destination) = 0;
        virtual void copyTo(std::shared_ptr<ITexture> destination, uint32_t dstX, uint32_t dstY, int32_t dstSlice) = 0;

        virtual void setState(D3D12_RESOURCE_STATES newState) = 0;
        virtual void pushState(D3D12_RESOURCE_STATES newState) = 0;
        virtual void popState() = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::Texture>(this);
        }
    };

    // A buffer to be used with shaders.
    struct IShaderBuffer
    {
        virtual ~IShaderBuffer() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void uploadData(const void* buffer, size_t count) = 0;

        virtual void pushState(D3D12_RESOURCE_STATES newState) = 0;
        virtual void popState() = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::Buffer>(this);
        }
    };

    struct SimpleMeshVertex
    {
        XrVector3f Position;
        XrVector3f Color;
    };

    // A simple (unskinned) mesh.
    struct ISimpleMesh
    {
        virtual ~ISimpleMesh() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::Mesh>(this);
        }
    };

    // A graphics execution context (eg: command list).
    struct IContext
    {
        virtual ~IContext() = default;

        virtual Api getApi() const = 0;
        virtual std::shared_ptr<IDevice> getDevice() const = 0;

        virtual void* getNativePtr() const = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::Context>(this);
        }
    };

    // A graphics device.
    struct IDevice
    {
        virtual ~IDevice() = default;

        virtual Api getApi() const = 0;

        virtual const std::string& getDeviceName() const = 0;

        virtual int64_t getTextureFormat(TextureFormat format) const = 0;
        virtual bool isTextureFormatSRGB(int64_t format) const = 0;

        virtual void saveContext(bool clear = true) = 0;
        virtual void restoreContext() = 0;
        virtual void flushContext(bool blocking = false, bool isEndOfFrame = false) = 0;

        virtual std::shared_ptr<ITexture> createTexture(const XrSwapchainCreateInfo& info,
                                                        std::string_view debugName,
                                                        int64_t overrideFormat = 0,
                                                        uint32_t rowPitch = 0,
                                                        uint32_t imageSize = 0,
                                                        const void* initialData = nullptr) = 0;

        virtual std::shared_ptr<IShaderBuffer> createBuffer(size_t size,
                                                            std::string_view debugName,
                                                            const void* initialData = nullptr,
                                                            bool immutable = false) = 0;

        virtual std::shared_ptr<ISimpleMesh> createSimpleMesh(std::vector<SimpleMeshVertex>& vertices,
                                                              std::vector<uint16_t>& indices,
                                                              std::string_view debugName) = 0;

        virtual std::shared_ptr<IQuadShader> createQuadShader(const std::filesystem::path& shaderFile,
                                                              const std::string& entryPoint,
                                                              std::string_view debugName,
                                                              const D3D_SHADER_MACRO* defines = nullptr,
                                                              std::filesystem::path includePath = "") = 0;

        virtual std::shared_ptr<IComputeShader> createComputeShader(const std::filesystem::path& shaderFile,
                                                                    const std::string& entryPoint,
                                                                    std::string_view debugName,
                                                                    const std::array<unsigned int, 3>& threadGroups,
                                                                    const D3D_SHADER_MACRO* defines = nullptr,
                                                                    std::filesystem::path includePath = "") = 0;

        // Must be invoked prior to setting the input/output.
        virtual void setShader(std::shared_ptr<IQuadShader> shader, SamplerType sampler) = 0;

        // Must be invoked prior to setting the input/output.
        virtual void setShader(std::shared_ptr<IComputeShader> shader, SamplerType sampler) = 0;

        virtual void setShaderInput(uint32_t slot, std::shared_ptr<ITexture> input, int32_t slice = -1) = 0;
        virtual void setShaderInput(uint32_t slot, std::shared_ptr<IShaderBuffer> input) = 0;
        virtual void setShaderOutput(uint32_t slot, std::shared_ptr<ITexture> output, int32_t slice = -1) = 0;

        virtual void dispatchShader(bool doNotClear = false) const = 0;

        virtual void setRenderTargets(size_t numRenderTargets,
                                      std::shared_ptr<ITexture>* renderTargets,
                                      int32_t* renderSlices = nullptr,
                                      const XrRect2Di* viewport0 = nullptr,
                                      std::shared_ptr<ITexture> depthBuffer = nullptr,
                                      int32_t depthSlice = -1) = 0;
        virtual void unsetRenderTargets() = 0;

        virtual XrExtent2Di getViewportSize() const = 0;

        virtual void clearColor(float top, float left, float bottom, float right, const XrColor4f& color) const = 0;
        virtual void clearDepth(float value) = 0;

        virtual void setViewProjection(const xr::math::ViewProjection& view) = 0;
        virtual void draw(std::shared_ptr<ISimpleMesh> mesh,
                          const XrPosef& pose,
                          XrVector3f scaling = {1.0f, 1.0f, 1.0f}) = 0;

        virtual void resolveQueries() = 0;

        virtual void blockCallbacks() = 0;
        virtual void unblockCallbacks() = 0;

        using SetRenderTargetEvent =
            std::function<void(std::shared_ptr<IContext>, std::shared_ptr<ITexture> renderTarget)>;
        virtual void registerSetRenderTargetEvent(SetRenderTargetEvent event) = 0;
        using UnsetRenderTargetEvent = std::function<void(std::shared_ptr<IContext>)>;
        virtual void registerUnsetRenderTargetEvent(UnsetRenderTargetEvent event) = 0;
        using CopyTextureEvent = std::function<void(std::shared_ptr<IContext> /* context */,
                                                    std::shared_ptr<ITexture> /* source */,
                                                    std::shared_ptr<ITexture> /* destination */,
                                                    int /* sourceSlice */,
                                                    int /* destinationSlice */)>;
        virtual void registerCopyTextureEvent(CopyTextureEvent event) = 0;


        virtual void shutdown() = 0;

        virtual bool isEventsSupported() const = 0;

        virtual uint32_t getBufferAlignmentConstraint() const = 0;
        virtual uint32_t getTextureAlignmentConstraint() const = 0;

        virtual void* getNativePtr() const = 0;
        virtual void* getContextPtr() const = 0;

        virtual void executeDebugWorkload() = 0;

        template <typename ApiTraits>
        auto getAs() const
        {
            return GetAs<typename ApiTraits::Device>(this);
        }

        template <typename ApiTraits>
        auto getContextAs() const
        {
            return reinterpret_cast<typename ApiTraits::Context>(ApiTraits::Api == getApi() ? getContextPtr()
                                                                                            : nullptr);
        }
    };

    // A texture post-processor.
    struct IImageProcessor
    {
        virtual ~IImageProcessor() = default;

        virtual void reload() = 0;
        virtual void update() = 0;
        virtual void process(std::shared_ptr<ITexture> input,
                             std::shared_ptr<ITexture> output,
                             std::vector<std::shared_ptr<ITexture>>& textures,
                             std::array<uint8_t, 1024>& blob) = 0;
    }; 

} // namespace graphics


