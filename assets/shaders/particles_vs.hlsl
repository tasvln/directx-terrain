struct Particle
{
    float3 position; 
    float pad0;
    float3 velocity; 
    float life;
    float seed;
    float type;
    float pad2, pad3;
};

cbuffer ParticleParams : register(b0)
{
    float3 cameraPos;
    float deltaTime;

    float2 windDirection;
    float windStrength;

    float gravity;
    float rainIntensity;
    float snowIntensity;

    float spawnRadius;
    float spawnHeight;

    float rainSpeed;
    float rainStretch;
    float rainWidth;
    float rainTurbulence;

    float snowSpeed;
    float snowDrift;
    float snowSize;

    float groundY;
    float padding0;

    float4x4 viewProj;

    float3 camRight;
    float pad1;

    float3 camUp;
    float pad2;
};

StructuredBuffer<Particle> particles : register(t0);

struct VSOut
{
    float4 posH     : SV_Position;
    float2 uv       : TEXCOORD;
    float  life     : LIFE;
    float  isRain   : ISRAIN;
    float  intensity : INTENSITY;
};

float rand(float seed)
{
    return frac(sin(seed * 91.345f + 12.345f) * 47453.5453f);
}

VSOut vsmain(uint vertexID : SV_VertexID)
{
    // 6 vertices per particle — 2 triangles making a quad
    uint particleIdx = vertexID / 6;
    uint cornerIdx   = vertexID % 6;

    Particle p = particles[particleIdx];

    // Quad corner offsets for 2 triangles
    // Triangle 1: 0,1,2  Triangle 2: 0,2,3
    static const float2 corners[6] = {
        {-0.5f,  0.0f}, // top left
        { 0.5f,  0.0f}, // top right
        {-0.5f, -1.0f}, // bottom left
        { 0.5f,  0.0f}, // top right
        { 0.5f, -1.0f}, // bottom right
        {-0.5f, -1.0f}, // bottom left
    };

    static const float2 uvs[6] = {
        {0,0}, {1,0}, {0,1},
        {1,0}, {1,1}, {0,1}
    };

    bool isRain = (p.type == 0.0f);

    float size = isRain ? rainWidth : (snowSize + rand(p.seed) * 0.05f);

    float stretch = isRain ? rainStretch : 1.0f;

    float2 corner = corners[cornerIdx];

    float3 right = camRight;
    float3 up    = camUp;

    float3 offset;
    if (isRain)
    {
        float3 worldUp = float3(0, 1, 0);
        right = normalize(cross(worldUp, normalize(p.velocity)));
        up    = worldUp;
        offset = right * corner.x * size + up * corner.y * size * stretch;
    }
    else
    {
        // Snow — use camera-facing billboard, perfectly square
        offset = camRight * corner.x * size + camUp * corner.y * size;
    }
    
    float3 worldPos = p.position + offset;

    float intensity = isRain ? rainIntensity : snowIntensity;

    VSOut output;
    output.posH   = mul(float4(worldPos, 1.0f), viewProj);
    output.uv     = uvs[cornerIdx];
    output.life   = p.life;
    output.isRain = isRain ? 1.0f : 0.0f;
    output.intensity = intensity;

    return output;
}