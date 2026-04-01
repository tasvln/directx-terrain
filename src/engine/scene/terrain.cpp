#include "terrain.h"
#include "engine/shader.h"
#include "engine/pipeline.h"

#include <random>
#include <numeric>
#include <fstream>

struct TerrainVertex
{
    XMFLOAT3 pos;
    XMFLOAT2 uv;
};

Terrain::Terrain(
    ComPtr<ID3D12Device2> device,
    CommandQueue* commandQueue,
    DescriptorHeap* srvHeap
) :
    device(device),
    commandQueue(commandQueue),
    srvHeap(srvHeap)
{
    heightData.resize(HEIGHTMAP_SIZE * HEIGHTMAP_SIZE, 0.0f);
    generateNoise();
    buildMesh();
    uploadHeightmap();
    buildPipeline();

    paramsBuffer = std::make_unique<ConstantBuffer>(
        device,
        static_cast<UINT>(sizeof(TerrainParams))
    );
}

// -------------------------------------------------------
// Perlin noise
// -------------------------------------------------------
void Terrain::generateNoise(int seed)
{
    std::vector<int> p(256);
    std::iota(p.begin(), p.end(), 0);
    std::shuffle(p.begin(), p.end(), std::mt19937(seed));
    for (int i = 0; i < 256; i++) perm[i] = perm[i + 256] = p[i];

    const int   octaves     = 6;
    const float frequency   = 3.0f;  // ← was 1.0f
    const float persistence = 0.5f;  // ← was 0.4f
    const float lacunarity  = 2.0f;

    for (int z = 0; z < HEIGHTMAP_SIZE; z++)
    {
        for (int x = 0; x < HEIGHTMAP_SIZE; x++)
        {
            float nx = static_cast<float>(x) / HEIGHTMAP_SIZE * frequency;
            float nz = static_cast<float>(z) / HEIGHTMAP_SIZE * frequency;

            float value  = 0.0f;
            float amp    = 1.0f;
            float freq   = 1.0f;
            float maxVal = 0.0f;

            for (int o = 0; o < octaves; o++)
            {
                value  += perlin(nx * freq, nz * freq) * amp;
                maxVal += amp;
                amp    *= persistence;
                freq   *= lacunarity;
            }

            value = value / maxVal;           // -0.5 to 0.5 range
            value = (value + 1.0f) * 0.5f;   // remap to 0-1
            value = std::clamp(value, 0.0f, 1.0f);

            heightData[z * HEIGHTMAP_SIZE + x] = value;
        }
    }

    LOG_INFO(L"[Terrain] Noise generated (seed=%d)", seed);
}

float Terrain::grad(int hash, float x, float z) const
{
    switch (hash & 3)
    {
        case 0:  return  x + z;
        case 1:  return -x + z;
        case 2:  return  x - z;
        default: return -x - z;
    }
}

