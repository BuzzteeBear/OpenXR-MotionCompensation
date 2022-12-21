#pragma once

#include "pch.h"
#include "interfaces.h"

namespace graphics
{
    void HookForD3D11DebugLayer();
    void UnhookForD3D11DebugLayer();
    std::shared_ptr<IDevice> WrapD3D11Device(ID3D11Device* device,
                                             bool enableOculusQuirk = false);
    std::shared_ptr<IDevice> WrapD3D11TextDevice(ID3D11Device* device);
    std::shared_ptr<ITexture> WrapD3D11Texture(std::shared_ptr<IDevice> device,
                                               const XrSwapchainCreateInfo& info,
                                               ID3D11Texture2D* texture,
                                               std::string_view debugName);
    
    void EnableD3D12DebugLayer();
    std::shared_ptr<IDevice> WrapD3D12Device(ID3D12Device* device,
                                             ID3D12CommandQueue* queue,
                                             bool enableVarjoQuirk = false);
    std::shared_ptr<ITexture> WrapD3D12Texture(std::shared_ptr<IDevice> device,
                                               const XrSwapchainCreateInfo& info,
                                               ID3D12Resource* texture,
                                               D3D12_RESOURCE_STATES initialState,
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

        const std::string_view QuadVertexShader = R"_(
void vsMain(in uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD0)
{
    texcoord = float2((id == 1) ? 2.0 : 0.0, (id == 2) ? 2.0 : 0.0);
    position = float4(texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}
)_";
    } // namespace d3dcommon
} // namespace graphics
