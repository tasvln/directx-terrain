#include "application.h"
#include "window.h"

#include "engine/device.h"
#include "engine/command_queue.h"
#include "engine/swapchain.h"
#include "engine/mesh.h"
#include "engine/shader.h"
#include "engine/pipeline.h"
#include "engine/model.h"

#include "engine/resources/constant.h"

#include "engine/scene/camera.h"
#include "engine/scene/lighting.h"
#include "engine/scene/grid.h"
#include "engine/scene/player_controller.h"

#include "utils/events.h"
#include "utils/keyboard.h"
#include "utils/frame_timer.h"

Application::Application(
    HINSTANCE hInstance, 
    WindowConfig &config
) :  
    config(config) 
{
    window = std::make_unique<Window>(hInstance, config, this);

    init();
}

Application::~Application() {
    cleanUp();
}

void Application::init() {
    LOG_INFO(L"Application -> Initializing...");

    scissorRect = CD3DX12_RECT(
        0, 
        0, 
        static_cast<LONG>(config.width), 
        static_cast<LONG>(config.height)
    );

    viewport = CD3DX12_VIEWPORT(
        0.0f, 
        0.0f, 
        static_cast<float>(config.width), 
        static_cast<float>(config.height)
    );

    device = std::make_unique<Device>(config.useWarp);
    LOG_INFO(L"Application -> device initialized!");

    directCommandQueue = std::make_unique<CommandQueue>(
        device->getDevice(),
        D3D12_COMMAND_LIST_TYPE_DIRECT
    );
    LOG_INFO(L"Application -> directCommandQueue initialized!");

    // computeCommandQueue = std::make_unique<CommandQueue>(
    //     device->getDevice(),
    //     D3D12_COMMAND_LIST_TYPE_COMPUTE
    // );
    // LOG_INFO(L"Engine->DirectX 12 computeCommandQueue initialized.");

    // copyCommandQueue = std::make_unique<CommandQueue>(
    //     device->getDevice(),
    //     D3D12_COMMAND_LIST_TYPE_COPY
    // );
    // LOG_INFO(L"Engine->DirectX 12 copyCommandQueue initialized.");

    swapchain = std::make_unique<Swapchain>(
        window->getHwnd(),
        device->getDevice(),
        directCommandQueue->getCommandQueue(),
        config.width,
        config.height,
        FRAMEBUFFERCOUNT,
        device->getSupportTearingState()
    );
    LOG_INFO(L"Application -> swapchain initialized!");

    currentBackBufferIndex = swapchain->getSwapchain()->GetCurrentBackBufferIndex();

    LOG_INFO(L"Application Class initialized!");
    LOG_INFO(L"-- Resources --");

    // create buffers
    model = std::make_unique<Model>(
        device->getDevice(),
        directCommandQueue.get(),
        swapchain->getSRVHeap(),
        // "assets/models/building1/building.obj"
        // "assets/models/cat/cat.obj"
        "assets/models/mountain1/mountain.obj"
        // "assets/models/weapon1/sniper.obj"
    );
    LOG_INFO(L"Model Resource initialized!");

    mvpBuffer = std::make_unique<ConstantBuffer>(
        device->getDevice(),
        static_cast<UINT>(sizeof(MVPConstantStruct))
    );
    LOG_INFO(L"mvpBuffer Resource initialized!");

    materialBuffer = std::make_unique<ConstantBuffer>(
        device->getDevice(),
        static_cast<UINT>(sizeof(MaterialData))
    );
    LOG_INFO(L"mvpBuffer Resource initialized!");

    auto modelCenter = model->getBoundingCenter();
    auto modelRadius = model->getBoundingRadius();

    modelScale = 200.0f / modelRadius;

    keyboard = std::make_unique<Keyboard>();

    camera1 = std::make_unique<Camera>(
        45.0f,
        static_cast<float>(config.width) / static_cast<float>(config.height),
        0.1f,
        modelRadius * 10.0f
    );
    LOG_INFO(L"Camera initialized!");

    camera1->frameModel(modelCenter, modelRadius);
    camera1->setThirdPerson(6.0f, 3.0f, 0.3f);

    lighting1 = std::make_unique<Lighting>(
        device->getDevice()
    );
    LOG_INFO(L"Lighting initialized!");

    sceneGrid = std::make_unique<Grid>(
        device->getDevice(),
        directCommandQueue.get(),
        swapchain->getSRVHeap()
    );

    player = std::make_unique<PlayerController>();
    
    float startHeight = sceneGrid->sampleHeight(0.0f, 0.0f);

    player->setPosition({0.0f, startHeight, 0.0f});
    

    // pipeline
    // Root parameters: TODO: make it dynamic?

    // MVP = b0 (VS)
    CD3DX12_ROOT_PARAMETER cbvMvpParam;
    cbvMvpParam.InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    // Material = b0 (PS)
    CD3DX12_ROOT_PARAMETER cbvMaterialParam;
    cbvMaterialParam.InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    // Lighting = b1 (PS)
    CD3DX12_ROOT_PARAMETER cbvLightParam;
    cbvLightParam.InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    // Texture = t0 (PS)
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER srvRootParam;
    srvRootParam.InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Texture = t1 (PS)
    CD3DX12_DESCRIPTOR_RANGE normalSrvRange;
    normalSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_ROOT_PARAMETER normalSrvParam;
    normalSrvParam.InitAsDescriptorTable(1, &normalSrvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Texture = t2 (PS)
    CD3DX12_DESCRIPTOR_RANGE specularSrvRange;
    specularSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

    CD3DX12_ROOT_PARAMETER specularSrvParam;
    specularSrvParam.InitAsDescriptorTable(1, &specularSrvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Combine
    std::vector<D3D12_ROOT_PARAMETER> rootParams = {
        cbvMvpParam,
        cbvMaterialParam,
        cbvLightParam,
        srvRootParam,
        normalSrvParam,
        specularSrvParam
    };

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    auto vertexShader = Shader(L"assets/shaders/vertex.cso");
    auto pixelShader = Shader(L"assets/shaders/pixel.cso");

    
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplers { sampler };

    pipeline1 = std::make_unique<Pipeline>(
        device->getDevice(),
        vertexShader,
        pixelShader,
        inputLayout,
        rootParams,
        samplers,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_D24_UNORM_S8_UINT
    );

    
    // --------------------
    // Initialize material
    // --------------------
    MaterialData mat;
    mat.emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
    mat.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mat.specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    
    mat.specularPower = 64.0f; // Typical values: 16.0f, 32.0f, 64.0f
    mat.useTexture = 1;

    mat.useNormalMap = 0;
    mat.useSpecularMap = 0;

    materialBuffer->update(&mat, sizeof(MaterialData));

    // --------------------
    // Initialize lights
    // --------------------
    lighting1->setLight(
        0,
        LightType::Directional,
        { 0.0f, 0.0f, 0.0f }, // position ignored for directional
        { 0.5f, 1.0f, -0.5f }, // direction -> change for light (sun or moon) direction
        0.0f, // range,
        0.0f, // innerAngle,
        0.0f, // outerAngle
        { 1.0f, 1.0f, 1.0f }, // color -> light color (sun or moon)
        20.0f // intensity
    );

    lighting1->setLight(
        1,
        LightType::Point,
        { -1.0f, 5.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }, // direction ignored
        20.0f, 
        0.0f, 
        0.0f,
        { 1.0f, 0.9f, 0.8f },
        1.0f
    );

    lighting1->updateGPU();
}

int Application::run() {
    MSG msg = {};
    Timer timer;

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            timer.tick();

            UpdateEventArgs updateArgs(
                timer.getDeltaSeconds(), 
                timer.getTotalSeconds()
            );
            onUpdate(updateArgs);

            RenderEventArgs renderArgs(
                timer.getDeltaSeconds(), 
                timer.getTotalSeconds()
            );
            onRender(renderArgs);
            
            std::wstring title = std::wstring(config.appName) + L" - " + timer.getFPSString();
            if (window) {
                SetWindowTextW(window->getHwnd(), title.c_str());
            } else {
                LOG_ERROR(L"Window is null!");
            }
        }
    }

    return static_cast<int>(msg.wParam);
}

