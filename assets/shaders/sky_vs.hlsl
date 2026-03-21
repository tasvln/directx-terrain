struct VSIn
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct VSOut
{
    float4 posH : SV_Position;
    float2 uv   : TEXCOORD;
};

VSOut vsmain(VSIn input)
{
    VSOut output;
    
    // Set W=1 and Z=1 so sky renders at maximum depth (far plane)
    // This means anything drawn before sky will occlude it

    output.posH = float4(input.pos, 1.0f, 1.0f);
    output.uv   = input.uv;
    return output;
}