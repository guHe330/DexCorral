/**
 * DexCorral - a free and open source Windows desktop icon organizer
 * Copyright (C) 2026 Gunter Heiss
 *
 * For more information see: https://dexcorral.com
 * The DexCorral project is hosted on GitHub: https://github.com/guHe330/DexCorral
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * MouseHook.cpp - Global mouse event monitoring
 *
 * Implements a global mouse hook to capture mouse wheel events and other input
 * for the application. Provides singleton mouse hook management with thread-safe
 * event notification.
 */

#include "MouseHook.h"

MouseHook *MouseHook::instance = nullptr;

MouseHook::MouseHook() : hookHandle(nullptr)
{
    instance = this;
}

MouseHook::~MouseHook()
{
    Stop();
    instance = nullptr;
}

void MouseHook::Start()
{
    if (hookHandle == nullptr)
    {
        hookHandle = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
    }
}

void MouseHook::Stop()
{
    if (hookHandle != nullptr)
    {
        UnhookWindowsHookEx(hookHandle);
        hookHandle = nullptr;
    }
}

void MouseHook::SetLeftButtonDownCallback(MouseEventCallback callback)
{
    leftButtonDownCallback = callback;
}

void MouseHook::SetLeftButtonUpCallback(MouseEventCallback callback)
{
    leftButtonUpCallback = callback;
}

void MouseHook::SetMouseMoveCallback(MouseEventCallback callback)
{
    mouseMoveCallback = callback;
}

/// Dead code: Mousewheel callback mechanism was replaced with direct handling in MouseProc.
/// The hook proc now directly routes WM_MOUSEWHEEL to the window under the cursor,
/// making the callback approach unnecessary. Kept for historical reference.
// void MouseHook::SetMouseWheelCallback(MouseWheelCallback callback) {
//     mouseWheelCallback = callback;
// }

LRESULT CALLBACK MouseHook::MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && instance != nullptr)
    {
        MSLLHOOKSTRUCT *mouseInfo = (MSLLHOOKSTRUCT *)lParam;
        POINT pt = mouseInfo->pt;

        switch (wParam)
        {
        case WM_LBUTTONDOWN:
            if (instance->leftButtonDownCallback)
            {
                instance->leftButtonDownCallback(pt);
            }
            break;
        case WM_LBUTTONUP:
            if (instance->leftButtonUpCallback)
            {
                instance->leftButtonUpCallback(pt);
            }
            break;
        case WM_MOUSEMOVE:
            if (instance->mouseMoveCallback)
            {
                instance->mouseMoveCallback(pt);
            }
            break;
        case WM_MOUSEWHEEL:
        {
            // Only re-route scroll to DexCorral tool windows — they don't receive
            // WM_MOUSEWHEEL naturally because of WS_EX_TOOLWINDOW style.
            // For all other windows, let the message pass through normally.
            HWND hwndUnder = WindowFromPoint(pt);
            if (hwndUnder)
            {
                wchar_t className[64] = {};
                GetClassNameW(hwndUnder, className, _countof(className));
                if (wcscmp(className, L"DexCorralWindowClass") == 0)
                {
                    int delta = (short)HIWORD(mouseInfo->mouseData);
                    SendMessageW(hwndUnder, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(pt.x, pt.y));
                    return 1;
                }
            }
            break;
        }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
