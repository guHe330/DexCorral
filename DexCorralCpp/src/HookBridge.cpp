/**
 * HookBridge.cpp - IPC communication with Explorer hook DLL
 *
 * Implements inter-process communication with the CorralHook.dll that is injected
 * into explorer.exe. Uses shared memory with version counter for zero-cost cache checks,
 * allowing the hook to efficiently determine when the list of hidden icons has changed
 * without polling or event notifications.
 */

#include "HookBridge.h"
#include "Constants.h"
#include <vector>

/// Shared memory name - must match DexCorralHook.dll
static const wchar_t* SHARED_MEMORY_NAME = L"Local\\DexCorralHiddenIcons";

// Shared memory layout:
//   [0..3]   DWORD version  - incremented on every update
//   [4..N]   wchar_t[]      - null-separated display names, double-null terminated

HANDLE HookBridge::s_hMapFile = nullptr;
static DWORD s_Version = 0;

void HookBridge::UpdateHiddenIcons(const std::vector<std::wstring>& displayNames) {
    // Create mapping if not yet created
    if (!s_hMapFile) {
        s_hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, HOOK_SHARED_MEMORY_SIZE, SHARED_MEMORY_NAME);
        if (!s_hMapFile) return;
    }

    BYTE* data = (BYTE*)MapViewOfFile(s_hMapFile, FILE_MAP_WRITE, 0, 0, 0);
    if (!data) return;

    // Increment version
    s_Version++;
    *(DWORD*)data = s_Version;

    // Write names after the version header
    wchar_t* p = (wchar_t*)(data + sizeof(DWORD));
    DWORD remaining = (HOOK_SHARED_MEMORY_SIZE - sizeof(DWORD)) / sizeof(wchar_t);

    for (const auto& name : displayNames) {
        DWORD len = (DWORD)name.length() + 1;
        if (len + 1 > remaining) break;
        wcscpy_s(p, remaining, name.c_str());
        p += len;
        remaining -= len;
    }
    *p = L'\0';  // Double-null terminator

    UnmapViewOfFile(data);
}

void HookBridge::ClearHiddenIcons() {
    if (s_hMapFile) {
        BYTE* data = (BYTE*)MapViewOfFile(s_hMapFile, FILE_MAP_WRITE, 0, 0, 0);
        if (data) {
            s_Version++;
            *(DWORD*)data = s_Version;
            wchar_t* p = (wchar_t*)(data + sizeof(DWORD));
            *p = L'\0';
            UnmapViewOfFile(data);
        }
        CloseHandle(s_hMapFile);
        s_hMapFile = nullptr;
    }
}

void HookBridge::RefreshDesktop() {
    HWND hListView = DesktopIcons::GetDesktopListView();
    if (hListView) {
        RedrawWindow(hListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}
