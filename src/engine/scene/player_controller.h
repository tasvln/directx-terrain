#pragma once

#include "utils/pch.h"

class PlayerController {
public:
    PlayerController();

    void update(
        float deltaTime,
        const XMFLOAT3& moveInput,
        bool jump,
        std::function<float(float, float)> sampleHeight
    );
    
    const PlayerState& getState() const { return state; }

    void setPosition(const DirectX::XMFLOAT3& pos);

private:
    PlayerState state;
    
    float moveSpeed = 6.0f;
    float gravity   = -20.0f;
    float jumpForce = 8.0f;
};