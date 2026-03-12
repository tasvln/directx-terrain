#include "camera.h"

Camera::Camera(
    float fov, 
    float aspect, 
    float nearZ, 
    float farZ
) : 
    fov(fov),
    aspect(aspect),
    nearZ(nearZ),
    farZ(farZ),
    position{ 0.0f, 0.0f, -20.0f }, 
    target{ 0.0f, 0.0f, 0.0f },
    radius(20.0f), 
    targetRadius(20.0f)
{
    LOG_INFO(L"Initializing Camera...");
    LOG_INFO(L"Camera position set to (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
    LOG_INFO(L"Camera target set to (%.2f, %.2f, %.2f)", target.x, target.y, target.z);

    setProjection(
        fov,
        aspect,
        nearZ,
        farZ
    );
    
    updateViewMatrix();
}

void Camera::frameModel(const XMFLOAT3& center, float boundingRadius)
{
    target = center;
    targetRadius = boundingRadius * 2.5f;
    minRadius = boundingRadius * 1.5f;
    maxRadius = boundingRadius * 20.0f;

    // Look at front (+Z) with slight top-down angle
    yaw = 0.0f; // front of model
    pitch = XMConvertToRadians(20.0f);

    radius = targetRadius;
    updatePositionFromOrbit();
}

void Camera::setProjection(
    float fov, 
    float aspect, 
    float nearZ, 
    float farZ
) {
    this->projection = XMMatrixPerspectiveFovLH(
        fov,
        aspect,
        nearZ,
        farZ
    );

    LOG_INFO(L"Projection matrix updated. FOV: %.2f, Aspect: %.2f, NearZ: %.2f, FarZ: %.2f", fov, aspect, nearZ, farZ);
}

void Camera::update(float delta)
{
    if (mode == CameraMode::Orbit)
    {
        float t = 1.0f - std::exp(-delta * 5.0f);
        radius = radius + (targetRadius - radius) * t;
        updateViewMatrix();
    }
    else if (mode == CameraMode::FPS)
    {
        updateFPSView();
    }
}

void Camera::updateViewMatrix() {
    XMVECTOR pos = XMLoadFloat3(&position);
    XMVECTOR tgt = XMLoadFloat3(&target);
    XMVECTOR up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    this->view = XMMatrixLookAtLH(pos, tgt, up);
}

void Camera::updatePositionFromOrbit() {
    // Spherical → Cartesian
    float x = radius * cosf(pitch) * sinf(yaw);
    float y = radius * sinf(pitch);
    float z = radius * cosf(pitch) * cosf(yaw);

    position.x = target.x + x;
    position.y = target.y + y;
    position.z = target.z + z;

    updateViewMatrix();
}

void Camera::orbit(float deltaYaw, float deltaPitch)
{
    // Horizontal rotation (yaw)
    yaw += deltaYaw;
    // Vertical rotation (pitch) - clamp to avoid flipping
    pitch = std::clamp(pitch + deltaPitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    updatePositionFromOrbit();
}

// Zoom by FOV (lens zoom) -> more FPS camera orientated
// The camera stays in place, but the projection matrix FOV changes. Perspective distortion changes (things stretch when wide)
void Camera::setFov(float newFov) {
    this->fov = std::clamp(newFov, 0.1f, XM_PIDIV2);
    setProjection(
        fov,
        aspect,
        nearZ,
        farZ
    );
}

// Zoom by radius (what you have now)
// Zooming moves the camera closer or farther from the target (changes radius). This is like physically walking toward/away from the cube.
void Camera::zoom(float wheelDelta)
{
    targetRadius *= (1.0f - wheelDelta * 0.1f);
    targetRadius = std::clamp(targetRadius, minRadius, maxRadius);
    radius = targetRadius;

    updatePositionFromOrbit();
}

void Camera::pan(float deltaX, float deltaY)
{
    float factor = radius * 0.001f; // tweak for sensitivity

    // Camera local axes
    XMVECTOR tgt = XMLoadFloat3(&target);
    XMVECTOR pos = XMLoadFloat3(&position);

    XMVECTOR forward = XMVector3Normalize(tgt - pos);
    XMVECTOR up = XMVectorSet(0,1,0,0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

    // Apply movement
    tgt = XMVectorAdd(tgt, XMVectorScale(right, deltaX * factor));
    tgt = XMVectorAdd(tgt, XMVectorScale(up, -deltaY * factor));

    XMStoreFloat3(&target, tgt);
    updatePositionFromOrbit();
}

// FPS
void Camera::setFPS(const XMFLOAT3& pos, float yaw, float pitch)
{
    mode = CameraMode::FPS;
    position = pos;

    this->yaw = yaw;
    this->pitch = pitch;
    updateFPSView();
}

void Camera::updateFPSView()
{
    XMVECTOR pos = XMLoadFloat3(&position);

    XMVECTOR forward = XMVectorSet(
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw),
        0.0f
    );

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    view = XMMatrixLookToLH(pos, forward, up);
}

XMFLOAT3 Camera::getForward() const
{
    XMFLOAT3 f;
    f.x = target.x - position.x;
    f.y = 0.0f; // flatten for walking
    f.z = target.z - position.z;

    float len = std::sqrt(f.x * f.x + f.z * f.z);
    if (len > 0.0f)
    {
        f.x /= len;
        f.z /= len;
    }

    return f;
}

XMFLOAT3 Camera::getRight() const
{
    XMFLOAT3 f = getForward();

    // cross(up, forward)
    XMFLOAT3 r;
    r.x =  f.z;
    r.y =  0.0f;
    r.z = -f.x;

    return r;
}

void Camera::setTarget(const XMFLOAT3& t)
{
    target = t;
}

void Camera::setThirdPerson(float distance, float height, float pitch)
{
    mode        = CameraMode::ThirdPerson;
    tpDistance  = distance;
    tpHeight    = height;
    tpPitch     = pitch;
}

void Camera::followPlayer(const XMFLOAT3& playerPos, float yaw, float pitch)
{
    float offsetX = -sinf(yaw)   * cosf(pitch) * tpDistance;
    float offsetZ = -cosf(yaw)   * cosf(pitch) * tpDistance;
    float offsetY =  sinf(pitch) * tpDistance + tpHeight;

    position.x = playerPos.x + offsetX;
    position.y = playerPos.y + offsetY;
    position.z = playerPos.z + offsetZ;

    target.x = playerPos.x;
    target.y = playerPos.y + tpHeight * 0.8f;
    target.z = playerPos.z;

    updateViewMatrix();
}
