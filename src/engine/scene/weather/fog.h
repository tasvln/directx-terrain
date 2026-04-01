#pragma once

#include "utils/pch.h"

class ConstantBuffer; // forward declare

struct FogData
{
    XMFLOAT3 fogColor;      float fogDensity;
    float    heightDensity; float fogStart;
    float    fogEnd;        float pad;
};

class Fog
{
public:
    Fog(ComPtr<ID3D12Device2> device);
    ~Fog();

    void update(
        const XMFLOAT3& fogColor,
        float sunIntensity,
        float density     = 0.002f,
        float heightDens  = 0.05f,
        float fogStart    = 100.0f,
        float fogEnd      = 500.0f
    );

    D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress() const; // ← declaration only
    const FogData& getData() const { return data; }

private:
    FogData data{};
    std::unique_ptr<ConstantBuffer> fogCBV;
};