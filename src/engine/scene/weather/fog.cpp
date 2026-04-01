#include "fog.h"
#include "engine/resources/constant.h" // full type here

Fog::Fog(ComPtr<ID3D12Device2> device)
{
    fogCBV = std::make_unique<ConstantBuffer>(
        device,
        static_cast<UINT>(sizeof(FogData))
    );
}

Fog::~Fog() = default; // unique_ptr needs full type here to destruct

D3D12_GPU_VIRTUAL_ADDRESS Fog::getGPUAddress() const
{
    return fogCBV->getGPUAddress(); // full type visible now
}

void Fog::update(
    const XMFLOAT3& fogColor,
    float sunIntensity,
    float density,
    float heightDens,
    float fogStart,
    float fogEnd
)
{
    float nightMultiplier = 1.0f + (1.0f - sunIntensity) * 1.5f;

    data.fogColor      = fogColor;
    data.fogDensity    = density * nightMultiplier;
    data.heightDensity = heightDens;
    data.fogStart      = fogStart;
    data.fogEnd        = fogEnd;

    fogCBV->update(&data, sizeof(FogData));
}