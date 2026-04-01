#include "sky.h"
#include "engine/shader.h"
#include "engine/pipeline.h"
#include "engine/resources/constant.h"
#include "engine/descriptor_heap.h"
#include "engine/scene/weather/clock.h"

struct SkyVertex
{
    XMFLOAT2 pos; // clip space position (-1 to 1)
    XMFLOAT2 uv;  // 0 to 1
};

Sky::Sky(ComPtr<ID3D12Device2> device, DXGI_FORMAT rtvFormat)
    : device(device)
{
    buildFullscreenQuad();
    buildPipeline(rtvFormat);

    paramsBuffer = std::make_unique<ConstantBuffer>(
        device,
        static_cast<UINT>(sizeof(SkyParams))
    );
}

void Sky::update(
    const TimeOfDayState& tod,
    const XMMATRIX& view,
    const XMMATRIX& proj,
    float totalTime,
    float cloudCoverage
)
{
    // Compute inverse view-proj so the shader can reconstruct world-space ray direction
    // from each pixel's clip-space position
    XMMATRIX viewProj    = view * proj;
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

    // Moon is always opposite the sun
    XMFLOAT3 moonDir = {
        -tod.sunDirection.x,
        -tod.sunDirection.y,
        -tod.sunDirection.z
    };

    // Moon intensity is inverse of sun — full moon at midnight, invisible at noon
    float moonIntensity = 1.0f - tod.sunIntensity;

    SkyParams params{};
    params.invViewProj   = XMMatrixTranspose(invViewProj);
    params.sunDirection  = tod.sunDirection;
    params.sunColor      = tod.sunColor;
    params.sunIntensity  = tod.sunIntensity;
    params.ambientColor  = tod.ambientColor;
    params.moonDirection = moonDir;
    params.moonIntensity = moonIntensity;
    params.time          = totalTime;
    params.cloudCoverage = cloudCoverage;
    params.cloudSpeed    = 0.02f;

    paramsBuffer->update(&params, sizeof(SkyParams));
}

void Sky::buildFullscreenQuad()
{
    // A quad covering the entire screen in clip space
    // We use two triangles making a rectangle from -1,-1 to 1,1
    SkyVertex vertices[] = {
        { { -1.0f,  1.0f }, { 0.0f, 0.0f } }, // top left
        { {  1.0f,  1.0f }, { 1.0f, 0.0f } }, // top right
        { { -1.0f, -1.0f }, { 0.0f, 1.0f } }, // bottom left
        { {  1.0f, -1.0f }, { 1.0f, 1.0f } }, // bottom right
    };

    UINT vbSize = sizeof(vertices);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    throwFailed(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&vertexBuffer)
    ));

    void* mapped = nullptr;
    throwFailed(vertexBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, vertices, vbSize);
    vertexBuffer->Unmap(0, nullptr);

    vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes    = vbSize;
    vbView.StrideInBytes  = sizeof(SkyVertex);
}

void Sky::buildPipeline(DXGI_FORMAT rtvFormat)
{
    // b0 = SkyParams
    CD3DX12_ROOT_PARAMETER paramsParam;
    paramsParam.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_PARAMETER fogParam;
    fogParam.InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    std::vector<D3D12_ROOT_PARAMETER> rootParams = { paramsParam, fogParam };

    std::vector<D3D12_INPUT_ELEMENT_DESC> layout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(XMFLOAT2), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Sky renders behind everything so disable depth write
    // but keep depth test so terrain occludes the sky correctly
    D3D12_DEPTH_STENCIL_DESC depthDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // don't write depth
    depthDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // No backface culling — fullscreen quad always faces camera
    D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

    pipeline = std::make_unique<Pipeline>(
        device,
        Shader(L"assets/shaders/sky_vs.cso"),
        Shader(L"assets/shaders/sky_ps.cso"),
        layout, rootParams,
        std::vector<D3D12_STATIC_SAMPLER_DESC>{},
        rtvFormat,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        &rasterDesc,
        nullptr,
        &depthDesc
    );
}

void Sky::draw(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS fogCBV)
{
    cmdList->SetPipelineState(pipeline->getPipelineState().Get());
    cmdList->SetGraphicsRootSignature(pipeline->getRootSignature().Get());
    
    cmdList->SetGraphicsRootConstantBufferView(0, paramsBuffer->getGPUAddress());
    cmdList->SetGraphicsRootConstantBufferView(1, fogCBV);

    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // 4 vertices as a triangle strip = 2 triangles = fullscreen quad
    cmdList->DrawInstanced(4, 1, 0, 0);
}