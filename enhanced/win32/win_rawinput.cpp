#include "win_rawinput.h"

#include <algorithm>
#include <climits>
#include <cstdint>

namespace
{
constexpr char windowClass[] = "BlackOpsCommunityRawMouse";

struct RawMouseThread
{
    HANDLE ready = nullptr;
    HANDLE thread = nullptr;
    HWND window = nullptr;
    bool registered = false;
};

RawMouseThread worker;
SRWLOCK mouseLock = SRWLOCK_INIT;
HWND gameWindow = nullptr;
std::int64_t mouseX = 0;
std::int64_t mouseY = 0;

LRESULT CALLBACK RawMouseWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CLOSE)
    {
        PostQuitMessage(0);
        return 0;
    }

    if (message == WM_INPUT)
    {
        RAWINPUT input{};
        UINT size = sizeof(input);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &input, &size,
                            sizeof(RAWINPUTHEADER)) == sizeof(input) &&
            input.header.dwType == RIM_TYPEMOUSE &&
            !(input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
        {
            const HWND foreground = GetForegroundWindow();
            AcquireSRWLockExclusive(&mouseLock);
            if (gameWindow && foreground == gameWindow)
            {
                mouseX += input.data.mouse.lLastX;
                mouseY += input.data.mouse.lLastY;
            }
            ReleaseSRWLockExclusive(&mouseLock);
        }
    }

    return DefWindowProcA(window, message, wParam, lParam);
}

DWORD WINAPI RawMouseThreadMain(void *)
{
    WNDCLASSA windowInfo{};
    windowInfo.hInstance = GetModuleHandleA(nullptr);
    windowInfo.lpfnWndProc = RawMouseWindowProc;
    windowInfo.lpszClassName = windowClass;
    const ATOM windowAtom = RegisterClassA(&windowInfo);
    if (windowAtom)
    {
        worker.window = CreateWindowExA(0, windowClass, "", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                       nullptr, windowInfo.hInstance, nullptr);
    }

    RAWINPUTDEVICE mouse{1, 2, RIDEV_INPUTSINK, worker.window};
    worker.registered = worker.window && RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
    SetEvent(worker.ready);

    if (worker.registered)
    {
        MSG message;
        while (GetMessageA(&message, nullptr, 0, 0) > 0)
            DispatchMessageA(&message);

        mouse.dwFlags = RIDEV_REMOVE;
        mouse.hwndTarget = nullptr;
        RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
    }

    if (worker.window)
        DestroyWindow(worker.window);
    if (windowAtom)
        UnregisterClassA(windowClass, windowInfo.hInstance);
    return 0;
}

int TakeAxis(std::int64_t &counts)
{
    const auto delta = std::clamp<std::int64_t>(counts, -INT_MAX, INT_MAX);
    counts -= delta;
    return static_cast<int>(delta);
}
}

bool BO1_StartRawMouse()
{
    if (worker.thread)
        return true;

    worker.ready = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!worker.ready)
        return false;

    worker.thread = CreateThread(nullptr, 0, RawMouseThreadMain, nullptr, 0, nullptr);
    if (worker.thread)
        WaitForSingleObject(worker.ready, INFINITE);
    CloseHandle(worker.ready);
    worker.ready = nullptr;

    if (worker.registered)
        return true;

    BO1_StopRawMouse();
    return false;
}

void BO1_StopRawMouse()
{
    BO1_SetRawMouseWindow(nullptr);
    if (worker.thread)
    {
        if (worker.registered)
            PostMessageA(worker.window, WM_CLOSE, 0, 0);
        WaitForSingleObject(worker.thread, INFINITE);
        CloseHandle(worker.thread);
    }
    worker = {};
}

void BO1_SetRawMouseWindow(HWND window)
{
    AcquireSRWLockExclusive(&mouseLock);
    if (gameWindow != window)
    {
        gameWindow = window;
        mouseX = 0;
        mouseY = 0;
    }
    ReleaseSRWLockExclusive(&mouseLock);
}

void BO1_TakeRawMouse(int &dx, int &dy)
{
    AcquireSRWLockExclusive(&mouseLock);
    dx = TakeAxis(mouseX);
    dy = TakeAxis(mouseY);
    ReleaseSRWLockExclusive(&mouseLock);
}
