#pragma once

#include "utils/pch.h"

enum class WeatherType
{
    Clear,
    Cloudy,
    Overcast,
    Rain,
    Storm,
    Snow,
    Blizzard
};

struct WindState
{
    XMFLOAT2 direction  = { 1.0f, 0.0f };  // normalized XZ
    float    strength   = 0.0f;            // 0=calm, 1=storm
    float    gustiness  = 0.0f;            // random variation per frame
};

struct WeatherState
{
    WeatherType type          = WeatherType::Clear;

    // Sky
    float cloudCoverage       = 0.0f;  // 0=clear, 1=overcast
    float cloudDarkness       = 0.0f;  // how dark/stormy clouds look

    // Atmosphere
    float fogDensity          = 0.002f;
    float fogHeightDensity    = 0.08f;
    float visibility          = 1.0f;  // multiplier on fog start distance

    // Wind
    WindState wind;

    // Precipitation
    float rainIntensity       = 0.0f;  // 0=none, 1=heavy
    float snowIntensity       = 0.0f;

    // Lighting mood
    float ambientMultiplier   = 1.0f;  // overcast = darker ambient
    float sunMultiplier       = 1.0f;  // storm = dimmer sun
};

class WeatherSystem
{
public:
    WeatherSystem();

    void update(float deltaTime);

    // Manual override — transitions smoothly
    void setWeather(WeatherType type);

    // Getters
    const WeatherState& getCurrent()  const { return current;  }
    const WeatherState& getTarget()   const { return target;   }
    WeatherType         getType()     const { return current.type; }
    float               getBlend()    const { return blendT;   }

    std::wstring getWeatherString() const;

private:
    void buildPreset(WeatherType type, WeatherState& out);
    void pickNextWeather();
    void lerpStates(const WeatherState& a, const WeatherState& b, float t, WeatherState& out);
    XMFLOAT2 lerpDir(const XMFLOAT2& a, const XMFLOAT2& b, float t);

private:
    WeatherState current;   // what's rendering right now
    WeatherState from;      // what we're blending from
    WeatherState target;    // what we're blending toward

    float blendT            = 1.0f;  // 0=from, 1=target
    float blendDuration     = 90.0f; // seconds to transition
    float stateTimer        = 0.0f;  // time in current state
    float stateDuration     = 120.0f;// how long before auto-transition

    bool  manualOverride    = false;

    // For wind gusting
    float gustTimer         = 0.0f;
    float currentGust       = 0.0f;
    float targetGust        = 0.0f;
};