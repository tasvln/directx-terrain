#pragma once

#include <DirectXMath.h>

#include <cstdint>

using namespace DirectX;

constexpr UINT MAX_LIGHTS = 16;

// future -> can put this in a settings file if i decide on a debug UI
struct WindowConfig {
    LPCWSTR appName;
    LPCWSTR windowClassName;
    uint32_t width;
    uint32_t height;
    bool enabledDirectX;
    bool useWarp;
    bool fullscreen = false;
    bool resizable = true;
};

struct alignas(16) VertexStruct {
    XMFLOAT4 position;
    XMFLOAT3 normal;
    XMFLOAT4 tangent;
    XMFLOAT2 texcoord;
};

struct alignas(256) MVPConstantStruct
{
    XMMATRIX model;
    XMMATRIX viewProj;
};

// Lighting Struct
enum class LightType : int {
    Directional = 0,
    Point = 1,
    Spot = 2
};

struct alignas(16) Light
{
    XMFLOAT4 position; // world-space position (for point/spot)
    XMFLOAT4 direction; // world-space direction (for directional/spot)
    XMFLOAT4 color; // RGB color
    float range = 0.0f; // how far point/spot light reaches
    float innerAngle = 0.0f; // spot light inner cone (radians)
    float outerAngle = 0.0f; // spot light outer cone (radians)
    float intensity = 1.0f; // brightness multiplier
    int type = 0; // 0=dir, 1=point, 2=spot
    float enabled = 0.0f; // 0 = off, 1 = on
    float pad[2] = {0,0}; // align to 16 bytes
};

struct alignas(16) LightBufferData
{
    XMFLOAT4 eyePosition;
    XMFLOAT4 globalAmbient;
    Light lights[MAX_LIGHTS];
    UINT numLights;
    float useBlinnPhong;
    float pad[2];
};

struct alignas(16) MaterialData
{
    XMFLOAT4 emissive; 
    XMFLOAT4 ambient; 
    XMFLOAT4 diffuse; 
    XMFLOAT4 specular; 
    float specularPower; 
    float useTexture; 
    float useNormalMap; 
    float useSpecularMap; 
    XMFLOAT2 padding; 
};

struct PlayerState
{
    XMFLOAT3 position;
    XMFLOAT3 velocity;

    float radius;   // collision radius
    float height;   // eye height
    bool grounded;
};
