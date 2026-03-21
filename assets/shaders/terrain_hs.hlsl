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

// Input from VS
struct HSIn
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

// Output per control point
struct HSOut
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

// Output per patch — the tessellation amounts
struct PatchTess
{
    float edgeTess[4]   : SV_TessFactor;
    float insideTess[2] : SV_InsideTessFactor;
};

// How much to tessellate one edge
// Based on distance from camera to edge midpoint
float computeTess(float3 p0, float3 p1)
{
    float3 mid  = (p0 + p1) * 0.5f;
    float  dist = distance(mid, cameraPos);

    // t=1 when camera is right next to it, t=0 when far away
    float t = saturate(1.0f - (dist / tessDistance));

    return lerp(minTessFactor, maxTessFactor, t);
}

// Runs once per PATCH — decides tessellation for all 4 edges
PatchTess patchHSmain(InputPatch<HSIn, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;

    // Each edge gets its own tessellation factor
    // Edge 0 = left  (corners 0 and 2)
    // Edge 1 = bottom (corners 0 and 1)
    // Edge 2 = right  (corners 1 and 3)
    // Edge 3 = top    (corners 2 and 3)
    pt.edgeTess[0] = computeTess(patch[0].pos, patch[2].pos);
    pt.edgeTess[1] = computeTess(patch[0].pos, patch[1].pos);
    pt.edgeTess[2] = computeTess(patch[1].pos, patch[3].pos);
    pt.edgeTess[3] = computeTess(patch[2].pos, patch[3].pos);

    // Interior = average of opposite edges
    pt.insideTess[0] = (pt.edgeTess[1] + pt.edgeTess[3]) * 0.5f;
    pt.insideTess[1] = (pt.edgeTess[0] + pt.edgeTess[2]) * 0.5f;

    return pt;
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("patchHSmain")]
HSOut hsmain(InputPatch<HSIn, 4> patch, uint i : SV_OutputControlPointID)
{
    // Just pass each control point through unchanged
    // All the interesting work was in patchHSmain above
    HSOut output;
    output.pos = patch[i].pos;
    output.uv  = patch[i].uv;
    return output;
}