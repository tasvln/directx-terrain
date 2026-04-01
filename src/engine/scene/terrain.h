#pragma once

#include "utils/pch.h"
#include "engine/resources/constant.h"
#include "engine/descriptor_heap.h"
#include "engine/command_queue.h"
#include <array>

class Pipeline;

struct TerrainParams
{
    XMMATRIX viewProj;
    XMFLOAT3 cameraPos;
    float     heightScale;
    float     terrainSize;
    float     minTessFactor;
    float     maxTessFactor;
    float     tessDistance;
};

class Terrain
{
public:
    Terrain(
        ComPtr<ID3D12Device2> device,
        CommandQueue* commandQueue,
        DescriptorHeap* srvHeap
    );
    ~Terrain() = default;

    void update(const XMMATRIX& viewProj, const XMFLOAT3& cameraPos);
    void draw(
        ID3D12GraphicsCommandList* cmdList,
        D3D12_GPU_VIRTUAL_ADDRESS lightingCBV,
        D3D12_GPU_VIRTUAL_ADDRESS fogCBV 
    );

    float sampleHeight(float x, float z) const;

    // Heightmap IO
    void generateNoise(int seed = 42);
    void saveHeightmap(const std::wstring& path) const;
    void loadHeightmap(const std::wstring& path);

private:
    void buildMesh();
    void buildPipeline();
    void uploadHeightmap();

    float sampleNoise(float x, float z) const;
    float perlin(float x, float z) const;
    float fade(float t) const { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    float lerp(float a, float b, float t) const { return a + t * (b - a); }
    float grad(int hash, float x, float z) const;

private:
    ComPtr<ID3D12Device2> device;
    CommandQueue* commandQueue = nullptr;
    DescriptorHeap* srvHeap   = nullptr;

    // Terrain config
    static constexpr int   HEIGHTMAP_SIZE = 512;
    static constexpr float TERRAIN_SIZE   = 200.0f; // world units
    static constexpr float HEIGHT_SCALE   = 8.0f;   // max height
    static constexpr int   PATCH_SIZE     = 32;      // quads per patch row/col
    static constexpr float MIN_TESS       = 1.0f;
    static constexpr float MAX_TESS       = 16.0f;
    static constexpr float TESS_DISTANCE  = 100.0f;

    // CPU heightmap data (normalized 0-1)
    std::vector<float> heightData; // HEIGHTMAP_SIZE * HEIGHTMAP_SIZE

    // Perlin permutation table
    std::array<int, 512> perm;

    // GPU resources
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbView{};
    D3D12_INDEX_BUFFER_VIEW  ibView{};
    UINT indexCount = 0;

    // Heightmap texture on GPU
    ComPtr<ID3D12Resource> heightmapTexture;
    ComPtr<ID3D12Resource> heightmapUploadHeap;
    UINT heightmapSRVIndex = 0;

    // Constant buffer
    std::unique_ptr<ConstantBuffer> paramsBuffer;

    // Pipeline
    std::unique_ptr<Pipeline> pipeline;
};