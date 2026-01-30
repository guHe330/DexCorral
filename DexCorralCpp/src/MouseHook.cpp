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
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
