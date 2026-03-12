#pragma once

#include "model.h"
#include "mesh.h"
#include "descriptor_heap.h"
#include "command_queue.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace fs = std::filesystem;

Model::Model(
    ComPtr<ID3D12Device2> device, 
    CommandQueue* uploadQueue, 
    DescriptorHeap* srvHeap, 
    const std::string& path
) :
    device(device), 
    uploadQueue(uploadQueue), 
    srvHeap(srvHeap)
{
    // store model folder for resolving relative texture paths
    try {
        directory = fs::path(path).parent_path().string();
    } catch (...) {
        directory.clear();
    }

    // at top of loadModel() after you have validated `uploadQueue`
    if (uploadQueue) {
        uploadCmdList = uploadQueue->getCommandList();
    }

    loadModel(path);
}

void Model::loadModel(const std::string& path) {
    LOG_INFO(L"Model -> Loading from path: %hs", path.c_str());

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_CalcTangentSpace |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR(L"Model -> Assimp failed: %hs", importer.GetErrorString());
        throw std::runtime_error("Assimp failed to load model");
    }

    // Reset bounds
    globalMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    globalMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    processNode(scene->mRootNode, scene);

    if (uploadCmdList && uploadQueue) {
        UINT64 fence = uploadQueue->executeCommandList(uploadCmdList);

        uploadQueue->fenceWait(fence);

        uploadCmdList.Reset();
    }

    // Compute global bounding sphere
    XMFLOAT3 extent = {
        (globalMax.x - globalMin.x) * 0.5f,
        (globalMax.y - globalMin.y) * 0.5f,
        (globalMax.z - globalMin.z) * 0.5f
    };

    boundingCenter = {
        (globalMin.x + globalMax.x) * 0.5f,
        (globalMin.y + globalMax.y) * 0.5f,
        (globalMin.z + globalMax.z) * 0.5f
    };

    boundingRadius = std::sqrt(
        extent.x * extent.x +
        extent.y * extent.y +
        extent.z * extent.z
    );
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    // Process all the meshes at this node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }

    // Then process all children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

std::unique_ptr<Mesh> Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    LOG_INFO(L"[Model] Processing mesh: %hs, Vertices: %u, Faces: %u",
        mesh->mName.C_Str(),
        mesh->mNumVertices,
        mesh->mNumFaces
    );

    std::vector<VertexStruct> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3);

    XMFLOAT3 minPos = { FLT_MAX,  FLT_MAX,  FLT_MAX };
    XMFLOAT3 maxPos = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        VertexStruct vertex{};
        
        // Position
        vertex.position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z,
            1.0f
        };

        // Normal
        if (mesh->HasNormals()) {
            vertex.normal = {
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            };
        }

        // Tangent + Handedness
        if (mesh->HasTangentsAndBitangents() && mesh->HasNormals()) {
            // Load Assimp vectors directly into XMVECTOR
            XMVECTOR tVec = XMLoadFloat3(
                reinterpret_cast<XMFLOAT3*>(&mesh->mTangents[i])
            );
            XMVECTOR bVec = XMLoadFloat3(
                reinterpret_cast<XMFLOAT3*>(&mesh->mBitangents[i])
            );
            XMVECTOR nVec = XMLoadFloat3(
                reinterpret_cast<XMFLOAT3*>(&mesh->mNormals[i])
            );

            // Optional: normalize to be safe
            tVec = XMVector3Normalize(tVec);
            bVec = XMVector3Normalize(bVec);
            nVec = XMVector3Normalize(nVec);

            // Compute handedness
            float handedness = (XMVectorGetX(XMVector3Dot(XMVector3Cross(nVec, tVec), bVec)) < 0.0f) ? -1.0f : 1.0f;

            vertex.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z, handedness };
        } else {
            vertex.tangent = { 1.0f, 0.0f, 0.0f, 1.0f }; // fallback
        }

        // UVs
        if (mesh->HasTextureCoords(0)) {
            vertex.texcoord = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            };
        } else {
            vertex.texcoord = { 0.0f, 0.0f };
        }

        vertices.push_back(vertex);

        // Track mesh bounds
        minPos.x = std::min(minPos.x, mesh->mVertices[i].x);
        minPos.y = std::min(minPos.y, mesh->mVertices[i].y);
        minPos.z = std::min(minPos.z, mesh->mVertices[i].z);
        maxPos.x = std::max(maxPos.x, mesh->mVertices[i].x);
        maxPos.y = std::max(maxPos.y, mesh->mVertices[i].y);
        maxPos.z = std::max(maxPos.z, mesh->mVertices[i].z);
    }


    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) indices.push_back(face.mIndices[j]);
    }

    globalMin = { std::min(globalMin.x, minPos.x), std::min(globalMin.y, minPos.y), std::min(globalMin.z, minPos.z) };
    globalMax = { std::max(globalMax.x, maxPos.x), std::max(globalMax.y, maxPos.y), std::max(globalMax.z, maxPos.z) };

    // --- Material & Texture Handling ---
    std::shared_ptr<Texture> texForMesh = nullptr;

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* aimat = scene->mMaterials[mesh->mMaterialIndex];
        aiString texPath;
        if ((aimat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
             aimat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) ||
            (aimat->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
             aimat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)) {

            std::string texStr(texPath.C_Str());
            if (!texStr.empty()) {
                std::wstring wpath = resolveTexturePath(texStr);

                // Only create one shared_ptr per texture; store in textures vector
                auto texShared = std::make_shared<Texture>(device, uploadCmdList, srvHeap, wpath, nextDescriptorIndex++);
                textures.push_back(texShared);
                texForMesh = texShared;
            }
        }
    }

    // Fallback texture
    if (!texForMesh) {
        if (!whiteTexture) {
            LOG_INFO(L"[Model] Creating white fallback texture for mesh");
            whiteTexture = makeWhiteFallbackTexture();
        }
        texForMesh = whiteTexture;
    }

    auto matPtr = std::make_shared<Material>(texForMesh);
    materials.push_back(matPtr);

    return std::make_unique<Mesh>(device, vertices, indices, matPtr);
}

void Model::draw(ID3D12GraphicsCommandList* cmdList, ID3D12DescriptorHeap* srvHeap, UINT rootIndex) {

    ID3D12DescriptorHeap* heaps[] = { srvHeap };
    cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

    for (size_t i = 0; i < meshes.size(); ++i) {
        LOG_D3D12_MESSAGES(device);
        meshes[i]->draw(cmdList, rootIndex);
        LOG_D3D12_MESSAGES(device);
    }

    LOG_INFO(L"[Model] draw() completed for %zu meshes", meshes.size());
}

std::wstring Model::resolveTexturePath(const std::string& texRel) const {
    fs::path p(texRel);
    if (p.is_absolute()) 
        return p.wstring();

    return (fs::path(directory) / p).wstring();
}

std::shared_ptr<Texture> Model::makeWhiteFallbackTexture() {
    std::wstring whitePath = DEFAULT_WHITE_TEXTURE;
    auto tex = std::make_shared<Texture>(device, uploadCmdList, srvHeap, whitePath, nextDescriptorIndex++);
    textures.push_back(tex);
    return tex;
}
