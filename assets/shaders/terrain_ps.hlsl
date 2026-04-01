#include "lighting.hlsl"
#include "fog.hlsl"

cbuffer TerrainParams : register(b0)
{
    float4x4 viewProj;
    float3   cameraPos;
    float    heightScale;
    float    terrainSize;
    float    minTessFactor;
    float    maxTessFactor;
    float    tessDistance;
};

// Same lighting buffer your model uses
cbuffer LightCB : register(b1)
{
    float4 eyePosition;
    float4 globalAmbient;
    Light  lights[MAX_LIGHTS];
    uint   numLights;
    float  useBlinnPhong;
    float  padding[2];
};

struct PSIn
{
    float4 posH : SV_Position;
    float3 posW : POSITION;
    float2 uv   : TEXCOORD;
    float3 nor  : NORMAL;
};

float4 psmain(PSIn input) : SV_Target
{
    // --- Height based colour ---
    float3 dirtColor  = float3(0.35f, 0.25f, 0.15f);
    float3 grassColor = float3(0.45f, 0.65f, 0.30f);
    float3 rockColor  = float3(0.45f, 0.42f, 0.38f);

    float heightT = saturate(input.posW.y / 8.0f + 0.5f);
    float3 color  = lerp(dirtColor, grassColor, heightT);

    // --- Slope based colour ---
    float slopeFactor = saturate(1.0f - input.nor.y);
    color = lerp(color, rockColor, smoothstep(0.3f, 0.7f, slopeFactor));

    // --- Lighting from shared buffer ---
    float3 ambient = globalAmbient.rgb * color * 3.0f;
    float3 diffuse = float3(0, 0, 0);

    for (uint i = 0; i < numLights; i++)
    {
        if (lights[i].enabled < 0.5f) continue;

        float3 L;

        if (lights[i].type == 0) // Directional
        {
            L = normalize(-lights[i].direction.xyz);
        }
        else // Point
        {
            L = normalize(lights[i].position.xyz - input.posW);
        }

        float ndotl = saturate(dot(input.nor, L));
        diffuse += color * lights[i].color.rgb * ndotl * lights[i].intensity * 2.0f;
    }

    // Night — dim blue moonlight so terrain isn't pitch black
    float sunIntensity = saturate(lights[0].direction.y * -1.0f);
    float3 moonLight   = color * float3(0.05f, 0.07f, 0.15f) * (1.0f - sunIntensity);

    float4 finalColor = float4(saturate(ambient + diffuse + moonLight), 1.0f);

    float fogFactor = computeFog(input.posW, cameraPos);
    float3 fogged   = lerp(finalColor.rgb, fogColor, fogFactor);

    return float4(fogged, 1.0f);;
}