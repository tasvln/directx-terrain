#pragma once

#include "utils/pch.h"

class Window;
class Device;
class CommandQueue;
class Swapchain;
class Model;
class ConstantBuffer;
class Pipeline;
class Camera;
class Lighting;
class Grid;
class PlayerController;
class Keyboard;
class Terrain;
class Clock;
class Sky;

class UpdateEventArgs;
class RenderEventArgs;
class ResizeEventArgs;
class MouseWheelEventArgs;
class MouseMotionEventArgs;
class KeyEventArgs;

class Application
{
    public:
        Application(HINSTANCE hInstance, WindowConfig& config);
        ~Application();

        int run();

        // Window event callbacks — called by Window via IWindowEventHandler
        void onUpdate(UpdateEventArgs& args);
        void onRender(RenderEventArgs& args);
        void onResize(ResizeEventArgs& args);
        void onMouseWheel(MouseWheelEventArgs& args);
        void onMouseMoved(MouseMotionEventArgs& args);
        void onKeyPressed(KeyEventArgs& e);
        void onKeyReleased(KeyEventArgs& e);

        // Utility — transitions a GPU resource between states (e.g. present → render target)
        void transitionResource(
            ComPtr<ID3D12GraphicsCommandList2> commandList,
            ComPtr<ID3D12Resource> resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState
        );

    private:
        void init();
        void cleanUp();

    private:
        // --- Camera state ---
        float cameraYaw   = 0.0f;
        float cameraPitch = 0.3f;

        // --- Model state ---
        float modelScale = 1.0f;

        // --- Window/DX12 state ---
        HWND         hwnd     = nullptr;
        WindowConfig config;
        RECT         windowRect = {};

        UINT     currentBackBufferIndex;
        uint64_t fenceValues[FRAMEBUFFERCOUNT]{};

        D3D12_VIEWPORT viewport;
        D3D12_RECT     scissorRect;

        // --- Core DX12 ---
        std::unique_ptr<Window>       window;
        std::unique_ptr<Device>       device;
        std::unique_ptr<CommandQueue> directCommandQueue;
        std::unique_ptr<Swapchain>    swapchain;

        // --- Scene ---
        std::unique_ptr<Camera>          camera1;
        std::unique_ptr<Lighting>        lighting1;
        std::unique_ptr<PlayerController> player;
        std::unique_ptr<Keyboard>        keyboard;
        std::unique_ptr<Clock>           clock;
        std::unique_ptr<Grid>            sceneGrid;
        std::unique_ptr<Terrain>         terrain1;
        std::unique_ptr<Sky>         sky;

        // --- Model ---
        std::unique_ptr<Model>         model;
        XMMATRIX viewProj = XMMatrixIdentity();
};