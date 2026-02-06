// CorralHook.cpp - Minimal Explorer hook for hiding desktop icons
// Reads a list of icon names to hide from a memory-mapped file written by DexCorral.exe
// Icons in the list are skipped during NM_CUSTOMDRAW (CDRF_SKIPDEFAULT)

#include "CorralHook.h"
#include <CommCtrl.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")

// Shared memory name - must match HookBridge in main app
static const wchar_t* SHARED_MEMORY_NAME = L"Local\\DexCorralHiddenIcons";

// Shared memory layout:
//   [0..3]   DWORD version  - incremented by main app on every update
//   [4..N]   wchar_t[]      - null-separated display names, double-null terminated

// Global state
static HWND g_hDesktopListView = nullptr;
static HWND g_hShellDefView = nullptr;
static WNDPROC g_OriginalShellDefViewProc = nullptr;
static bool g_HookActive = false;

// Shared memory - opened once, kept mapped
static HANDLE g_hMapFile = nullptr;
static const BYTE* g_pMappedData = nullptr;

// Cached hidden icon list - only rebuilt when version changes
static std::vector<std::wstring> g_HiddenIcons;
static DWORD g_LastVersion = 0;

// ============================================================================
// Log helper
// ============================================================================

static void Log(const wchar_t* format, ...) {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"CorralHook.log");

    va_list args;
    va_start(args, format);
    wchar_t message[512];
    vswprintf_s(message, format, args);
    va_end(args);

    HANDLE hFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buffer[600];
        swprintf_s(buffer, L"[%02d:%02d:%02d.%03d] %s\r\n",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
        DWORD written;
        WriteFile(hFile, buffer, (DWORD)(wcslen(buffer) * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(hFile);
    }
}

// ============================================================================
// Shared memory reading - zero kernel calls if data hasn't changed
// ============================================================================

static void EnsureMappingOpen() {
    if (g_hMapFile) return;

    g_hMapFile = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEMORY_NAME);
    if (g_hMapFile) {
        g_pMappedData = (const BYTE*)MapViewOfFile(g_hMapFile, FILE_MAP_READ, 0, 0, 0);
        if (!g_pMappedData) {
            CloseHandle(g_hMapFile);
            g_hMapFile = nullptr;
        }
    }
}

static void RefreshHiddenIconCache() {
    // Try to open mapping if not yet open (main app may have (re)created it)
    if (!g_pMappedData) {
        EnsureMappingOpen();
        if (!g_pMappedData) {
            // No shared memory available - clear the list
            if (!g_HiddenIcons.empty()) {
                g_HiddenIcons.clear();
                g_LastVersion = 0;
            }
            return;
        }
    }

    // Check version - skip rebuild if unchanged (zero cost path)
    DWORD currentVersion = *(const DWORD*)g_pMappedData;
    if (currentVersion == g_LastVersion) return;
    g_LastVersion = currentVersion;

    // Version changed - rebuild the list
    g_HiddenIcons.clear();
    const wchar_t* p = (const wchar_t*)(g_pMappedData + sizeof(DWORD));
    while (*p) {
        g_HiddenIcons.push_back(p);
        p += wcslen(p) + 1;
    }
}

static bool ShouldHideIcon(const std::wstring& name) {
    for (const auto& hidden : g_HiddenIcons) {
        if (_wcsicmp(hidden.c_str(), name.c_str()) == 0) return true;
    }
    return false;
}

// ============================================================================
// Desktop ListView discovery
// ============================================================================

static HWND FindDesktopListView(HWND* outShellDefView) {
    *outShellDefView = nullptr;

    HWND progman = FindWindowW(L"Progman", nullptr);
    HWND shellDll = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);

    if (!shellDll) {
        HWND workerW = nullptr;
        do {
            workerW = FindWindowExW(nullptr, workerW, L"WorkerW", nullptr);
            if (workerW) {
                shellDll = FindWindowExW(workerW, nullptr, L"SHELLDLL_DefView", nullptr);
                if (shellDll) break;
            }
        } while (workerW);
    }

    if (!shellDll) return nullptr;
    *outShellDefView = shellDll;
    return FindWindowExW(shellDll, nullptr, L"SysListView32", nullptr);
}

// ============================================================================
// Subclass procedure - the core of the hook
// ============================================================================

static LRESULT CALLBACK ShellDefViewSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_NOTIFY) {
        NMHDR* nmhdr = (NMHDR*)lParam;
        if (nmhdr->code == NM_CUSTOMDRAW && nmhdr->hwndFrom == g_hDesktopListView) {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lParam;

            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;

            case CDDS_ITEMPREPAINT: {
                // Check version counter - no kernel calls if unchanged
                RefreshHiddenIconCache();

                if (g_HiddenIcons.empty()) return CDRF_DODEFAULT;

                // Get this icon's display name
                int itemIndex = (int)cd->nmcd.dwItemSpec;
                wchar_t buf[MAX_PATH] = {};
                LVITEMW item = {};
                item.iSubItem = 0;
                item.cchTextMax = MAX_PATH;
                item.pszText = buf;
                SendMessageW(g_hDesktopListView, LVM_GETITEMTEXTW, itemIndex, (LPARAM)&item);

                if (buf[0] && ShouldHideIcon(buf)) {
                    return CDRF_SKIPDEFAULT;
                }
                return CDRF_DODEFAULT;
            }
            }
        }
    }

    return CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Initialization & Cleanup
// ============================================================================

bool InitializeCorralHook() {
    HWND shellDefView = nullptr;
    g_hDesktopListView = FindDesktopListView(&shellDefView);

    if (!g_hDesktopListView || !shellDefView) {
        Log(L"InitializeCorralHook: Desktop ListView not found");
        return false;
    }

    g_hShellDefView = shellDefView;
    Log(L"InitializeCorralHook: Found ListView=%p, ShellDefView=%p",
        g_hDesktopListView, g_hShellDefView);

    // Subclass SHELLDLL_DefView for NM_CUSTOMDRAW
    g_OriginalShellDefViewProc = (WNDPROC)SetWindowLongPtrW(
        g_hShellDefView, GWLP_WNDPROC, (LONG_PTR)ShellDefViewSubclassProc);

    g_HookActive = true;

    // Open shared memory and do initial read
    EnsureMappingOpen();
    g_LastVersion = 0;  // Force re-read
    RefreshHiddenIconCache();

    // Trigger repaint so hidden icons disappear
    RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

    Log(L"InitializeCorralHook: Hook active, %d icons to hide", (int)g_HiddenIcons.size());
    return true;
}

void CleanupCorralHook() {
    if (g_hShellDefView && g_OriginalShellDefViewProc) {
        SetWindowLongPtrW(g_hShellDefView, GWLP_WNDPROC, (LONG_PTR)g_OriginalShellDefViewProc);
        g_OriginalShellDefViewProc = nullptr;
    }

    g_HookActive = false;
    g_HiddenIcons.clear();
    g_LastVersion = 0;

    // Close shared memory mapping
    if (g_pMappedData) {
        UnmapViewOfFile(g_pMappedData);
        g_pMappedData = nullptr;
    }
    if (g_hMapFile) {
        CloseHandle(g_hMapFile);
        g_hMapFile = nullptr;
    }

    // Refresh desktop to show all icons again
    if (g_hDesktopListView) {
        RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    Log(L"CleanupCorralHook: Hook cleaned up");
}

bool IsCorralHookActive() { return g_HookActive; }
