#include "grid.h"

#include "engine/shader.h"
#include "engine/command_queue.h"
#include "engine/descriptor_heap.h"
#include "engine/mesh.h"
#include "engine/pipeline.h"
#include "engine/material.h"
#include "engine/resources/constant.h"

Grid::Grid(
    ComPtr<ID3D12Device2> device,
    CommandQueue* commandQueue,
    DescriptorHeap* srvHeap
) : device(device)
{
    // Simple plane geometry
    std::vector<VertexStruct> vertices = {
        { XMFLOAT4(-100.0f, 0.0f, -100.0f, 1.0f), XMFLOAT3(0.0, 1.0, 0.0), XMFLOAT4(1.0, 0.0, 0.0, 1.0), XMFLOAT2(0.0, 0.0) },
        { XMFLOAT4(-100.0f, 0.0f,  100.0f, 1.0f), XMFLOAT3(0.0, 1.0, 0.0), XMFLOAT4(1.0, 0.0, 0.0, 1.0), XMFLOAT2(0.0, 1.0) },
        { XMFLOAT4( 100.0f, 0.0f, -100.0f, 1.0f), XMFLOAT3(0.0, 1.0, 0.0), XMFLOAT4(1.0, 0.0, 0.0, 1.0), XMFLOAT2(1.0, 0.0) },
        { XMFLOAT4( 100.0f, 0.0f,  100.0f, 1.0f), XMFLOAT3(0.0, 1.0, 0.0), XMFLOAT4(1.0, 0.0, 0.0, 1.0), XMFLOAT2(1.0, 1.0) },
    };

    std::vector<uint32_t> indices = { 0, 1, 2, 2, 1, 3 };

    auto mat = std::make_shared<Material>();
    mesh = std::make_unique<Mesh>(device, vertices, indices, mat);

    // Load shaders
    Shader vs(L"assets/shaders/grid_vs.cso");
    Shader ps(L"assets/shaders/grid_ps.cso");

    // Input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> layout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Root signature — MVP (b0) and GridParams (b1)
    CD3DX12_ROOT_PARAMETER mvpParam;
    mvpParam.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_ROOT_PARAMETER gridParamsParam;
    gridParamsParam.InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    std::vector<D3D12_ROOT_PARAMETER> rootParams = {
        mvpParam, gridParamsParam
    };

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers = {};

    D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE; // no back-face culling
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.FrontCounterClockwise = FALSE;

    // Create pipeline
    pipeline = std::make_unique<Pipeline>(
        device,
        vs,
        ps,
        layout,
        rootParams,
        samplers,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        &rasterDesc
    );

    // Constant buffers
    mvpBuffer = std::make_unique<ConstantBuffer>(device, static_cast<UINT>(sizeof(MVPConstantStruct)));
    gridParamsBuffer = std::make_unique<ConstantBuffer>(device, static_cast<UINT>(sizeof(GridParams)));
}

void Grid::updateMVP(const XMMATRIX& viewProj)
{
    MVPConstantStruct mvp{};
    mvp.model = XMMatrixTranspose(XMMatrixIdentity());
    mvp.viewProj = XMMatrixTranspose(viewProj);
    mvpBuffer->update(&mvp, sizeof(MVPConstantStruct));
}

void Grid::updateGridParams(const XMFLOAT3& camPos, float fadeDistance)
{
    GridParams params{};
    params.cameraPos = camPos;
    params.gridFadeDistance = fadeDistance;
    gridParamsBuffer->update(&params, sizeof(GridParams));
}

void Grid::draw(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->SetPipelineState(pipeline->getPipelineState().Get());
    cmdList->SetGraphicsRootSignature(pipeline->getRootSignature().Get());

    cmdList->SetGraphicsRootConstantBufferView(0, mvpBuffer->getGPUAddress());
    cmdList->SetGraphicsRootConstantBufferView(1, gridParamsBuffer->getGPUAddress());

    mesh->draw(cmdList, 0);
}

float Grid::sampleHeight(float x, float z) const
{
    return 0.0f;
}
