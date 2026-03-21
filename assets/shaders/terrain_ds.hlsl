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

Texture2D<float> heightmap : register(t0);
SamplerState     samp      : register(s0);

struct DSIn
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct DSOut
{
    float4 posH : SV_Position; // clip space (required output)
    float3 posW : POSITION;    // world space (for lighting in PS)
    float2 uv   : TEXCOORD;
    float3 nor  : NORMAL;
};

struct PatchTess
{
    float edgeTess[4]   : SV_TessFactor;
    float insideTess[2] : SV_InsideTessFactor;
};

[domain("quad")]
DSOut dsmain(
    PatchTess patchTess,
    float2 domainUV : SV_DomainLocation,        // position within patch (0,0)-(1,1)
    const OutputPatch<DSIn, 4> patch
)
{
    DSOut output;

    // --- Step 1: Interpolate position across the quad patch ---
    // domainUV.x goes left to right across the patch
    // domainUV.y goes bottom to top across the patch
    float3 p0  = lerp(patch[0].pos, patch[1].pos, domainUV.x); // bottom edge
    float3 p1  = lerp(patch[2].pos, patch[3].pos, domainUV.x); // top edge
    float3 pos = lerp(p0, p1, domainUV.y);                     // blend between edges

    // Interpolate UV the same way
    float2 t0    = lerp(patch[0].uv, patch[1].uv, domainUV.x);
    float2 t1    = lerp(patch[2].uv, patch[3].uv, domainUV.x);
    float2 texUV = lerp(t0, t1, domainUV.y);

    // --- Step 2: Sample heightmap and push vertex up ---
    float h = heightmap.SampleLevel(samp, texUV, 0); // 0.0 to 1.0
    pos.y   = h * heightScale - (heightScale * 0.75f);                        // scale to world height

    // --- Step 3: Compute normal via finite differences ---
    // Sample 4 neighbouring heights to figure out slope direction
    float texelSize = 1.0f / 512.0f; // one pixel in UV space

    float hL = heightmap.SampleLevel(samp, texUV + float2(-texelSize, 0), 0) * heightScale;
    float hR = heightmap.SampleLevel(samp, texUV + float2( texelSize, 0), 0) * heightScale;
    float hD = heightmap.SampleLevel(samp, texUV + float2(0, -texelSize), 0) * heightScale;
    float hU = heightmap.SampleLevel(samp, texUV + float2(0,  texelSize), 0) * heightScale;

    // Cross product of XZ slope gives surface normal
    // 2.0 in Y controls how "sharp" the normal transitions are
    float3 nor = normalize(float3(hL - hR, 2.0f, hD - hU));

    // --- Step 4: Transform to clip space for rasterizer ---
    output.posW = pos;
    output.posH = mul(float4(pos, 1.0f), viewProj);
    output.uv   = texUV;
    output.nor  = nor;

    return output;
}