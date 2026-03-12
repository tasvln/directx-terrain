#pragma once
#include "utils/key_codes.h"
#include <array>

class Keyboard
{
public:
    Keyboard() { keys.fill(false); }

    void onKeyPressed(KeyCode::Key key) {
        keys[static_cast<uint8_t>(key)] = true;
    }

    void onKeyReleased(KeyCode::Key key) {
        keys[static_cast<uint8_t>(key)] = false;
    }

    bool isDown(KeyCode::Key key) const {
        return keys[static_cast<uint8_t>(key)];
    }

    // Returns true only on the frame the key was first pressed
    bool isJustPressed(KeyCode::Key key) const {
        uint8_t idx = static_cast<uint8_t>(key);
        return keys[idx] && !prevKeys[idx];
    }

    // Call once per frame at the START of onUpdate
    void tick() {
        prevKeys = keys;
    }

private:
    std::array<bool, 256> keys;
    std::array<bool, 256> prevKeys;
};