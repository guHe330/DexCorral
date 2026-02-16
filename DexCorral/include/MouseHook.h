#pragma once
#include <Windows.h>
#include <functional>

class MouseHook {
public:
    using MouseEventCallback = std::function<void(POINT)>;
    using MouseWheelCallback = std::function<void(POINT, int delta)>;

    MouseHook();
    ~MouseHook();

    void Start();
    void Stop();

    void SetLeftButtonDownCallback(MouseEventCallback callback);
    void SetLeftButtonUpCallback(MouseEventCallback callback);
    void SetMouseMoveCallback(MouseEventCallback callback);
    // Mousewheel is now handled directly in the hook, not via callback
    // void SetMouseWheelCallback(MouseWheelCallback callback);

private:
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static MouseHook* instance;

    HHOOK hookHandle;
    MouseEventCallback leftButtonDownCallback;
    MouseEventCallback leftButtonUpCallback;
    MouseEventCallback mouseMoveCallback;
    // Mousewheel handled directly in hook
    // MouseWheelCallback mouseWheelCallback;
};
