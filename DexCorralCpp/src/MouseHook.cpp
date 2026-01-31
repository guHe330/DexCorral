#include "MouseHook.h"

MouseHook* MouseHook::instance = nullptr;

MouseHook::MouseHook() : hookHandle(nullptr) {
    instance = this;
}

MouseHook::~MouseHook() {
    Stop();
    instance = nullptr;
}

void MouseHook::Start() {
    if (hookHandle == nullptr) {
        hookHandle = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
    }
}

void MouseHook::Stop() {
    if (hookHandle != nullptr) {
        UnhookWindowsHookEx(hookHandle);
        hookHandle = nullptr;
    }
}

void MouseHook::SetLeftButtonDownCallback(MouseEventCallback callback) {
    leftButtonDownCallback = callback;
}

void MouseHook::SetLeftButtonUpCallback(MouseEventCallback callback) {
    leftButtonUpCallback = callback;
}

void MouseHook::SetMouseMoveCallback(MouseEventCallback callback) {
    mouseMoveCallback = callback;
}

// Mousewheel is now handled directly in the hook proc, not via callback
// void MouseHook::SetMouseWheelCallback(MouseWheelCallback callback) {
//     mouseWheelCallback = callback;
// }

LRESULT CALLBACK MouseHook::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && instance != nullptr) {
        MSLLHOOKSTRUCT* mouseInfo = (MSLLHOOKSTRUCT*)lParam;
        POINT pt = mouseInfo->pt;

        switch (wParam) {
        case WM_LBUTTONDOWN:
            if (instance->leftButtonDownCallback) {
                instance->leftButtonDownCallback(pt);
            }
            break;
        case WM_LBUTTONUP:
            if (instance->leftButtonUpCallback) {
                instance->leftButtonUpCallback(pt);
            }
            break;
        case WM_MOUSEMOVE:
            if (instance->mouseMoveCallback) {
                instance->mouseMoveCallback(pt);
            }
            break;
        case WM_MOUSEWHEEL: {
            // For tool windows, we need to manually route the message to the window under the cursor
            // Get the window under the cursor
            HWND hwndUnder = WindowFromPoint(pt);
            if (hwndUnder) {
                // Send the mousewheel message directly to that window
                int delta = (short)HIWORD(mouseInfo->mouseData);
                // Forward with proper wParam/lParam format for WM_MOUSEWHEEL
                SendMessageW(hwndUnder, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(pt.x, pt.y));
                // Consume the event to prevent default routing (which wouldn't work for tool windows anyway)
                return 1;
            }
            break;
        }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
