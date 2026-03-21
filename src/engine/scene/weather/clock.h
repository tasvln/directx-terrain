#pragma once

#include "utils/pch.h"

struct TimeOfDayState
{
    float        timeOfDay;      // 0.0 - 1.0 (0=midnight, 0.25=sunrise, 0.5=noon, 0.75=sunset)
    XMFLOAT3     sunDirection;   // normalized direction toward sun
    XMFLOAT3     sunColor;       // color of sunlight
    XMFLOAT3     ambientColor;   // sky ambient
    XMFLOAT3     fogColor;       // fog matches sky
    float        sunIntensity;   // 0 at night, 1 at noon
    float        moonIntensity;  // opposite of sun
    bool         isNight;
};


class Clock
{
public:
    Clock(float startTime = 0.25f, float dayDurationSeconds = 300.0f);
    // ~Clock();

    void update(float deltaTime);

    // Getters
    const TimeOfDayState& getState() const { return state; }
    float getTimeOfDay()             const { return state.timeOfDay; }
    float getDayDuration()           const { return dayDurationSeconds; }
    bool  isNight()                  const { return state.isNight; }

    // Manual control
    void setTimeOfDay(float t)            { state.timeOfDay = std::fmod(t, 1.0f); }
    void setDayDuration(float seconds)    { dayDurationSeconds = seconds; }
    void setTimeScale(float scale)        { timeScale = scale; }
    void setPaused(bool paused)           { this->paused = paused; }

    // Helpers for UI/debug
    std::wstring getTimeString() const;   // returns "06:30" style string

    bool reset() const;

private:
    void computeState();

    XMFLOAT3 lerpColor(const XMFLOAT3& a, const XMFLOAT3& b, float t) const;
    XMFLOAT3 sampleSkyColor(float t) const;
    XMFLOAT3 sampleSunColor(float t) const;
    float    sampleSunIntensity(float t) const;

private:
    TimeOfDayState state;

    float dayDurationSeconds = 300.0f; // 5 min default
    float timeScale          = 1.0f;   // multiplier on top of duration
    bool  paused             = false;
};
