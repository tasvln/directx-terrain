struct VSIn
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct VSOut
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

// Just pass through — no transform here
// Domain shader does the final clip space transform
VSOut vsmain(VSIn input)
{
    VSOut output;
    output.pos = input.pos;
    output.uv  = input.uv;
    return output;
}