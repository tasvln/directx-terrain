#include "clock.h"

Clock::Clock(float startTime, float dayDurationSeconds) : dayDurationSeconds(dayDurationSeconds) {
    state.timeOfDay = startTime;
    computeState();
}

void Clock::update(float deltaTime)
{
    if (paused) return;

    // Advance time — one full day = dayDurationSeconds real seconds
    float advance = (deltaTime * timeScale) / dayDurationSeconds;
    state.timeOfDay = std::fmod(state.timeOfDay + advance, 1.0f);

    computeState();
}

void Clock::computeState()
{
    float t = state.timeOfDay;

    // -------------------------------------------------------
    // Sun direction — orbits around X axis
    // t=0.25 (sunrise) → sun on horizon east
    // t=0.5  (noon)    → sun directly overhead
    // t=0.75 (sunset)  → sun on horizon west
    // -------------------------------------------------------
    float sunAngle = (t - 0.25f) * XM_2PI; // 0 at sunrise, PI at sunset

    state.sunDirection = {
        cosf(sunAngle),                    // X — east/west
        sinf(sunAngle),                    // Y — up/down
        0.3f                               // Z — slight tilt for interest
    };

    // Normalize
    XMVECTOR dir = XMLoadFloat3(&state.sunDirection);
    XMStoreFloat3(&state.sunDirection, XMVector3Normalize(dir));

    // -------------------------------------------------------
    // Sun intensity — 0 at night, peaks at noon
    // -------------------------------------------------------
    state.sunIntensity  = sampleSunIntensity(t);
    state.moonIntensity = 1.0f - state.sunIntensity;
    state.isNight       = state.sunIntensity < 0.1f;

    // -------------------------------------------------------
    // Colors — interpolate through time of day palette
    // -------------------------------------------------------
    state.sunColor     = sampleSunColor(t);
    state.ambientColor = sampleSkyColor(t);

    // Fog matches ambient sky color but slightly desaturated
    state.fogColor = {
        state.ambientColor.x * 0.9f,
        state.ambientColor.y * 0.9f,
        state.ambientColor.z * 0.9f
    };
}

float Clock::sampleSunIntensity(float t) const
{
    // Sin curve — 0 at midnight (t=0), peaks at noon (t=0.5)
    // Shift by 0.25 so sunrise starts at t=0.2
    float intensity = sinf((t - 0.15f) * XM_PI / 0.7f);
    return std::clamp(intensity, 0.0f, 1.0f);
}

XMFLOAT3 Clock::sampleSunColor(float t) const
{
    // Key colors at different times
    // midnight = dark blue, sunrise = orange, noon = white, sunset = red/orange, midnight = dark
    struct ColorKey { float t; XMFLOAT3 color; };

    static const ColorKey keys[] = {
        { 0.00f, { 0.05f, 0.05f, 0.15f } }, // midnight — dark blue
        { 0.20f, { 1.00f, 0.50f, 0.20f } }, // sunrise  — orange
        { 0.30f, { 1.00f, 0.90f, 0.70f } }, // morning  — warm white
        { 0.50f, { 1.00f, 1.00f, 0.95f } }, // noon     — bright white
        { 0.70f, { 1.00f, 0.80f, 0.50f } }, // afternoon— warm
        { 0.80f, { 1.00f, 0.40f, 0.10f } }, // sunset   — deep orange/red
        { 0.90f, { 0.20f, 0.10f, 0.20f } }, // dusk     — purple
        { 1.00f, { 0.05f, 0.05f, 0.15f } }, // midnight — dark blue
    };

    // Find surrounding keys and lerp
    for (int i = 0; i < 7; i++)
    {
        if (t >= keys[i].t && t <= keys[i + 1].t)
        {
            float localT = (t - keys[i].t) / (keys[i + 1].t - keys[i].t);
            return lerpColor(keys[i].color, keys[i + 1].color, localT);
        }
    }

    return { 1.0f, 1.0f, 1.0f };
}

XMFLOAT3 Clock::sampleSkyColor(float t) const
{
    struct ColorKey { float t; XMFLOAT3 color; };

    static const ColorKey keys[] = {
        { 0.00f, { 0.02f, 0.02f, 0.08f } }, // midnight — near black
        { 0.20f, { 0.60f, 0.30f, 0.20f } }, // sunrise  — orange horizon
        { 0.30f, { 0.40f, 0.60f, 0.90f } }, // morning  — light blue
        { 0.50f, { 0.25f, 0.50f, 0.85f } }, // noon     — deep blue
        { 0.70f, { 0.40f, 0.55f, 0.85f } }, // afternoon
        { 0.80f, { 0.70f, 0.35f, 0.20f } }, // sunset   — orange/red
        { 0.90f, { 0.10f, 0.05f, 0.15f } }, // dusk     — dark purple
        { 1.00f, { 0.02f, 0.02f, 0.08f } }, // midnight
    };

    for (int i = 0; i < 7; i++)
    {
        if (t >= keys[i].t && t <= keys[i + 1].t)
        {
            float localT = (t - keys[i].t) / (keys[i + 1].t - keys[i].t);
            return lerpColor(keys[i].color, keys[i + 1].color, localT);
        }
    }

    return { 0.25f, 0.50f, 0.85f };
}

XMFLOAT3 Clock::lerpColor(const XMFLOAT3& a, const XMFLOAT3& b, float t) const
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

std::wstring Clock::getTimeString() const
{
    // Convert 0-1 to HH:MM
    float totalMinutes = state.timeOfDay * 24.0f * 60.0f;
    int hours   = static_cast<int>(totalMinutes / 60.0f) % 24;
    int minutes = static_cast<int>(totalMinutes) % 60;

    wchar_t buf[16];
    swprintf_s(buf, L"%02d:%02d", hours, minutes);
    return std::wstring(buf);
}