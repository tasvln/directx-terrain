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

RWStructuredBuffer<Particle> particles : register(u0);

float rand(float seed)
{
    return frac(sin(seed * 127.1f + 311.7f) * 43758.5453f);
}

float3 randomSpawnPos(float seed, float3 camPos)
{
    // Use multiple different seed offsets to break up patterns
    float angle  = rand(seed * 3.7f + 1.3f) * 6.28318f;
    float radius = sqrt(rand(seed * 7.1f + 2.9f)) * spawnRadius;

    // Add time-based variation so respawned particles don't land
    // in the same spot as their previous life
    float jitterX = (rand(seed * 13.3f + deltaTime * 0.1f) - 0.5f) * spawnRadius * 0.5f;
    float jitterZ = (rand(seed * 17.7f + deltaTime * 0.2f) - 0.5f) * spawnRadius * 0.5f;

    float x = cos(angle) * radius + jitterX;
    float z = sin(angle) * radius + jitterZ;
    float y = spawnHeight + rand(seed * 5.3f + 0.2f) * 15.0f;

    return float3(camPos.x + x, camPos.y + y, camPos.z + z);
}

void setVelocity(inout Particle p, bool isRain)
{
    float turbulence = (rand(p.seed + deltaTime) - 0.5f) * 2.0f;

    if (isRain)
    {
        p.velocity = float3(
            windDirection.x * windStrength * rainTurbulence + turbulence,
            -rainSpeed - rand(p.seed) * rainSpeed * 0.5f,
            windDirection.y * windStrength * rainTurbulence + turbulence
        );
    }
    else
    {
        p.velocity = float3(
            windDirection.x * windStrength * snowDrift * 0.5f,
            -snowSpeed - rand(p.seed) * 1.5f,
            windDirection.y * windStrength * snowDrift * 0.5f
        );
    }
}

[numthreads(256,1,1)]
void csmain(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;
    if (idx >= 50000) return;

    Particle p = particles[idx];

    bool  isRain       = (rainIntensity >= snowIntensity);
    float expectedType = isRain ? 0.0f : 1.0f;

    // Type changed mid-life — update immediately
    if (p.type != expectedType)
    {
        p.type = expectedType;
        setVelocity(p, isRain);
    }

    // Ground collision
    if (p.position.y <= groundY)
        p.life = 0.0f;

    // Respawn
    if (p.life <= 0.0f)
    {
        p.seed     = rand(p.seed + deltaTime + float(idx) * 0.001f);
        p.position = randomSpawnPos(p.seed, cameraPos);
        p.life     = rand(p.seed * 2.3f) * 0.3f + 0.7f;
        p.type     = expectedType;
        setVelocity(p, isRain);
    }

    p.position += p.velocity * deltaTime;
    particles[idx] = p;
}