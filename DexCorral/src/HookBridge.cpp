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
 * HookBridge.cpp - In-process communication between app and Explorer hook
 *
 * Both the app (worker thread) and the Explorer hook (UI thread) run inside
 * DexCorralHook.dll in the same process. Communication uses a global vector
 * protected by a CRITICAL_SECTION, with a version counter for zero-cost
 * cache checks on the hook side.
 */

#include "HookBridge.h"
#include "Constants.h"
#include <vector>

static CRITICAL_SECTION s_Lock;
static std::vector<std::wstring> s_HiddenNames;
static volatile DWORD s_Version = 0;
static bool s_Initialized = false;
static bool s_DebugLogging = false;

void HookBridge::Initialize()
{
    if (!s_Initialized)
    {
        InitializeCriticalSection(&s_Lock);
        s_Initialized = true;
    }
}

void HookBridge::Cleanup()
{
    if (s_Initialized)
    {
        DeleteCriticalSection(&s_Lock);
        s_Initialized = false;
    }
}

void HookBridge::UpdateHiddenIcons(const std::vector<std::wstring> &displayNames)
{
    EnterCriticalSection(&s_Lock);
    s_HiddenNames = displayNames;
    s_Version++;
    LeaveCriticalSection(&s_Lock);
}

void HookBridge::ClearHiddenIcons()
{
    EnterCriticalSection(&s_Lock);
    s_HiddenNames.clear();
    s_Version++;
    LeaveCriticalSection(&s_Lock);
}

DWORD HookBridge::GetVersion()
{
    return s_Version;
}

DWORD HookBridge::GetHiddenIconNames(std::vector<std::wstring> &out)
{
    EnterCriticalSection(&s_Lock);
    out = s_HiddenNames;
    DWORD ver = s_Version;
    LeaveCriticalSection(&s_Lock);
    return ver;
}

void HookBridge::SetDebugLogging(bool enable)
{
    s_DebugLogging = enable;
}

bool HookBridge::IsDebugLogging()
{
    return s_DebugLogging;
}

void HookBridge::RefreshDesktop()
{
    HWND hListView = DesktopIcons::GetDesktopListView();
    if (hListView)
    {
        RedrawWindow(hListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}
