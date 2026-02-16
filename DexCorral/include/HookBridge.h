#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include "DesktopIcons.h"

// Bridge between app worker thread and Explorer hook (both in DexCorralHook.dll).
// Uses a global vector + CRITICAL_SECTION for thread-safe in-process communication.
class HookBridge {
public:
    // Write the list of icon display names that should be hidden
    static void UpdateHiddenIcons(const std::vector<std::wstring>& displayNames);

    // Clear the hidden icon list (call on shutdown)
    static void ClearHiddenIcons();

    // Force desktop repaint (so hidden icons disappear/reappear)
    static void RefreshDesktop();

    // --- Called by CorralHook.cpp (Explorer UI thread) ---

    // Returns the current version counter (cheap read, no lock)
    static DWORD GetVersion();

    // Copies the current hidden icon list into `out`. Returns the version.
    static DWORD GetHiddenIconNames(std::vector<std::wstring>& out);

    // Initialize/cleanup the critical section (called from DllMain)
    static void Initialize();
    static void Cleanup();
};
