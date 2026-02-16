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

void HookBridge::Initialize() {
    if (!s_Initialized) {
        InitializeCriticalSection(&s_Lock);
        s_Initialized = true;
    }
}

void HookBridge::Cleanup() {
    if (s_Initialized) {
        DeleteCriticalSection(&s_Lock);
        s_Initialized = false;
    }
}

void HookBridge::UpdateHiddenIcons(const std::vector<std::wstring>& displayNames) {
    EnterCriticalSection(&s_Lock);
    s_HiddenNames = displayNames;
    s_Version++;
    LeaveCriticalSection(&s_Lock);
}

void HookBridge::ClearHiddenIcons() {
    EnterCriticalSection(&s_Lock);
    s_HiddenNames.clear();
    s_Version++;
    LeaveCriticalSection(&s_Lock);
}

DWORD HookBridge::GetVersion() {
    return s_Version;
}

DWORD HookBridge::GetHiddenIconNames(std::vector<std::wstring>& out) {
    EnterCriticalSection(&s_Lock);
    out = s_HiddenNames;
    DWORD ver = s_Version;
    LeaveCriticalSection(&s_Lock);
    return ver;
}

void HookBridge::RefreshDesktop() {
    HWND hListView = DesktopIcons::GetDesktopListView();
    if (hListView) {
        RedrawWindow(hListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}
