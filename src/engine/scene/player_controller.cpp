#include "player_controller.h"

PlayerController::PlayerController() {
    state.position = { 0.0f, 5.0f, 0.0f };
    state.velocity = { 0.0f, 0.0f, 0.0f };
    state.radius   = 0.4f;
    state.height   = 1.8f;
    state.grounded = false;
}

void PlayerController::update(
    float deltaTime,
    const XMFLOAT3& moveInput,
    bool jump,
    std::function<float(float, float)> sampleHeight
)
{
    const float gravity = 9.81f;
    const float moveSpeed = 6.0f;
    const float jumpSpeed = 5.0f;

    // Horizontal movement (world-space for now)
    state.velocity.x = moveInput.x * moveSpeed;
    state.velocity.z = moveInput.z * moveSpeed;

    // Jump
    if (jump && state.grounded)
    {
        state.velocity.y = jumpSpeed;
        state.grounded = false;
    }

    // Gravity
    state.velocity.y -= gravity * deltaTime;

    // Integrate
    state.position.x += state.velocity.x * deltaTime;
    state.position.y += state.velocity.y * deltaTime;
    state.position.z += state.velocity.z * deltaTime;

    // Ground collision
    float groundY = sampleHeight(state.position.x, state.position.z);
    float minY = groundY + state.radius;

    LOG_INFO(L"pos.y=%.2f groundY=%.2f minY=%.2f grounded=%d", 
    state.position.y, groundY, minY, state.grounded);

    if (state.position.y <= minY)
    {
        state.position.y = minY;
        state.velocity.y = 0.0f;
        state.grounded = true;
    }
}

void PlayerController::setPosition(const XMFLOAT3& pos)
{
    state.position = pos;
}