float Terrain::perlin(float x, float z) const
{
    int X = static_cast<int>(std::floor(x)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    z -= std::floor(z);

    float u = fade(x);
    float v = fade(z);

    int a = perm[X]     + Z;
    int b = perm[X + 1] + Z;

    return lerp(
        lerp(grad(perm[a],     x,        z      ),
             grad(perm[b],     x - 1.0f, z      ), u),
        lerp(grad(perm[a + 1], x,        z - 1.0f),
             grad(perm[b + 1], x - 1.0f, z - 1.0f), u),
        v
    );
}

// -------------------------------------------------------
// Save / Load
// -------------------------------------------------------
void Terrain::saveHeightmap(const std::wstring& path) const
{
    std::ofstream f(path, std::ios::binary);
    if (!f) { LOG_ERROR(L"[Terrain] Failed to save heightmap"); return; }

    int size = HEIGHTMAP_SIZE;
    f.write(reinterpret_cast<const char*>(&size), sizeof(int));
    f.write(reinterpret_cast<const char*>(heightData.data()), heightData.size() * sizeof(float));

    LOG_INFO(L"[Terrain] Heightmap saved to %s", path.c_str());
}

void Terrain::loadHeightmap(const std::wstring& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { LOG_ERROR(L"[Terrain] Failed to load heightmap"); return; }

    int size = 0;
    f.read(reinterpret_cast<char*>(&size), sizeof(int));

    if (size != HEIGHTMAP_SIZE)
    {
        LOG_ERROR(L"[Terrain] Heightmap size mismatch: got %d expected %d", size, HEIGHTMAP_SIZE);
        return;
    }

    f.read(reinterpret_cast<char*>(heightData.data()), heightData.size() * sizeof(float));
    uploadHeightmap();

    LOG_INFO(L"[Terrain] Heightmap loaded from %s", path.c_str());
}

// -------------------------------------------------------
// CPU height sample
// -------------------------------------------------------
float Terrain::sampleHeight(float worldX, float worldZ) const
{
    float half = TERRAIN_SIZE * 0.5f;
    float u = (worldX + half) / TERRAIN_SIZE;
    float v = (worldZ + half) / TERRAIN_SIZE;

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float fx = u * (HEIGHTMAP_SIZE - 1);
    float fz = v * (HEIGHTMAP_SIZE - 1);

    int x0 = static_cast<int>(fx);
    int z0 = static_cast<int>(fz);
    int x1 = std::min(x0 + 1, HEIGHTMAP_SIZE - 1);
    int z1 = std::min(z0 + 1, HEIGHTMAP_SIZE - 1);

    float tx = fx - x0;
    float tz = fz - z0;

    float h00 = heightData[z0 * HEIGHTMAP_SIZE + x0];
    float h10 = heightData[z0 * HEIGHTMAP_SIZE + x1];
    float h01 = heightData[z1 * HEIGHTMAP_SIZE + x0];
    float h11 = heightData[z1 * HEIGHTMAP_SIZE + x1];

    float h = lerp(lerp(h00, h10, tx), lerp(h01, h11, tx), tz);
    return h * HEIGHT_SCALE - (HEIGHT_SCALE * 0.75f);
}

// -------------------------------------------------------
// Build mesh — raw D3D12 buffers (TerrainVertex != VertexStruct)
// so we can't use your VertexBuffer/IndexBuffer classes here
// -------------------------------------------------------
void Terrain::buildMesh()
{
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t>      indices;

    float half      = TERRAIN_SIZE * 0.5f;
    float patchStep = TERRAIN_SIZE / PATCH_SIZE;

    for (int z = 0; z <= PATCH_SIZE; z++)
    {
        for (int x = 0; x <= PATCH_SIZE; x++)
        {
            TerrainVertex v;
            v.pos = {
                -half + x * patchStep,
                0.0f,
                -half + z * patchStep
            };
            v.uv = {
                static_cast<float>(x) / PATCH_SIZE,
                static_cast<float>(z) / PATCH_SIZE
            };
            vertices.push_back(v);
        }
    }

    // 4 control points per patch (quads)
    for (int z = 0; z < PATCH_SIZE; z++)
    {
        for (int x = 0; x < PATCH_SIZE; x++)
        {
            uint32_t base = z * (PATCH_SIZE + 1) + x;
            indices.push_back(base);
            indices.push_back(base + (PATCH_SIZE + 1));
            indices.push_back(base + 1);
            indices.push_back(base + (PATCH_SIZE + 1) + 1);
        }
    }

    indexCount = static_cast<UINT>(indices.size());

    // --- Vertex buffer ---
    UINT vbSize = static_cast<UINT>(vertices.size() * sizeof(TerrainVertex));

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    throwFailed(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&vertexBuffer)
    ));

    void* mapped = nullptr;
    throwFailed(vertexBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, vertices.data(), vbSize);
    vertexBuffer->Unmap(0, nullptr);

    vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vbView.SizeInBytes    = vbSize;
    vbView.StrideInBytes  = sizeof(TerrainVertex);

    // --- Index buffer ---
    UINT ibSize = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

    throwFailed(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&indexBuffer)
    ));

    throwFailed(indexBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, indices.data(), ibSize);
    indexBuffer->Unmap(0, nullptr);

    ibView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    ibView.SizeInBytes    = ibSize;
    ibView.Format         = DXGI_FORMAT_R32_UINT;

    LOG_INFO(L"[Terrain] Mesh built: %u patches, %u indices", PATCH_SIZE * PATCH_SIZE, indexCount);
}

