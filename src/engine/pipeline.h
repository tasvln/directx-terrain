#pragma once

#include "utils/pch.h"
#include "shader.h"

class Pipeline {
public:
    Pipeline(
        ComPtr<ID3D12Device2> device,
        const Shader& vertexShader,
        const Shader& pixelShader,
        const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
        const std::vector<D3D12_ROOT_PARAMETER>& rootParams = {},
        const std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers = {},
        DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
        // change to a setter method to make it flexible
        const D3D12_RASTERIZER_DESC* customRasterizer = nullptr,
        const D3D12_BLEND_DESC* customBlend = nullptr,
        const D3D12_DEPTH_STENCIL_DESC* customDepthStencil = nullptr
    );

    ~Pipeline() = default;

    ComPtr<ID3D12PipelineState> getPipelineState() const { return pipelineState; }
    ComPtr<ID3D12RootSignature> getRootSignature() const { return rootSignature; }

private:
    ComPtr<ID3D12PipelineState> pipelineState;
    ComPtr<ID3D12RootSignature> rootSignature;
};
