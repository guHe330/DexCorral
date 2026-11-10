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

MouseHook::MouseHook() : hookHandle(nullptr), hookThreadId(0)
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
    if (hookThread.joinable())
        return;

    // A low-level hook is called on the thread that installed it, through that
    // thread's message loop, and every mouse event in the system waits for the
    // answer. So it gets a thread that does nothing else: on the app thread, a
    // slow config save or a reload of a big folder held up the whole mouse, and
    // Windows silently removes a hook that keeps answering late.
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    hookThread = std::thread([this, ready]()
                             {
        MSG msg;
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE); // create the queue for Stop()
        hookThreadId = GetCurrentThreadId();
        hookHandle = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
        if (ready)
            SetEvent(ready);

        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (hookHandle != nullptr)
        {
            UnhookWindowsHookEx(hookHandle);
            hookHandle = nullptr;
        } });

    if (ready)
    {
        WaitForSingleObject(ready, 2000);
        CloseHandle(ready);
    }
}

void MouseHook::Stop()
{
    if (!hookThread.joinable())
        return;
    PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0);
    hookThread.join();
    hookThreadId = 0;
}

void MouseHook::SetLeftButtonDownCallback(MouseEventCallback callback)
{
    leftButtonDownCallback = callback;
}

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
                    // Posted, not sent: the corral scrolls and repaints on its
                    // own thread while the mouse carries on.
                    int delta = (short)HIWORD(mouseInfo->mouseData);
                    PostMessageW(hwndUnder, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(pt.x, pt.y));
                    return 1;
                }
            }
            break;
        }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
