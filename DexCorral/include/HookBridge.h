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

// One hidden desktop icon as communicated from the app to the hook.
// displayName is what the ListView shows (no .lnk extension); parsingName is
// the canonical shell identity (full filesystem path, or "::{CLSID}" for
// special shell items). parsingName may be empty if the app could not resolve
// it; the hook then falls back to display-name matching.
struct HiddenIconInfo
{
    std::wstring displayName;
    std::wstring parsingName;
};

inline bool operator==(const HiddenIconInfo &a, const HiddenIconInfo &b)
{
    return a.displayName == b.displayName && a.parsingName == b.parsingName;
}

// Bridge between app worker thread and Explorer hook (both in DexCorralHook.dll).
// Uses a global vector + CRITICAL_SECTION for thread-safe in-process communication.
class HookBridge
{
public:
    // Write the list of icons that should be hidden
    static void UpdateHiddenIcons(const std::vector<HiddenIconInfo> &icons);

    // Clear the hidden icon list (call on shutdown)
    static void ClearHiddenIcons();

    // Force desktop repaint (so hidden icons disappear/reappear)
    static void RefreshDesktop();

    // Scoped marker set by the app around intentional icon repositioning.
    // The hook swallows LVM_SETITEMPOSITION aimed at hidden icons unless this
    // is active — Explorer (auto-arrange, align-to-grid, third-party tools)
    // can never move a corral-owned icon.
    static void BeginAppIconMove();
    static void EndAppIconMove();
    static bool IsAppIconMoveActive();

    // Identity-based icon position requests, written by the app and consumed
    // by the hook on the Explorer UI thread (DexCorral_PositionIconsByPath
    // registered message)
    static void SetIconPositionRequests(const std::vector<IconPositionRequest> &requests);
    static void TakeIconPositionRequests(std::vector<IconPositionRequest> &out);

    // Desktop icon snapshot (identity + position per icon), written by the
    // hook on the Explorer UI thread (DexCorral_GetIconSnapshot registered
    // message) and consumed by the app
    static void SetIconSnapshot(const std::vector<DesktopIconInfo> &icons);
    static void TakeIconSnapshot(std::vector<DesktopIconInfo> &out);

    // --- Called by CorralHook.cpp (Explorer UI thread) ---

    // Returns the current version counter (cheap read, no lock)
    static DWORD GetVersion();

    // Copies the current hidden icon list into `out`. Returns the version.
    static DWORD GetHiddenIcons(std::vector<HiddenIconInfo> &out);

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

    // This DLL's own module handle, set once from DllMain(DLL_PROCESS_ATTACH).
    // App.cpp/CorralWindow.cpp run injected into Explorer, so GetModuleHandleW(nullptr)
    // there resolves to explorer.exe, not this DLL — use this instead to load the
    // icon resources embedded in DexCorralHook.rc.
    static void SetDllModule(HMODULE hModule);
    static HMODULE GetDllModule();
};

// RAII scope for HookBridge::BeginAppIconMove/EndAppIconMove
struct AppIconMoveScope
{
    AppIconMoveScope() { HookBridge::BeginAppIconMove(); }
    ~AppIconMoveScope() { HookBridge::EndAppIconMove(); }
    AppIconMoveScope(const AppIconMoveScope &) = delete;
    AppIconMoveScope &operator=(const AppIconMoveScope &) = delete;
};