void Application::onUpdate(UpdateEventArgs& args)
{
    // Movement relative to cameraYaw, not camera forward vector
    XMFLOAT3 moveInput = { 0.0f, 0.0f, 0.0f };

    float fw_x =  sinf(cameraYaw);
    float fw_z =  cosf(cameraYaw);
    float ri_x =  cosf(cameraYaw);
    float ri_z = -sinf(cameraYaw);

    if (keyboard->isDown(KeyCode::Key::W)) { moveInput.x += fw_x; moveInput.z += fw_z; }
    if (keyboard->isDown(KeyCode::Key::S)) { moveInput.x -= fw_x; moveInput.z -= fw_z; }
    if (keyboard->isDown(KeyCode::Key::D)) { moveInput.x += ri_x; moveInput.z += ri_z; }
    if (keyboard->isDown(KeyCode::Key::A)) { moveInput.x -= ri_x; moveInput.z -= ri_z; }

    float len = std::sqrt(moveInput.x * moveInput.x + moveInput.z * moveInput.z);
    if (len > 1.0f) { moveInput.x /= len; moveInput.z /= len; }

    bool jump = keyboard->isJustPressed(KeyCode::Key::Space);

    player->update(
        static_cast<float>(args.elapsedTime),
        moveInput,
        jump,
        [&](float x, float z) { return sceneGrid->sampleHeight(x, z); }
    );

    const PlayerState& p = player->getState();

    // Follow player with third person camera
    camera1->followPlayer(p.position, cameraYaw, cameraPitch);

    // XMMATRIX model      = XMMatrixIdentity();
    XMFLOAT3 center = model->getBoundingCenter();
    // XMMATRIX modelMat = XMMatrixScaling(modelScale, modelScale, modelScale);
    XMMATRIX modelMat = 
    XMMatrixScaling(modelScale, modelScale, modelScale) *
    XMMatrixTranslation(
        -center.x * modelScale,   // center X
        -center.y * modelScale,   // sit on ground
        -center.z * modelScale    // center Z
    );
    XMMATRIX view       = camera1->getViewMatrix();
    XMMATRIX projection = camera1->getProjectionMatrix();

    MVPConstantStruct mvpData;
    mvpData.model    = XMMatrixTranspose(modelMat);
    mvpData.viewProj = XMMatrixTranspose(view * projection);
    mvpBuffer->update(&mvpData, sizeof(mvpData));

    XMFLOAT3 camPos = camera1->getPosition();
    lighting1->setEyePosition(camPos);
    lighting1->updateGPU();

    sceneGrid->updateMVP(view * projection);
    keyboard->tick();
}

