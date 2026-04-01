struct PSIn
{
    float4 posH      : SV_Position;
    float2 uv        : TEXCOORD;
    float  life      : LIFE;
    float  isRain    : ISRAIN;
    float  intensity : INTENSITY;
};

float4 psmain(PSIn input) : SV_Target
{
    float alpha = 0.0f;
    float3 color = float3(1.0f, 1.0f, 1.0f);

    if (input.isRain > 0.5f)
    {
        float fadeTop    = smoothstep(0.0f, 0.2f, input.uv.y);
        float fadeBottom = smoothstep(1.0f, 0.8f, input.uv.y);
        float fadeEdge   = saturate(1.0f - abs(input.uv.x - 0.5f) * 6.0f);

        alpha = fadeTop * fadeBottom * fadeEdge * 0.6f;
        color = float3(0.7f, 0.8f, 1.0f);
    }
    else
    {
        float2 center = input.uv - 0.5f;
        float dist    = length(center) * 2.0f;
        alpha = (1.0f - smoothstep(0.5f, 1.0f, dist)) * 0.9f;
        color = float3(0.95f, 0.97f, 1.0f);
    }

    // life = particle age fade
    // intensity = weather blend fade (smooth crossover between rain/snow)
    alpha *= input.life * input.intensity;

    return float4(color, alpha);
}