#pragma once

#include "utils/pch.h"

class ConstantBuffer;
class CommandQueue;

struct ParticleData
{
    XMFLOAT3 position;  
    float pad0;
    XMFLOAT3 velocity;  
    float life;
    float seed;      
    float type;
    float pad2, pad3;
};

struct ParticleParams
{
    XMFLOAT3 cameraPos;     
    float deltaTime;

    XMFLOAT2 windDirection; 
    float windStrength;

    float gravity;
    float rainIntensity; 
    float snowIntensity;

    float spawnRadius;   
    float spawnHeight;

    // Rain
    float rainSpeed;
    float rainStretch;
    float rainWidth;
    float rainTurbulence;

    // Snow
    float snowSpeed;
    float snowDrift;
    float snowSize;

    // Ground
    float groundY;
    float padding0;

    XMMATRIX viewProj;

    XMFLOAT3 camRight;      
    float pad1;

    XMFLOAT3 camUp;         
    float pad2;
};

class Particles
{
public:
    Particles(ComPtr<ID3D12Device2> device, CommandQueue* commandQueue);
    ~Particles() = default;

    void update(
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
        float terrainBaseHeight = -4.0f
    );

    void draw(ID3D12GraphicsCommandList* cmdList);

private:
    void buildBuffers();
    void buildComputePipeline();
    void buildRenderPipeline();

private:
    ComPtr<ID3D12Device2> device;
    CommandQueue*         commandQueue = nullptr;
    bool firstFrame = true;

    static constexpr UINT PARTICLE_COUNT = 50000;

    // GPU particle buffer — written by CS, read by VS
    ComPtr<ID3D12Resource> particleBuffer;
    ComPtr<ID3D12Resource> particleUploadBuffer;

    // Descriptor heap for UAV/SRV
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    UINT descriptorSize = 0;

    // UAV — compute shader writes to this
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandle{};
    // SRV — vertex shader reads from this
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle{};

    // Constant buffer
    std::unique_ptr<ConstantBuffer> paramsBuffer;

    // Compute pipeline
    ComPtr<ID3D12PipelineState> computePSO;
    ComPtr<ID3D12RootSignature> computeRootSig;

    // Render pipeline
    ComPtr<ID3D12PipelineState> renderPSO;
    ComPtr<ID3D12RootSignature> renderRootSig;
};