void Application::onRender(RenderEventArgs& args)
{
    LOG_INFO(L"Application -> Rendering frame...");

    // Get a command list from the queue; allocator is managed internally
    auto commandList = directCommandQueue->getCommandList();
    LOG_INFO(L"Application -> CommandList acquired.");

    auto commandQueue = directCommandQueue->getCommandQueue();

    auto pipelineState = pipeline1->getPipelineState();
    auto rootSignature = pipeline1->getRootSignature();
    auto rtvHeap = swapchain->getRTVHeap();
    auto dsvHeap = swapchain->getDSVHeap();
    auto srvHeap = swapchain->getSRVHeap();
    auto vsync = device->getSupportTearingState();

    // Set viewport and scissor
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    // Transition back buffer to render target
    auto backBuffer = swapchain->getBackBuffer(currentBackBufferIndex);
    transitionResource(
        commandList, 
        backBuffer.Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    LOG_INFO(L"Application -> Back buffer transitioned to RENDER_TARGET.");

    // Set render target and depth-stencil
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        rtvHeap->getHeap()->GetCPUDescriptorHandleForHeapStart(),
        currentBackBufferIndex, 
        rtvHeap->getDescriptorSize()
    );
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
        dsvHeap->getHeap()->GetCPUDescriptorHandleForHeapStart()
    );
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Clear render target and depth-stencil
    const float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(
        dsvHandle, 
        D3D12_CLEAR_FLAG_DEPTH, 
        1.0f, 0, 0, 
        nullptr
    );
    LOG_INFO(L"Application -> Render target and depth-stencil cleared.");

    sceneGrid->draw(commandList.Get());
    LOG_INFO(L"Application -> sceneGrid->draw.");

    
    // Reset command list with current pipeline
    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetGraphicsRootSignature(rootSignature.Get());
    LOG_INFO(L"Application -> Pipeline state and root signature set.");

    // Draw the Model
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    LOG_INFO(L"Application -> Primitive topology set to TRIANGLELIST.");

    // Set constant buffer (MVP updated in onUpdate)
    commandList->SetGraphicsRootConstantBufferView(0, mvpBuffer->getGPUAddress());
    LOG_INFO(L"Application -> Constant buffer bound.");

    commandList->SetGraphicsRootConstantBufferView(1, materialBuffer->getGPUAddress());

    // Root parameter 1 = light CBV
    commandList->SetGraphicsRootConstantBufferView(2, lighting1->getCBV()->getGPUAddress());
    LOG_INFO(L"Application -> Lighting CBV bound.");

    // call mesh/model draw
    model->draw(
        commandList.Get(),
        srvHeap->getHeap().Get(),
        3 // Root parameter index for the SRV (t0)
    );
    
    LOG_INFO(L"Application -> Model drawn.");

    // Transition back buffer to present
    transitionResource(commandList, backBuffer.Get(),
                       D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PRESENT);
    LOG_INFO(L"Application -> Back buffer transitioned to PRESENT.");

    // Execute command list
    fenceValues[currentBackBufferIndex] = directCommandQueue->executeCommandList(commandList);
    LOG_INFO(L"Application -> CommandList executed.");

    // Present
    INT syncInterval = vsync ? 1 : 0;
    UINT presentFlags = !vsync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    throwFailed(swapchain->getSwapchain()->Present(syncInterval, presentFlags));
    LOG_INFO(L"Application -> Frame presented.");

    currentBackBufferIndex = swapchain->getSwapchain()->GetCurrentBackBufferIndex();

    // Wait for GPU to finish frame
    directCommandQueue->fenceWait(fenceValues[currentBackBufferIndex]);
    LOG_INFO(L"Application -> GPU finished frame.");
}

