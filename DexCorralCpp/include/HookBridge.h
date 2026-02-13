#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include "DesktopIcons.h"

// Bridge between DexCorral.exe and DexCorralHook.dll
// Writes hidden icon list to shared memory, repositions hidden icons under corrals
class HookBridge {
public:
    // Write the list of icon display names that should be hidden
    static void UpdateHiddenIcons(const std::vector<std::wstring>& displayNames);

    // Clear the hidden icon list (call on eject)
    static void ClearHiddenIcons();

    // Force desktop repaint (so hidden icons disappear/reappear)
    static void RefreshDesktop();

private:
    static HANDLE s_hMapFile;
};
