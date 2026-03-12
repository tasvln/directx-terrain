#include "pipeline.h"
#include <comdef.h>

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
) {

    if (!vertexShader.getBytecode() || !pixelShader.getBytecode()) {
        LOG_ERROR(L"Shader bytecode is null!");
        return;
    }

    // Root signature
    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = static_cast<UINT>(rootParams.size());
    rootDesc.pParameters = rootParams.empty() ? nullptr : rootParams.data();
    rootDesc.NumStaticSamplers = static_cast<UINT>(samplers.size());
    rootDesc.pStaticSamplers = samplers.empty() ? nullptr : samplers.data();
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSig,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            LOG_ERROR(L"Root signature serialization error: %S", (char*)errorBlob->GetBufferPointer());
        } else {
            LOG_ERROR(L"Failed to serialize root signature without error blob.");
        }
        throwFailed(hr);
    }

    hr = device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );
    throwFailed(hr);

    // PSO description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

    psoDesc.InputLayout = { 
        inputLayout.data(), 
        static_cast<UINT>(inputLayout.size()) 
    };

    psoDesc.pRootSignature = rootSignature.Get();

    psoDesc.VS = { 
        vertexShader.getBytecode()->GetBufferPointer(),
        vertexShader.getBytecode()->GetBufferSize()
    };

    psoDesc.PS = { 
        pixelShader.getBytecode()->GetBufferPointer(),
        pixelShader.getBytecode()->GetBufferSize()
    };

    // psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState = customRasterizer ? *customRasterizer : CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // psoDesc.RasterizerState = rasterizerDesc;

    // psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = customBlend ? *customBlend : CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    // psoDesc.BlendState = blendDesc;

    psoDesc.DepthStencilState = customDepthStencil ? *customDepthStencil : CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    // psoDesc.DepthStencilState = depthStencilDesc;

    psoDesc.SampleMask = UINT_MAX;

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;

    psoDesc.DSVFormat = dsvFormat;

    psoDesc.SampleDesc.Count = 1;
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        LOG_ERROR(L"CreateGraphicsPipelineState failed: HRESULT = 0x%08X", hr);
        std::cerr << "CreateGraphicsPipelineState failed. HRESULT = 0x"
                << std::hex << hr << std::dec << std::endl;

        // Dump D3D12 debug messages (critical - will show the exact reason)
        // dumpD3D12DebugMessages(device);
        LOG_D3D12_MESSAGES(device);

        throwFailed(hr);
    }
}