void Application::transitionResource(
    ComPtr<ID3D12GraphicsCommandList2> commandList,
    ComPtr<ID3D12Resource> resource,
    D3D12_RESOURCE_STATES beforeState,
    D3D12_RESOURCE_STATES afterState
) {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource.Get(),
        beforeState,
        afterState
    );

    commandList->ResourceBarrier(1, &barrier);
}

void Application::onResize(ResizeEventArgs& args)
{
    if (!device || !swapchain)
        return;

    // Skip if minimized
    if (args.width == 0 || args.height == 0)
    {
        LOG_INFO(L"Resize skipped (window minimized: %dx%d)", args.width, args.height);
        return;
    }

    // Only resize if size actually changed
    if (config.width != args.width || config.height != args.height) {
        config.width = std::max(1u, static_cast<unsigned int>(args.width));
        config.height = std::max(1u, static_cast<unsigned int>(args.height));

        // Wait for GPU to finish any in-flight commands
        directCommandQueue->flush();

        // Resize swap chain buffers
        swapchain->resize(config.width, config.height);

        // Reset back buffer index after resize
        currentBackBufferIndex = swapchain->getSwapchain()->GetCurrentBackBufferIndex();

        // Update camera projection
        camera1->setProjection(
            45.0f,
            float(config.width) / float(config.height),
            0.1f,
            100.0f
        );

        // Update viewport + scissor rect
        viewport = { 0.0f, 0.0f, float(config.width), float(config.height), 0.0f, 1.0f };
        scissorRect = { 0, 0, static_cast<LONG>(config.width), static_cast<LONG>(config.height) };

        LOG_INFO(L"Application resized to %dx%d", config.width, config.height);
    }
}

