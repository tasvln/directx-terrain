#pragma once

#include "utils/pch.h"

class CommandQueue;
class DescriptorHeap;
class Mesh;
class Pipeline;
class ConstantBuffer;

struct GridParams
{
    XMFLOAT3 cameraPos;
    float gridFadeDistance;
};

class Grid {
    public:
        Grid(
            ComPtr<ID3D12Device2> device,
            CommandQueue* commandQueue,
            DescriptorHeap* srvHeap
        );

        ~Grid() = default;

        void draw(ID3D12GraphicsCommandList* cmdList);

        void updateMVP(const XMMATRIX& viewProj);

        void updateGridParams(const XMFLOAT3& camPos, float fadeDistance = 300.0f);

        float sampleHeight(float x, float z) const;

    private:
        ComPtr<ID3D12Device2> device;
        std::unique_ptr<Mesh> mesh;
        std::unique_ptr<Pipeline> pipeline;
        std::unique_ptr<ConstantBuffer> mvpBuffer;
        std::unique_ptr<ConstantBuffer> gridParamsBuffer;
};