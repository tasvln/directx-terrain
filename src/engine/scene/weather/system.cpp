#include "system.h"
#include <random>

static std::mt19937 rng(42);

WeatherSystem::WeatherSystem()
{
    buildPreset(WeatherType::Clear, current);
    buildPreset(WeatherType::Clear, from);
    buildPreset(WeatherType::Clear, target);
}

// Presets — define what each weather type looks like
// These are the TARGET values we lerp toward
void WeatherSystem::buildPreset(WeatherType type, WeatherState& out)
{
    out.type = type;

    switch (type)
    {
        case WeatherType::Clear:
            out.cloudCoverage     = 0.2f;
            out.cloudDarkness     = 0.0f;
            out.fogDensity        = 0.001f;
            out.fogHeightDensity  = 0.05f;
            out.visibility        = 1.0f;
            out.wind.strength     = 0.05f;
            out.wind.gustiness    = 0.02f;
            out.wind.direction    = { 0.7f, 0.3f };
            out.rainIntensity     = 0.0f;
            out.snowIntensity     = 0.0f;
            out.ambientMultiplier = 1.0f;
            out.sunMultiplier     = 1.0f;
            break;

        case WeatherType::Cloudy:
            out.cloudCoverage     = 0.5f;
            out.cloudDarkness     = 0.1f;
            out.fogDensity        = 0.002f;
            out.fogHeightDensity  = 0.08f;
            out.visibility        = 0.9f;
            out.wind.strength     = 0.15f;
            out.wind.gustiness    = 0.05f;
            out.wind.direction    = { 0.8f, 0.2f };
            out.rainIntensity     = 0.0f;
            out.snowIntensity     = 0.0f;
            out.ambientMultiplier = 0.9f;
            out.sunMultiplier     = 0.85f;
            break;

        case WeatherType::Overcast:
            out.cloudCoverage     = 0.85f;
            out.cloudDarkness     = 0.3f;
            out.fogDensity        = 0.003f;
            out.fogHeightDensity  = 0.12f;
            out.visibility        = 0.7f;
            out.wind.strength     = 0.3f;
            out.wind.gustiness    = 0.1f;
            out.wind.direction    = { 0.6f, 0.4f };
            out.rainIntensity     = 0.0f;
            out.snowIntensity     = 0.0f;
            out.ambientMultiplier = 0.75f;
            out.sunMultiplier     = 0.6f;
            break;

        case WeatherType::Rain:
            out.cloudCoverage     = 0.95f;
            out.cloudDarkness     = 0.5f;
            out.fogDensity        = 0.006f;
            out.fogHeightDensity  = 0.15f;
            out.visibility        = 0.5f;
            out.wind.strength     = 0.5f;
            out.wind.gustiness    = 0.15f;
            out.wind.direction    = { 0.5f, 0.5f };
            out.rainIntensity     = 0.6f;
            out.snowIntensity     = 0.0f;
            out.ambientMultiplier = 0.6f;
            out.sunMultiplier     = 0.4f;
            break;

        case WeatherType::Storm:
            out.cloudCoverage     = 1.0f;
            out.cloudDarkness     = 0.8f;
            out.fogDensity        = 0.01f;
            out.fogHeightDensity  = 0.2f;
            out.visibility        = 0.3f;
            out.wind.strength     = 0.9f;
            out.wind.gustiness    = 0.3f;
            out.wind.direction    = { 0.3f, 0.7f };
            out.rainIntensity     = 1.0f;
            out.snowIntensity     = 0.0f;
            out.ambientMultiplier = 0.4f;
            out.sunMultiplier     = 0.2f;
            break;

        case WeatherType::Snow:
            out.cloudCoverage     = 0.8f;
            out.cloudDarkness     = 0.2f;
            out.fogDensity        = 0.004f;
            out.fogHeightDensity  = 0.1f;
            out.visibility        = 0.6f;
            out.wind.strength     = 0.2f;
            out.wind.gustiness    = 0.08f;
            out.wind.direction    = { 0.4f, 0.6f };
            out.rainIntensity     = 0.0f;
            out.snowIntensity     = 0.5f;
            out.ambientMultiplier = 0.85f;
            out.sunMultiplier     = 0.7f;
            break;

        case WeatherType::Blizzard:
            out.cloudCoverage     = 1.0f;
            out.cloudDarkness     = 0.6f;
            out.fogDensity        = 0.02f;
            out.fogHeightDensity  = 0.25f;
            out.visibility        = 0.15f;
            out.wind.strength     = 1.0f;
            out.wind.gustiness    = 0.4f;
            out.wind.direction    = { 0.2f, 0.8f };
            out.rainIntensity     = 0.0f;
            out.snowIntensity     = 1.0f;
            out.ambientMultiplier = 0.5f;
            out.sunMultiplier     = 0.3f;
            break;
        }
    }