void Application::onMouseWheel(MouseWheelEventArgs& args)
{
    // if (args.control) {
    //     camera1->zoom(args.wheelDelta * 0.1f); // radius zoom
    // } 
    // else {
    //     camera1->setFov(camera1->getFov() - args.wheelDelta * 0.02f); // slow FOV change
    // }

    camera1->setFov(camera1->getFov() - args.wheelDelta * 0.05f);

    // camera1->zoom(args.wheelDelta * 0.1f);
}

void Application::onMouseMoved(MouseMotionEventArgs& args) {
    if (args.leftButton) {
        if (camera1->getMode() == CameraMode::ThirdPerson) {
            cameraYaw   += static_cast<float>(args.relX) * 0.01f;
            cameraPitch -= static_cast<float>(args.relY) * 0.01f;  // inverted Y feels natural
            cameraPitch  = std::clamp(cameraPitch, -XM_PIDIV4, XM_PIDIV2 - 0.05f); // don't flip over
        } else if (args.shift) {
            camera1->pan(static_cast<float>(args.relX), static_cast<float>(args.relY));
        } else {
            camera1->orbit(static_cast<float>(args.relX) * 0.01f, static_cast<float>(args.relY) * 0.01f);
        }
    }
}

void Application::cleanUp()
{
    LOG_INFO(L"Application cleanup started.");

    if (directCommandQueue)
    {
        LOG_INFO(L"Flushing GPU commands before releasing resources...");
        directCommandQueue->flush();
    }

    // Release GPU-dependent objects first
    if (sceneGrid) {
        sceneGrid.reset();
        LOG_INFO(L"Scene grid released.");
    }

    if (model) {
        model.reset();
        LOG_INFO(L"Model released.");
    }

    if (mvpBuffer) {
        mvpBuffer.reset();
        LOG_INFO(L"MVP constant buffer released.");
    }

    if (materialBuffer) {
        materialBuffer.reset();
        LOG_INFO(L"Material constant buffer released.");
    }

    if (lighting1) {
        lighting1.reset();
        LOG_INFO(L"Lighting released.");
    }

    if (pipeline1) {
        pipeline1.reset();
        LOG_INFO(L"Pipeline released.");
    }

    if (camera1) {
        camera1.reset();
        LOG_INFO(L"Camera released.");
    }

    if (swapchain) {
        swapchain.reset();
        LOG_INFO(L"Swapchain released.");
    }

    if (directCommandQueue) {
        directCommandQueue.reset();
        LOG_INFO(L"Command queue released.");
    }

    if (device) {
        device.reset();
        LOG_INFO(L"Device released.");
    }

    if (window) {
        window.reset();
        LOG_INFO(L"Window released.");
    }

    hwnd = nullptr;
    currentBackBufferIndex = 0;

    LOG_INFO(L"Application cleanup finished.");
}

void Application::onKeyPressed(KeyEventArgs& e) {
    keyboard->onKeyPressed(e.key);
}

void Application::onKeyReleased(KeyEventArgs& e) {
    keyboard->onKeyReleased(e.key);
}