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

#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include "DesktopIcons.h"

// Bridge between app worker thread and Explorer hook (both in DexCorralHook.dll).
// Uses a global vector + CRITICAL_SECTION for thread-safe in-process communication.
class HookBridge
{
public:
    // Write the list of icon display names that should be hidden
    static void UpdateHiddenIcons(const std::vector<std::wstring> &displayNames);

    // Clear the hidden icon list (call on shutdown)
    static void ClearHiddenIcons();

    // Force desktop repaint (so hidden icons disappear/reappear)
    static void RefreshDesktop();

    // --- Called by CorralHook.cpp (Explorer UI thread) ---

    // Returns the current version counter (cheap read, no lock)
    static DWORD GetVersion();

    // Copies the current hidden icon list into `out`. Returns the version.
    static DWORD GetHiddenIconNames(std::vector<std::wstring> &out);

    // Initialize/cleanup the critical section (called from DllMain)
    static void Initialize();
    static void Cleanup();

    // Register the app's message window so the hook can post notifications to it.
    // Called by App::Initialize() immediately after creating the message window.
    static void SetAppMessageWindow(HWND hwnd);
    static HWND GetAppMessageWindow();

    // Debug logging flag — set from config after load, read by dllmain/CorralHook Log()
    static void SetDebugLogging(bool enable);
    static bool IsDebugLogging();
};