// Auto transition — picks next logical weather state
void WeatherSystem::pickNextWeather()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(rng);

    // Transition table — what weather leads to what
    switch (current.type)
    {
        case WeatherType::Clear:
            // Clear usually stays clear, sometimes gets cloudy
            setWeather(r < 0.6f ? WeatherType::Clear : WeatherType::Cloudy);
            break;

        case WeatherType::Cloudy:
            // Cloudy can clear up or get worse
            if      (r < 0.3f) setWeather(WeatherType::Clear);
            else if (r < 0.7f) setWeather(WeatherType::Cloudy);
            else               setWeather(WeatherType::Overcast);
            break;

        case WeatherType::Overcast:
            // Overcast usually leads to rain or clears
            if      (r < 0.2f) setWeather(WeatherType::Cloudy);
            else if (r < 0.6f) setWeather(WeatherType::Rain);
            else               setWeather(WeatherType::Overcast);
            break;

        case WeatherType::Rain:
            // Rain can intensify to storm or clear to overcast
            if      (r < 0.3f) setWeather(WeatherType::Overcast);
            else if (r < 0.6f) setWeather(WeatherType::Rain);
            else               setWeather(WeatherType::Storm);
            break;

        case WeatherType::Storm:
            // Storms always clear eventually
            if (r < 0.5f) setWeather(WeatherType::Rain);
            else          setWeather(WeatherType::Overcast);
            break;

        case WeatherType::Snow:
            if      (r < 0.4f) setWeather(WeatherType::Snow);
            else if (r < 0.7f) setWeather(WeatherType::Blizzard);
            else               setWeather(WeatherType::Overcast);
            break;

        case WeatherType::Blizzard:
            // Blizzards calm down to snow
            if (r < 0.7f) setWeather(WeatherType::Snow);
            else          setWeather(WeatherType::Overcast);
            break;
    }

    // Random duration for next state — 2 to 5 minutes
    std::uniform_real_distribution<float> durDist(120.0f, 300.0f);
    stateDuration = durDist(rng);
}

void WeatherSystem::update(float deltaTime)
{
    // Advance blend
    if (blendT < 1.0f)
    {
        blendT += deltaTime / blendDuration;
        blendT  = std::min(blendT, 1.0f);
    }

    // Advance state timer — auto transition when expired
    stateTimer += deltaTime;
    if (stateTimer >= stateDuration && !manualOverride)
    {
        stateTimer = 0.0f;
        pickNextWeather();
    }

    // Smooth ease — cubic in/out like RDR2 feels
    // Raw linear blendT → smooth t
    float t = blendT * blendT * (3.0f - 2.0f * blendT);

    lerpStates(from, target, t, current);

    // Wind gusting — independent fast fluctuation on top of base strength
    gustTimer += deltaTime;
    if (gustTimer > 0.5f)
    {
        gustTimer   = 0.0f;
        targetGust  = std::uniform_real_distribution<float>(-1.0f, 1.0f)(rng)
                      * current.wind.gustiness;
    }
    currentGust += (targetGust - currentGust) * deltaTime * 3.0f;
    current.wind.strength = std::clamp(
        current.wind.strength + currentGust,
        0.0f, 1.0f
    );
}

void WeatherSystem::setWeather(WeatherType type)
{
    from   = current;
    buildPreset(type, target);

    blendT        = 0.0f;
    stateTimer    = 0.0f;

    // Transition speed depends on severity change — RDR2 style
    // Calm→storm takes longer than storm→calm
    bool escalating = static_cast<int>(type) > static_cast<int>(current.type);
    blendDuration   = escalating ? 90.0f : 60.0f;

    manualOverride  = true; // disable auto-transition while user controls
}

// Lerp between two weather states
void WeatherSystem::lerpStates(
    const WeatherState& a,
    const WeatherState& b,
    float t,
    WeatherState& out)
{
    auto lf = [](float x, float y, float t) { return x + (y - x) * t; };

    out.type             = t < 0.5f ? a.type : b.type;
    out.cloudCoverage    = lf(a.cloudCoverage,    b.cloudCoverage,    t);
    out.cloudDarkness    = lf(a.cloudDarkness,    b.cloudDarkness,    t);
    out.fogDensity       = lf(a.fogDensity,       b.fogDensity,       t);
    out.fogHeightDensity = lf(a.fogHeightDensity, b.fogHeightDensity, t);
    out.visibility       = lf(a.visibility,       b.visibility,       t);
    out.rainIntensity    = lf(a.rainIntensity,    b.rainIntensity,    t);
    out.snowIntensity    = lf(a.snowIntensity,    b.snowIntensity,    t);
    out.ambientMultiplier= lf(a.ambientMultiplier,b.ambientMultiplier,t);
    out.sunMultiplier    = lf(a.sunMultiplier,    b.sunMultiplier,    t);

    // Wind
    out.wind.strength  = lf(a.wind.strength,  b.wind.strength,  t);
    out.wind.gustiness = lf(a.wind.gustiness, b.wind.gustiness, t);
    out.wind.direction = lerpDir(a.wind.direction, b.wind.direction, t);
}

XMFLOAT2 WeatherSystem::lerpDir(const XMFLOAT2& a, const XMFLOAT2& b, float t)
{
    // Lerp direction components and renormalize
    float x = a.x + (b.x - a.x) * t;
    float z = a.y + (b.y - a.y) * t;
    float len = std::sqrt(x * x + z * z);
    if (len > 0.0f) { x /= len; z /= len; }
    return { x, z };
}

std::wstring WeatherSystem::getWeatherString() const
{
    switch (current.type)
    {
        case WeatherType::Clear:    return L"Clear";
        case WeatherType::Cloudy:   return L"Cloudy";
        case WeatherType::Overcast: return L"Overcast";
        case WeatherType::Rain:     return L"Rain";
        case WeatherType::Storm:    return L"Storm";
        case WeatherType::Snow:     return L"Snow";
        case WeatherType::Blizzard: return L"Blizzard";
        default:                    return L"Unknown";
    }
}