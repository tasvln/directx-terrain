#include "particles.h"

#include "engine/shader.h"
#include "engine/command_queue.h"
#include "engine/resources/constant.h"

#include <random>

Particles::Particles(ComPtr<ID3D12Device2> device, CommandQueue* commandQueue)
    : device(device), commandQueue(commandQueue)
{
    buildBuffers();
    buildComputePipeline();
    buildRenderPipeline();

    paramsBuffer = std::make_unique<ConstantBuffer>(
        device,
        static_cast<UINT>(sizeof(ParticleParams))
    );
}

// buildBuffers — create particle StructuredBuffer -> UAV for CS to write, SRV for VS to read
void Particles::buildBuffers()
{
    UINT bufferSize = PARTICLE_COUNT * sizeof(ParticleData);

    // Default heap — GPU only, fast
    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(
        bufferSize,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS // needed for UAV
    );

    throwFailed(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&particleBuffer)
    ));

    // Upload heap — CPU writes initial data here
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    throwFailed(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&particleUploadBuffer)
    ));

    // Initialize particles spread across the sky
    std::vector<ParticleData> initial(PARTICLE_COUNT);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posXZ(-100.0f, 100.0f);
    std::uniform_real_distribution<float> posY(0.0f, 50.0f);
    std::uniform_real_distribution<float> seedDist(0.0f, 1.0f);

    for (auto& p : initial)
    {
        p.position = { posXZ(rng), posY(rng), posXZ(rng) };
        p.velocity = { 0.0f, -10.0f, 0.0f }; // falling down
        p.life     = seedDist(rng);            // staggered start
        p.seed     = seedDist(rng);
        p.type = (p.seed < 0.5f) ? 0.0f : 1.0f;
    }

    // Upload initial data
    void* mapped = nullptr;
    particleUploadBuffer->Map(0, nullptr, &mapped);
    memcpy(mapped, initial.data(), bufferSize);
    particleUploadBuffer->Unmap(0, nullptr);

    auto cmdList = commandQueue->getCommandList();

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        particleBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->CopyResource(particleBuffer.Get(), particleUploadBuffer.Get());

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        particleBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    cmdList->ResourceBarrier(1, &barrier);

    UINT64 fence = commandQueue->executeCommandList(cmdList);
    commandQueue->fenceWait(fence);

    // Create descriptor heap for UAV + SRV
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2; // slot 0 = UAV, slot 1 = SRV
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    throwFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));
    descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // UAV — slot 0
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format                     = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.NumElements         = PARTICLE_COUNT;
    uavDesc.Buffer.StructureByteStride = sizeof(ParticleData);

    CD3DX12_CPU_DESCRIPTOR_HANDLE uavCPU(
        descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, descriptorSize);
    device->CreateUnorderedAccessView(particleBuffer.Get(), nullptr, &uavDesc, uavCPU);

    uavHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, descriptorSize);

    // SRV — slot 1
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format                     = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements         = PARTICLE_COUNT;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleData);

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvCPU(
        descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, descriptorSize);
    device->CreateShaderResourceView(particleBuffer.Get(), &srvDesc, srvCPU);

    srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, descriptorSize);
}

void Particles::buildComputePipeline()
{
    // Root sig: b0 = params, u0 = particle UAV
    CD3DX12_ROOT_PARAMETER params;
    params.InitAsConstantBufferView(0, 0);

    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER uavParam;
    uavParam.InitAsDescriptorTable(1, &uavRange);

    std::vector<D3D12_ROOT_PARAMETER> rootParams = { params, uavParam };

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = static_cast<UINT>(rootParams.size());
    rootDesc.pParameters   = rootParams.data();
    rootDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serialized, error;
    D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
    device->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&computeRootSig));

    Shader cs(L"assets/shaders/particles_cs.cso");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = computeRootSig.Get();
    psoDesc.CS = { cs.getBytecode()->GetBufferPointer(), cs.getBytecode()->GetBufferSize() };

    throwFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computePSO)));
}

