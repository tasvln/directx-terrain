#pragma once

#include "utils/pch.h"

class Pipeline;
class ConstantBuffer;
class DescriptorHeap;
struct TimeOfDayState;

// Sent to sky shaders every frame
struct SkyParams
{
    XMMATRIX invViewProj;    // needed to reconstruct view direction from screen pos
    XMFLOAT3 sunDirection;   
    float pad0;
    XMFLOAT3 sunColor;       
    float sunIntensity;
    XMFLOAT3 ambientColor;   
    float pad1;
    XMFLOAT3 moonDirection;  
    float moonIntensity;
    float    time;           // total elapsed time — drives cloud scrolling
    float    cloudCoverage;  // 0=clear, 1=overcast
    float    cloudSpeed;     // how fast clouds scroll
    float    pad2;
};

class Sky
{
public:
    Sky(ComPtr<ID3D12Device2> device, DXGI_FORMAT rtvFormat);
    ~Sky() = default;

    void update(const TimeOfDayState& tod, const XMMATRIX& view, const XMMATRIX& proj, float totalTime, float cloudCoverage = 0.8f);
    void draw(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS fogCBV);

private:
    void buildPipeline(DXGI_FORMAT rtvFormat);
    void buildFullscreenQuad();

private:
    ComPtr<ID3D12Device2> device;

    std::unique_ptr<Pipeline>       pipeline;
    std::unique_ptr<ConstantBuffer> paramsBuffer;

    // Fullscreen quad — 4 vertices, no index buffer needed
    ComPtr<ID3D12Resource>   vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vbView{};
};