// -------------------------------------------------------
// Upload heightmap texture to GPU
// -------------------------------------------------------
void Terrain::uploadHeightmap()
{
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width            = HEIGHTMAP_SIZE;
    texDesc.Height           = HEIGHTMAP_SIZE;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels        = 1;
    texDesc.Format           = DXGI_FORMAT_R32_FLOAT;
    texDesc.SampleDesc       = { 1, 0 };
    texDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

    throwFailed(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&heightmapTexture)
    ));

    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadSize);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    throwFailed(device->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE,
        &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&heightmapUploadHeap)
    ));

    // Use CommandQueue to get a command list — same pattern as rest of your code
    auto cmdList = commandQueue->getCommandList();

    D3D12_SUBRESOURCE_DATA subData = {};
    subData.pData      = heightData.data();
    subData.RowPitch   = HEIGHTMAP_SIZE * sizeof(float);
    subData.SlicePitch = subData.RowPitch * HEIGHTMAP_SIZE;

    UpdateSubresources(
        cmdList.Get(),
        heightmapTexture.Get(),
        heightmapUploadHeap.Get(),
        0, 0, 1, &subData
    );

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        heightmapTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    );
    cmdList->ResourceBarrier(1, &barrier);

    UINT64 fence = commandQueue->executeCommandList(cmdList);
    commandQueue->fenceWait(fence);

    // Allocate SRV slot using your new allocate() method
    heightmapSRVIndex = srvHeap->allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                  = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels     = 1;

    // Use your getCPUHandle() method — matches your DescriptorHeap API
    device->CreateShaderResourceView(
        heightmapTexture.Get(),
        &srvDesc,
        srvHeap->getCPUHandle(heightmapSRVIndex)
    );

    LOG_INFO(L"[Terrain] Heightmap uploaded, SRV index=%u", heightmapSRVIndex);
}

// -------------------------------------------------------
// Build tessellation pipeline
// -------------------------------------------------------
void Terrain::buildPipeline()
{
    CD3DX12_ROOT_PARAMETER paramsParam;
    paramsParam.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_PARAMETER lightingParam;
    lightingParam.InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_DESCRIPTOR_RANGE heightmapRange;
    heightmapRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER heightmapParam;
    heightmapParam.InitAsDescriptorTable(1, &heightmapRange, D3D12_SHADER_VISIBILITY_ALL);
    
    CD3DX12_ROOT_PARAMETER fogParam;
    fogParam.InitAsConstantBufferView(3, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    std::vector<D3D12_ROOT_PARAMETER> rootParams = {
        paramsParam,    // slot 0 = b0 TerrainParams
        lightingParam,  // slot 1 = b1 LightCB
        fogParam,       // slot 2 = b3 FogCB
        heightmapParam  // slot 3 = t0 heightmap SRV
    };

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD           = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister   = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers{ sampler };

    std::vector<D3D12_INPUT_ELEMENT_DESC> layout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(XMFLOAT3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    Shader vs(L"assets/shaders/terrain_vs.cso");
    Shader hs(L"assets/shaders/terrain_hs.cso");
    Shader ds(L"assets/shaders/terrain_ds.cso");
    Shader ps(L"assets/shaders/terrain_ps.cso");

    D3D12_RASTERIZER_DESC rasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterDesc.FrontCounterClockwise = TRUE;

    // Uses your new 4-shader Pipeline constructor
    pipeline = std::make_unique<Pipeline>(
        device,
        vs, hs, ds, ps,
        layout,
        rootParams,
        samplers,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        &rasterDesc
    );
}
// -------------------------------------------------------
// Update + Draw
// -------------------------------------------------------
void Terrain::update(const XMMATRIX& viewProj, const XMFLOAT3& cameraPos)
{
    TerrainParams params{};
    params.viewProj      = XMMatrixTranspose(viewProj);
    params.cameraPos     = cameraPos;
    params.heightScale   = HEIGHT_SCALE;
    params.terrainSize   = TERRAIN_SIZE;
    params.minTessFactor = MIN_TESS;
    params.maxTessFactor = MAX_TESS;
    params.tessDistance  = TESS_DISTANCE;

    paramsBuffer->update(&params, sizeof(TerrainParams));
}

void Terrain::draw(
    ID3D12GraphicsCommandList* cmdList,
    D3D12_GPU_VIRTUAL_ADDRESS lightingCBV,
    D3D12_GPU_VIRTUAL_ADDRESS fogCBV 
) {
    cmdList->SetPipelineState(pipeline->getPipelineState().Get());
    cmdList->SetGraphicsRootSignature(pipeline->getRootSignature().Get());

    cmdList->SetGraphicsRootConstantBufferView(0, paramsBuffer->getGPUAddress());
    cmdList->SetGraphicsRootConstantBufferView(1, lightingCBV);
    cmdList->SetGraphicsRootConstantBufferView(2, fogCBV);

    ID3D12DescriptorHeap* heaps[] = { srvHeap->getHeap().Get() };
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetGraphicsRootDescriptorTable(3, srvHeap->getGPUHandle(heightmapSRVIndex)); // ← was 1

    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetIndexBuffer(&ibView);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    cmdList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}