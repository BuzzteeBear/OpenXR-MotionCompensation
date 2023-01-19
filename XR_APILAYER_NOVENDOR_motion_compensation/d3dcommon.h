#pragma once

#include "interfaces.h"

namespace graphics
{
    std::shared_ptr<IDevice> WrapD3D11Device(ID3D11Device* device);
    std::shared_ptr<ITexture> WrapD3D11Texture(std::shared_ptr<IDevice> device,
                                               const XrSwapchainCreateInfo& info,
                                               ID3D11Texture2D* texture,
                                               std::string_view debugName);
    std::shared_ptr<IDevice> WrapD3D12Device(ID3D12Device* device,
                                             ID3D12CommandQueue* queue);
    std::shared_ptr<ITexture> WrapD3D12Texture(std::shared_ptr<IDevice> device,
                                               const XrSwapchainCreateInfo& info,
                                               ID3D12Resource* texture,
                                               std::string_view debugName);                                            

    namespace d3dcommon
    {
        struct ModelConstantBuffer
        {
            DirectX::XMFLOAT4X4 Model;
        };

        struct ViewProjectionConstantBuffer
        {
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
} // namespace graphics
