#include "window.h"
#include "application.h"
#include "utils/events.h"

Window::Window(
    HINSTANCE hInstance, 
    WindowConfig& config, 
    Application* app
) :
    config(config),
    app(app)
{
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = config.windowClassName;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, L"Unable to register the window class.", L"Error", MB_OK | MB_ICONERROR);
        LOG_ERROR(L"RegisterClassEx failed!");
    }
    LOG_INFO(L"RegisterClassEx created!");

    hwnd = createWindow(hInstance);

    if (!hwnd) {
        LOG_ERROR(L"Window handle is null!");
        throw std::runtime_error("Failed to create window");
    }

    ShowWindow(hwnd, SW_SHOW);
}

Window::~Window() {
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

HWND Window::createWindow(HINSTANCE hInstance)
{
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    windowRect = {
        0,
        0,
        static_cast<LONG>(config.width),
        static_cast<LONG>(config.height)
    };

    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
    int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

    return CreateWindowEx(
        0,
        config.windowClassName,
        config.appName,
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        hInstance,
        this
    );
}

void Window::setFullScreen(bool enable) {
    if (config.fullscreen == enable) return; // nothing to do

    config.fullscreen = enable;

    if (enable) {
        // Save current placement (size/pos/maximized state)
        GetWindowPlacement(hwnd, &windowPlacement);

        // Remove borders & caption
        SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

        // Expand to monitor bounds
        HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX monitorInfo = { sizeof(MONITORINFOEX) };
        GetMonitorInfo(hMonitor, &monitorInfo);

        SetWindowPos(
            hwnd,
            HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE
        );
        ShowWindow(hwnd, SW_MAXIMIZE);
    }
    else {
        // Restore borders
        SetWindowLongW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        // Restore placement (size/pos/max state)
        SetWindowPlacement(hwnd, &windowPlacement);
        SetWindowPos(
            hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
        );
        ShowWindow(hwnd, SW_NORMAL);
    }
}

void Window::onFullscreen()
{
    setFullScreen(!config.fullscreen);
}

LRESULT CALLBACK Window::wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* window = nullptr;

    if (msg == WM_NCCREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd = hWnd;
    }
    else
    {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (window)
    {
        return window->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT Window::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_SIZE:
            if (app)
            {
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);

                ResizeEventArgs resizeArgs(width, height);
                app->onResize(resizeArgs);
            }
            break;
            
        case WM_KEYDOWN:
            if (wParam == VK_F11) {
                onFullscreen();
                return 0;
            }
            else if (wParam == VK_ESCAPE && config.fullscreen) {
                onFullscreen();
                return 0;
            }

            if (app) {
                KeyEventArgs keyArgs(
                    static_cast<KeyCode::Key>(wParam),
                    0,
                    KeyEventArgs::Pressed,
                    GetKeyState(VK_CONTROL) < 0,
                    GetKeyState(VK_SHIFT)   < 0,
                    GetKeyState(VK_MENU)    < 0
                );
                app->onKeyPressed(keyArgs);
            }
            break;
        case WM_KEYUP:
            if (app) {
                KeyEventArgs keyArgs(
                    static_cast<KeyCode::Key>(wParam),
                    0,
                    KeyEventArgs::Released,
                    GetKeyState(VK_CONTROL) < 0,
                    GetKeyState(VK_SHIFT)   < 0,
                    GetKeyState(VK_MENU)    < 0
                );
                app->onKeyReleased(keyArgs);
            }
            break;

        case WM_MOUSEMOVE:
            if (app) {
                short keyStates = static_cast<short>(LOWORD(wParam));

                bool lButton = (keyStates & MK_LBUTTON) != 0;
                bool rButton = (keyStates & MK_RBUTTON) != 0;
                bool mButton = (keyStates & MK_MBUTTON) != 0;
                bool shift   = (keyStates & MK_SHIFT)   != 0;
                bool control = (keyStates & MK_CONTROL) != 0;

                int x = static_cast<int>(SHORT(LOWORD(lParam)));
                int y = static_cast<int>(SHORT(HIWORD(lParam)));

                static int lastX = x;
                static int lastY = y;

                int relX = x - lastX;
                int relY = y - lastY;

                lastX = x;
                lastY = y;

                MouseMotionEventArgs args(
                    lButton, 
                    mButton, 
                    rButton, 
                    control, 
                    shift, 
                    x, 
                    y
                );
                args.relX = relX;
                args.relY = relY;

                app->onMouseMoved(args);
            }
            break;

        case WM_MOUSEWHEEL:
            if (app) {
                float zDelta = static_cast<float>(static_cast<int16_t>(HIWORD(wParam))) / static_cast<float>(WHEEL_DELTA);

                short keyStates = static_cast<short>(LOWORD(wParam));

                bool lButton = (keyStates & MK_LBUTTON) != 0;
                bool rButton = (keyStates & MK_RBUTTON) != 0;
                bool mButton = (keyStates & MK_MBUTTON) != 0;
                bool shift   = (keyStates & MK_SHIFT)   != 0;
                bool control = (keyStates & MK_CONTROL) != 0;

                int x = static_cast<int>(SHORT(LOWORD(lParam)));
                int y = static_cast<int>(SHORT(HIWORD(lParam)));

                // Convert screen → client coords
                POINT pt{ x, y };
                ScreenToClient(hwnd, &pt);

                MouseWheelEventArgs args(
                    zDelta, 
                    lButton, 
                    mButton, 
                    rButton, 
                    control, 
                    shift, 
                    pt.x, 
                    pt.y
                );
                
                app->onMouseWheel(args);
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}