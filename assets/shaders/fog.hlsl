#ifndef FOG_HLSL
#define FOG_HLSL

cbuffer FogCB : register(b3)
{
    float3 fogColor;       
    float fogDensity;
    float heightDensity;  
    float fogStart;
    float fogEnd;         
    float pad;
};


// Exponential distance fog -> density controls how thick the fog is overall
float distanceFog(float3 worldPos, float3 cameraPos)
{
    float dist = length(worldPos - cameraPos);
    return 1.0f - exp(-fogDensity * max(0.0f, dist - fogStart));
}

// Height fog — denser near ground, thins out at altitude -> heightDensity controls how quickly it thins with height
float heightFog(float worldY)
{
    return exp(-heightDensity * max(0.0f, worldY));
}

// Combined fog factor — clamp to 0-1
float computeFog(float3 worldPos, float3 cameraPos)
{
    float df = distanceFog(worldPos, cameraPos);
    float hf = heightFog(worldPos.y);

    return saturate(df * hf + hf * 0.3f);
    // The 0.3 blend ensures low areas always have some ground fog
    // even when the camera is close
}

#endif