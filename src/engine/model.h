#pragma once

#include "utils/pch.h"

class Mesh;
class DescriptorHeap;
class Pipeline;
class ConstantBuffer;
class CommandQueue;
class Texture;
class Material;

class aiNode;
class aiScene;
class aiMesh;

class Model {
public:
    Model(
        ComPtr<ID3D12Device2> device,
        CommandQueue* uploadQueue,
        DescriptorHeap* srvHeap,
        const std::string& path
    );

    // Draw now handles everything internally
    void draw(
        ID3D12GraphicsCommandList* cmdList,
        const XMMATRIX& viewProj,
        D3D12_GPU_VIRTUAL_ADDRESS lightingCBV
    );

    void setTransform(const XMMATRIX& transform) { worldTransform = transform; }

    XMFLOAT3 getBoundingCenter() const { return boundingCenter; }
    float     getBoundingRadius() const { return boundingRadius; }

private:
    void loadModel(const std::string& path);
    void processNode(aiNode* node, const aiScene* scene);
    std::unique_ptr<Mesh> processMesh(aiMesh* mesh, const aiScene* scene);
    void buildPipeline();

    std::wstring resolveTexturePath(const std::string& relPath) const;
    std::shared_ptr<Texture> makeWhiteFallbackTexture();

private:
    ComPtr<ID3D12Device2> device;
    CommandQueue*         uploadQueue  = nullptr;
    DescriptorHeap*       srvHeap      = nullptr;

    ComPtr<ID3D12GraphicsCommandList2> uploadCmdList;
    std::string directory;

    std::vector<std::unique_ptr<Mesh>>     meshes;
    std::vector<std::shared_ptr<Material>> materials;
    std::vector<std::shared_ptr<Texture>>  textures;
    std::shared_ptr<Texture>               whiteTexture;

    // Pipeline owned by model
    std::unique_ptr<Pipeline>       pipeline;
    std::unique_ptr<ConstantBuffer> mvpBuffer;
    std::unique_ptr<ConstantBuffer> materialBuffer;

    // World transform set externally each frame
    XMMATRIX worldTransform = XMMatrixIdentity();

    XMFLOAT3 globalMin, globalMax;
    XMFLOAT3 boundingCenter;
    float     boundingRadius = 0.0f;
};