void Particles::buildRenderPipeline()
{
    // Root sig: b0 = params, t0 = particle SRV
    CD3DX12_ROOT_PARAMETER params;
    params.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER srvParam;
    srvParam.InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_VERTEX);

    std::vector<D3D12_ROOT_PARAMETER> rootParams = { params, srvParam };

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = static_cast<UINT>(rootParams.size());
    rootDesc.pParameters   = rootParams.data();
    rootDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_NONE; // no input assembler

    ComPtr<ID3DBlob> serialized, error;
    D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
    device->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&renderRootSig));

    // Alpha blend — particles are transparent
    D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    blendDesc.RenderTarget[0].BlendEnable    = TRUE;
    blendDesc.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // No depth write — particles don't occlude each other
    D3D12_DEPTH_STENCIL_DESC depthDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

    // No input layout — VS generates quads from SV_VertexID
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature    = renderRootSig.Get();
    psoDesc.InputLayout       = { nullptr, 0 };

    Shader vs(L"assets/shaders/particles_vs.cso");
    Shader ps(L"assets/shaders/particles_ps.cso");

    psoDesc.VS                = { vs.getBytecode()->GetBufferPointer(), vs.getBytecode()->GetBufferSize() };
    psoDesc.PS                = { ps.getBytecode()->GetBufferPointer(), ps.getBytecode()->GetBufferSize() };
    psoDesc.BlendState        = blendDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.RasterizerState   = rasterDesc;
    psoDesc.SampleMask        = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets  = 1;
    psoDesc.RTVFormats[0]     = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat         = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count  = 1;

    throwFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&renderPSO)));
}

void Particles::update(
    ID3D12GraphicsCommandList* cmdList,
    const XMFLOAT3& cameraPos,
    const XMMATRIX& viewProj,
    const XMFLOAT3& camRight,
    const XMFLOAT3& camUp,
    float deltaTime,
    float rainIntensity,
    float snowIntensity,
    const XMFLOAT2& windDir,
    float windStrength,
    float terrainBaseHeight
)
{
    ParticleParams params{};
    params.cameraPos      = cameraPos;
    params.deltaTime      = deltaTime;
    params.windDirection  = windDir;
    params.windStrength   = windStrength;
    params.gravity        = 9.81f;
    params.rainIntensity  = rainIntensity;
    params.snowIntensity  = snowIntensity;
    params.spawnRadius    = 80.0f;
    params.spawnHeight    = 40.0f;
    params.viewProj       = XMMatrixTranspose(viewProj);
    params.camRight       = camRight;
    params.camUp          = camUp;

    params.rainSpeed      = 35.0f;
    params.rainStretch    = 8.0f;
    params.rainWidth      = 0.03f;
    params.rainTurbulence = 2.0f;

    params.snowSpeed = 4.0f;
    params.snowDrift = 2.0f;
    params.snowSize  = 0.12f;

    params.groundY = terrainBaseHeight;

    paramsBuffer->update(&params, sizeof(ParticleParams));

    // Transition buffer UAV → compute can write
    if (!firstFrame)
    {
        CD3DX12_RESOURCE_BARRIER toUAV = CD3DX12_RESOURCE_BARRIER::Transition(
            particleBuffer.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        );
        cmdList->ResourceBarrier(1, &toUAV);
    }
    firstFrame = false;

    // Dispatch compute
    cmdList->SetPipelineState(computePSO.Get());
    cmdList->SetComputeRootSignature(computeRootSig.Get());
    cmdList->SetComputeRootConstantBufferView(0, paramsBuffer->getGPUAddress());

    ID3D12DescriptorHeap* heaps[] = { descriptorHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetComputeRootDescriptorTable(1, uavHandle);

    // Each thread group handles 256 particles
    // PARTICLE_COUNT / 256 groups needed
    UINT groups = (PARTICLE_COUNT + 255) / 256;
    cmdList->Dispatch(groups, 1, 1);

    // Transition back to SRV so VS can read
    CD3DX12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        particleBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    );
    cmdList->ResourceBarrier(1, &toSRV);
}

void Particles::draw(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->SetPipelineState(renderPSO.Get());
    cmdList->SetGraphicsRootSignature(renderRootSig.Get());
    cmdList->SetGraphicsRootConstantBufferView(0, paramsBuffer->getGPUAddress());

    ID3D12DescriptorHeap* heaps[] = { descriptorHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 6 vertices per particle (2 triangles = 1 quad)
    cmdList->DrawInstanced(PARTICLE_COUNT * 6, 1, 0, 0);
}