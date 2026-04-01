#include "utils/pch.h"
#include "core/application.h"

// std::unique_ptr<Application> application;

WindowConfig config = {
    .appName = L"DirectX Terrain + Weather System Renderer",
    .windowClassName = L"DirectWindowClass",
    .width = 1440,
    .height = 700,
    .enabledDirectX = false,
    .useWarp = false
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    std::cout << "Debug console initialized.\n";

    try
    {
        auto application = std::make_unique<Application>(hInstance, config);
        return application->run();
    }
    catch (const std::exception &e)
    {
        MessageBoxA(nullptr, e.what(), "Fatal Error", MB_ICONERROR);
        std::cerr << e.what() << std::endl;
        std::cin.get();
        return -1;
    }
}

