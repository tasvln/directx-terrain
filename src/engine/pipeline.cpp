#include "pipeline.h"
#include <comdef.h>

// -------------------------------------------------------
// Shared helper — both constructors use this
// -------------------------------------------------------
void Pipeline::buildRootSignature(
    ComPtr<ID3D12Device2> device,
    const std::vector<D3D12_ROOT_PARAMETER>& rootParams,
    const std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers
)
{
    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters     = static_cast<UINT>(rootParams.size());
    rootDesc.pParameters       = rootParams.empty() ? nullptr : rootParams.data();
    rootDesc.NumStaticSamplers = static_cast<UINT>(samplers.size());
    rootDesc.pStaticSamplers   = samplers.empty() ? nullptr : samplers.data();
    rootDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSig,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob)
            LOG_ERROR(L"Root signature error: %S", (char*)errorBlob->GetBufferPointer());
        throwFailed(hr);
    }

    throwFailed(device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    ));
}

// -------------------------------------------------------
// Original VS + PS constructor — unchanged logic,
// just calls the shared helper now
// -------------------------------------------------------
Pipeline::Pipeline(
    ComPtr<ID3D12Device2> device,
    const Shader& vertexShader,
    const Shader& pixelShader,
    const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
    const std::vector<D3D12_ROOT_PARAMETER>& rootParams,
    const std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    const D3D12_RASTERIZER_DESC* customRasterizer,
    const D3D12_BLEND_DESC* customBlend,
    const D3D12_DEPTH_STENCIL_DESC* customDepthStencil
)
{
    buildRootSignature(device, rootParams, samplers);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout     = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
    psoDesc.pRootSignature  = rootSignature.Get();
    psoDesc.VS              = { vertexShader.getBytecode()->GetBufferPointer(), vertexShader.getBytecode()->GetBufferSize() };
    psoDesc.PS              = { pixelShader.getBytecode()->GetBufferPointer(),  pixelShader.getBytecode()->GetBufferSize()  };
    psoDesc.RasterizerState = customRasterizer  ? *customRasterizer  : CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState      = customBlend       ? *customBlend       : CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = customDepthStencil ? *customDepthStencil : CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask              = UINT_MAX;
    psoDesc.PrimitiveTopologyType   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets        = 1;
    psoDesc.RTVFormats[0]           = rtvFormat;
    psoDesc.DSVFormat               = dsvFormat;
    psoDesc.SampleDesc.Count        = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        LOG_ERROR(L"CreateGraphicsPipelineState failed: 0x%08X", hr);
        LOG_D3D12_MESSAGES(device);
        throwFailed(hr);
    }
}

// -------------------------------------------------------
// New VS + HS + DS + PS constructor for tessellation
// -------------------------------------------------------
Pipeline::Pipeline(
    ComPtr<ID3D12Device2> device,
    const Shader& vertexShader,
    const Shader& hullShader,
    const Shader& domainShader,
    const Shader& pixelShader,
    const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout,
    const std::vector<D3D12_ROOT_PARAMETER>& rootParams,
    const std::vector<D3D12_STATIC_SAMPLER_DESC>& samplers,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    const D3D12_RASTERIZER_DESC* customRasterizer,
    const D3D12_BLEND_DESC* customBlend,
    const D3D12_DEPTH_STENCIL_DESC* customDepthStencil
)
{
    buildRootSignature(device, rootParams, samplers);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout     = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
    psoDesc.pRootSignature  = rootSignature.Get();

    psoDesc.VS = { vertexShader.getBytecode()->GetBufferPointer(), vertexShader.getBytecode()->GetBufferSize() };
    psoDesc.HS = { hullShader.getBytecode()->GetBufferPointer(),   hullShader.getBytecode()->GetBufferSize()   };
    psoDesc.DS = { domainShader.getBytecode()->GetBufferPointer(), domainShader.getBytecode()->GetBufferSize() };
    psoDesc.PS = { pixelShader.getBytecode()->GetBufferPointer(),  pixelShader.getBytecode()->GetBufferSize()  };

    psoDesc.RasterizerState   = customRasterizer   ? *customRasterizer   : CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState        = customBlend        ? *customBlend        : CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = customDepthStencil ? *customDepthStencil : CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask        = UINT_MAX;

    // ← This is the key difference from the regular pipeline
    // Patches instead of triangles — the HS/DS handle subdivision
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;

    psoDesc.NumRenderTargets  = 1;
    psoDesc.RTVFormats[0]     = rtvFormat;
    psoDesc.DSVFormat         = dsvFormat;
    psoDesc.SampleDesc.Count  = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        LOG_ERROR(L"CreateGraphicsPipelineState (tessellation) failed: 0x%08X", hr);
        LOG_D3D12_MESSAGES(device);
        throwFailed(hr);
    }
}