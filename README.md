# DirectX 12 Weather & Terrain System

A real-time weather and environment rendering system built with **DirectX 12** and **C++**, featuring GPU-driven particles, dynamic sky rendering, and a modular weather pipeline.

<img width="1442" height="732" alt="Screenshot 2026-03-23 180041" src="https://github.com/user-attachments/assets/e91cea79-357c-4ad1-b5e0-94e0e1a898b9" />

<img width="1442" height="732" alt="Screenshot 2026-03-24 024151" src="https://github.com/user-attachments/assets/eafc2c34-467f-4ccc-a57b-46e9a0a03ca7" />



https://github.com/user-attachments/assets/31d457df-17b5-443d-840d-de06a43fa6d5



---

## Features

### Sky & Atmosphere

* Dynamic day/night cycle
* Procedural sky gradient (day → sunset → night)
* Sun & moon lighting with atmospheric scattering approximation
* Star field with twinkling effect
* Horizon-based fog blending

---

### Clouds

* Procedural clouds using FBM (fractal noise)
* Time-based movement (wind-driven)
* Adjustable cloud coverage and speed

---

### Weather System

* Multiple weather states:

  * Clear
  * Rain
  * Storm
  * Snow
  * Blizzard
* Smooth integration with lighting and fog
* Wind system affecting particles and clouds

---

### GPU Particle System

* Compute shader–driven particle simulation
* Supports **rain and snow (mutually exclusive)**
* ~50,000 particles simulated on GPU

#### Rain

* Fast-moving, stretched streaks
* Wind-influenced motion
* Camera-relative spawning

#### Snow

* Slow drifting particles
* Randomized spawn height and velocity
* De-synchronized movement to avoid pulsing

---

### Fog

* Distance-based fog
* Height-based attenuation
* Blended with sky color for atmospheric depth

---

### Terrain

* Heightmap-based terrain rendering
* Player movement constrained to terrain height
* Integrated with weather and fog systems

---

## System Architecture

### GPU Simulation Pipeline

1. **Compute Shader**

   * Updates particle position, velocity, and respawn
2. **Vertex Shader**

   * Expands particles into camera-facing quads
3. **Pixel Shader**

   * Renders rain streaks or snow sprites

---

### Weather Control

* Weather state is controlled on the CPU
* Particle type is **deterministic** (not random):

  * Rain → only rain particles
  * Snow → only snow particles

---

## ⚙️ Controls

| Key   | Action          |
| ----- | --------------- |
| F1–F4 | Set time of day |
| F5    | Pause time      |
| F6    | Clear weather   |
| F7    | Rain            |
| F8    | Storm           |
| F9    | Snow            |
| F10   | Blizzard        |

---

## Tunable Parameters

### Rain

* `rainSpeed` – fall speed
* `rainStretch` – streak length
* `rainWidth` – thickness
* `rainTurbulence` – wind variation

### Snow

* `snowSpeed` – fall speed
* `snowDrift` – horizontal movement
* `snowSize` – particle size

### Global

* `spawnRadius` – simulation area around camera
* `spawnHeight` – spawn altitude
* `windStrength` – global wind force

---

## Limitations

* Particles collide with a flat ground plane (`groundY`)
* No terrain-aware collision yet
* No accumulation (snow/puddles)
* Clouds are 2D procedural (not volumetric)

---

## 🛠️ Future Improvements

* Terrain-aware particle collision
* Rain splashes and ripple effects
* Wet surface shading (puddles, reflections)
* Volumetric clouds
* Wind gust simulation
* Snow accumulation system

---

## Tech Stack

* **C++**
* **DirectX 12**
* HLSL (Compute, Vertex, Pixel Shaders)

---

## Goal

This project focuses on building a **modular, real-time environment system** similar to those used in modern open-world games, with an emphasis on:

* GPU-driven simulation
* Real-time rendering techniques
* Scalable system design
