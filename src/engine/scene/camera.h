#pragma once

#include "utils/pch.h"

enum class CameraMode
{
    Orbit,
    FPS,
    ThirdPerson
};

class Camera {
public:
    Camera(
        float fov, 
        float aspect, 
        float nearZ, 
        float farZ
    );
    ~Camera() = default;

    void update(float delta);

    // Mode
    void setMode(CameraMode newMode) { mode = newMode; }
    CameraMode getMode() const { return mode; }

    // Orbit
    void orbit(float deltaYaw, float deltaPitch);
    void zoom(float wheelDelta);
    void pan(float deltaX, float deltaY);
    void frameModel(const XMFLOAT3& center, float radius);

    // FPS
    void setFPS(const XMFLOAT3& pos, float yaw, float pitch);
    void updateFPSView();

    // Third Person
    void setThirdPerson(float distance = 5.0f, float height = 1.8f, float pitch = 0.3f);
    void followPlayer(const XMFLOAT3& playerPos, float yaw, float pitch);

    // Shared
    void setFov(float newFov);
    void setTarget(const XMFLOAT3& t);
    void setProjection(float fov, float aspect, float nearZ, float farZ);
    void updatePositionFromOrbit();

    XMMATRIX getViewMatrix()           const { return view; }
    XMMATRIX getProjectionMatrix()     const { return projection; }
    XMMATRIX getViewProjectionMatrix() const { return XMMatrixMultiply(view, projection); }
    XMFLOAT3 getPosition()             const { return position; }
    float    getFov()                  const { return fov; }
    float    getRadius()               const { return radius; }

    XMFLOAT3 getForward() const;
    XMFLOAT3 getRight()   const;

    // for particles
    XMFLOAT3 getCamRight() const {
        XMFLOAT3 r;
        XMStoreFloat3(&r, XMVector3Normalize(XMVectorSet(
            view.r[0].m128_f32[0],
            view.r[1].m128_f32[0],
            view.r[2].m128_f32[0], 0)));
        return r;
    }

    XMFLOAT3 getCamUp() const {
        XMFLOAT3 u;
        XMStoreFloat3(&u, XMVector3Normalize(XMVectorSet(
            view.r[0].m128_f32[1],
            view.r[1].m128_f32[1],
            view.r[2].m128_f32[1], 0)));
        return u;
    }

private:
    void updateViewMatrix();

private:
    CameraMode mode = CameraMode::Orbit;

    float fov;
    float aspect;
    float nearZ;
    float farZ;

    float yaw   = 0.0f;
    float pitch = 0.0f;

    float radius       = 20.0f;
    float targetRadius = 20.0f;
    float minRadius    = 1.0f;
    float maxRadius    = 100.0f;

    // Third person settings
    float tpDistance = 5.0f;
    float tpHeight   = 1.8f;
    float tpPitch    = 0.3f;

    XMFLOAT3 position;
    XMFLOAT3 target;

    XMMATRIX projection;
    XMMATRIX view;
};