#include <string>

#include <system/window.hpp>

#include <system/logger.hpp>

bool window::init(HINSTANCE hInstance, int nCmdShow, uint width, uint height)
{
    screenWidth = width;
    screenHeight = height;

    windowTitle = "TC renderer";

    std::wstring wide_string = std::wstring(windowTitle.begin(), windowTitle.end());

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"Window";
    ATOM atom = RegisterClassEx(&windowClass);

    TC_CONDITIONB(atom != 0, "Failed to register class")

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    TC_CONDITIONB(AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE), "The width and height is not proper number");

    // Create the window and store a handle to it.
    hWindow = CreateWindow(
        windowClass.lpszClassName,
        wide_string.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        NULL);

    TC_CONDITIONB(hWindow != nullptr, "Failed to create Window")

    ShowWindow(hWindow, nCmdShow);

	return true;
}

void window::run()
{
}

void window::close()
{
}

LRESULT CALLBACK window::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    //if (gui::guiInputHandle(hWnd, message, wParam, lParam)) return true;

    //switch (message)
    //{
    //case WM_CREATE:
    //{
    //    // Save the DXSample* passed in to CreateWindow.
    //    LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
    //    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    //}
    //return 0;

    //case WM_KEYDOWN:
    //    input::keyEvent(static_cast<int>(wParam), true);
    //    return 0;

    //case WM_KEYUP:
    //    input::keyEvent(static_cast<int>(wParam), false);
    //    return 0;

    //case WM_MOUSEMOVE:
    //    //input::mouseEvent(((int)(short)LOWORD(lParam)), window::get_instance()->height() - ((int)(short)HIWORD(lParam)));
    //    return 0;

    //case WM_PAINT:

    //    return 0;

    //case WM_LBUTTONDOWN:
    //    input::keyEvent(VK_LBUTTON, true);
    //    return 0;
    //case WM_RBUTTONDOWN:
    //    input::keyEvent(VK_RBUTTON, true);
    //    return 0;
    //case WM_LBUTTONUP:
    //    input::keyEvent(VK_LBUTTON, false);
    //    return 0;
    //case WM_RBUTTONUP:
    //    input::keyEvent(VK_RBUTTON, false);
    //    return 0;

    //case WM_DESTROY:
    //    PostQuitMessage(0);
    //    return 0;
    //}

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
