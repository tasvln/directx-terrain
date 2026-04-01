cbuffer FogCB : register(b1)
{
    float3 fogColor;       
    float fogDensity;
    float heightDensity;  
    float fogStart;
    float fogEnd;         
    float pad;
};

cbuffer SkyParams : register(b0)
{
    float4x4 invViewProj;
    float3   sunDirection;  float pad0;
    float3   sunColor;      float sunIntensity;
    float3   ambientColor;  float pad1;
    float3   moonDirection; float moonIntensity;
    float    time;
    float    cloudCoverage;
    float    cloudSpeed;
    float    pad2;
};

float hash(float2 p)
{
    p = frac(p * float2(234.34f, 435.345f));
    p += dot(p, p + 34.23f);
    return frac(p.x * p.y);
}

float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(
        lerp(hash(i),               hash(i + float2(1,0)), u.x),
        lerp(hash(i + float2(0,1)), hash(i + float2(1,1)), u.x),
        u.y
    );
}

float fbm(float2 p)
{
    float value = 0.0f;
    float amp   = 0.5f;
    float freq  = 1.0f;
    for (int i = 0; i < 6; i++)
    {
        value += noise(p * freq) * amp;
        freq  *= 2.0f;
        amp   *= 0.5f;
    }
    return value;
}

// Mie glow — g controls tightness, higher = tighter
float mie(float cosAngle, float g)
{
    float denom = 1.0f + g * g - 2.0f * g * cosAngle;
    return (1.0f - g * g) / (4.0f * 3.14159f * pow(abs(denom), 1.5f));
}

struct PSIn
{
    float4 posH : SV_Position;
    float2 uv   : TEXCOORD;
};

float4 psmain(PSIn input) : SV_Target
{
    // --- Reconstruct view direction ---
    float2 clipPos = input.uv * 2.0f - 1.0f;
    clipPos.y = -clipPos.y;

    float4 nearH   = mul(float4(clipPos, 0.0f, 1.0f), invViewProj);
    float4 farH    = mul(float4(clipPos, 1.0f, 1.0f), invViewProj);
    float3 viewDir = normalize(farH.xyz / farH.w - nearH.xyz / nearH.w);

    // --- Base sky color ---
    // Day = blue, Night = deep dark blue
    // Blend purely based on sunIntensity, ignore the warm ambient color
    // that was bleeding red into everything
    float3 dayColor   = float3(0.25f, 0.52f, 0.95f);   // clear blue day
    float3 nightColor = float3(0.01f, 0.02f, 0.08f);   // deep dark blue night
    float3 skyColor   = lerp(nightColor, dayColor, sunIntensity);

    // Horizon is slightly lighter/hazier than overhead
    float horizonBlend = 1.0f - saturate(abs(viewDir.y) * 2.0f);
    float3 horizonDay   = float3(0.6f, 0.75f, 0.95f);  // hazy light blue
    float3 horizonNight = float3(0.02f, 0.04f, 0.12f); // dark horizon at night
    float3 horizonColor = lerp(horizonNight, horizonDay, sunIntensity);
    skyColor = lerp(skyColor, horizonColor, horizonBlend * 0.6f);

    // --- Sun glow ---
    // Wide outer glow + medium inner glow, no hard disk
    float cosAngleToSun = dot(viewDir, normalize(sunDirection));
    float sunWide   = mie(cosAngleToSun, 0.6f)  * sunIntensity * 0.06f;
    float sunTight  = mie(cosAngleToSun, 0.92f) * sunIntensity * 0.02f;
    skyColor += sunColor * (sunWide + sunTight);

    // Sunset/sunrise horizon tint when sun is near horizon
    // dot(sunDir, up) tells us how high the sun is
    float sunHeight   = dot(normalize(sunDirection), float3(0,1,0));
    float sunsetBlend = smoothstep(-0.1f, 0.3f, sunHeight) * (1.0f - smoothstep(0.3f, 0.8f, sunHeight));
    float3 sunsetColor = float3(1.0f, 0.4f, 0.1f); // orange-red
    skyColor = lerp(skyColor, sunsetColor * 0.5f + skyColor * 0.5f, sunsetBlend * 0.6f * sunIntensity);

    // --- Moon glow --- 
    // Exact same approach as sun — wide + tight Mie glow
    // nightBlend kills it completely during day
    float nightBlend     = saturate(1.0f - sunIntensity * 5.0f);
    float cosAngleToMoon = dot(viewDir, normalize(moonDirection));
    float moonWide  = mie(cosAngleToMoon, 0.6f)  * moonIntensity * 0.015f;
    float moonTight = mie(cosAngleToMoon, 0.92f) * moonIntensity * 0.005f;
    float3 moonColor = float3(0.85f, 0.92f, 1.0f); // cool blue-white
    skyColor += moonColor * (moonWide + moonTight) * nightBlend;

    // --- Stars ---
    float starVisibility = saturate(1.0f - sunIntensity * 15.0f);
    if (starVisibility > 0.0f && viewDir.y > 0.0f)
    {
        float2 starCoord      = viewDir.xz / (viewDir.y + 0.001f);
        float  starRandom     = hash(floor(starCoord * 300.0f));
        float  starExists     = step(0.997f, starRandom);
        float  starBrightness = pow(hash(starCoord + 0.5f), 2.0f);
        float  twinkle        = 0.85f + 0.15f * sin(time * 1.5f + starRandom * 300.0f);
        float  horizonFade    = saturate(viewDir.y * 15.0f);

        skyColor += float3(1.0f, 1.0f, 1.0f)
            * starExists * starBrightness * twinkle
            * starVisibility * horizonFade * 1.2f;
    }

    // --- Clouds ---
    if (viewDir.y > 0.01f)
    {
        float2 cloudUV = viewDir.xz / (viewDir.y + 0.1f);

        float cloud1 = fbm(cloudUV * 0.3f + time * cloudSpeed);
        float cloud2 = fbm(cloudUV * 0.8f + time * cloudSpeed * 1.5f);

        float cloudDensity = cloud1 * 0.7f + cloud2 * 0.3f;
        float cloudAmount  = smoothstep(
            1.0f - cloudCoverage,
            1.0f - cloudCoverage + 0.3f,
            cloudDensity
        );

        float3 cloudDay   = float3(0.9f, 0.92f, 0.95f);     // bright white day
        float3 cloudNight = float3(0.04f, 0.05f, 0.08f);    // very dark night
        float3 cloudColor = lerp(cloudNight, cloudDay, sunIntensity);
        cloudColor += moonColor * moonIntensity * nightBlend * 0.05f;

        float cloudFade = saturate(viewDir.y * 5.0f);
        skyColor = lerp(skyColor, cloudColor, cloudAmount * cloudFade);
    }

    float horizonHaze = saturate(1.0f - abs(viewDir.y) * 3.0f);
    skyColor = lerp(skyColor, fogColor, horizonHaze * fogDensity * 50.0f);

    return float4(saturate(skyColor), 1.0f);
}