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

class UpdateEventArgs;
class RenderEventArgs;
class ResizeEventArgs;
class MouseWheelEventArgs;
class UpdateEventArgs;
class MouseMotionEventArgs;

class KeyEventArgs;

class Application
{
    public:
        Application(HINSTANCE hInstance, WindowConfig &config);
        ~Application();

        // running the application
        int run();

        void onResize(ResizeEventArgs& args);

        // main functions for rendering / callbacks
        void onUpdate(UpdateEventArgs& args);
        void onRender(RenderEventArgs& args);
        void onMouseWheel(MouseWheelEventArgs& args);
        void onMouseMoved(MouseMotionEventArgs& args);

        void transitionResource(
            ComPtr<ID3D12GraphicsCommandList2> commandList,
            ComPtr<ID3D12Resource> resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState
        );

        void onKeyPressed(KeyEventArgs& e);
        void onKeyReleased(KeyEventArgs& e);

    private:
        void init();
        void cleanUp();

    private:
        float cameraYaw = 0.0f;
        float cameraPitch = 0.3f;
        float modelScale = 1.0f;
        
        HWND hwnd = nullptr;
        WindowConfig config;
        RECT windowRect = {};

        UINT currentBackBufferIndex;
        uint64_t fenceValues[FRAMEBUFFERCOUNT] {};

        D3D12_VIEWPORT viewport;
        D3D12_RECT scissorRect;

        // unique pttrsssssss -> GPU resources
        std::unique_ptr<Window> window;
        std::unique_ptr<Device> device;
        std::unique_ptr<CommandQueue> directCommandQueue;
        // std::unique_ptr<CommandQueue> computeCommandQueue;
        // std::unique_ptr<CommandQueue> copyCommandQueue;
        std::unique_ptr<Swapchain> swapchain;
        std::unique_ptr<Model> model;
        std::unique_ptr<ConstantBuffer> mvpBuffer;
        std::unique_ptr<ConstantBuffer> materialBuffer;
        std::unique_ptr<Pipeline> pipeline1;
        
        std::unique_ptr<Keyboard> keyboard;
        std::unique_ptr<Camera> camera1;

        std::unique_ptr<Grid> sceneGrid;

        std::unique_ptr<Lighting> lighting1;

        std::unique_ptr<PlayerController> player;
};