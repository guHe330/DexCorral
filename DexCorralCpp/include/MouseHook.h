#pragma once
#include <Windows.h>
#include <functional>

class MouseHook {
public:
    using MouseEventCallback = std::function<void(POINT)>;

    MouseHook();
    ~MouseHook();

    void Start();
    void Stop();

    void SetLeftButtonDownCallback(MouseEventCallback callback);
    void SetLeftButtonUpCallback(MouseEventCallback callback);
    void SetMouseMoveCallback(MouseEventCallback callback);

private:
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static MouseHook* instance;

    HHOOK hookHandle;
    MouseEventCallback leftButtonDownCallback;
    MouseEventCallback leftButtonUpCallback;
    MouseEventCallback mouseMoveCallback;
};
