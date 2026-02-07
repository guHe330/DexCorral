// CorralHook.cpp - Minimal Explorer hook for hiding desktop icons
// Reads a list of icon names to hide from a memory-mapped file written by DexCorral.exe
// Icons in the list are skipped during NM_CUSTOMDRAW (CDRF_SKIPDEFAULT)

#include "CorralHook.h"
#include <CommCtrl.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

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
static WNDPROC g_OriginalListViewProc = nullptr;
static bool g_HookActive = false;

// Shared memory - opened once, kept mapped
static HANDLE g_hMapFile = nullptr;
static const BYTE* g_pMappedData = nullptr;

// Cached hidden icon list - only rebuilt when version changes
static std::vector<std::wstring> g_HiddenIcons;
static DWORD g_LastVersion = 0;

// ============================================================================
// Debug logging - enabled by DexCorral.exe -debug flag via named event
// ============================================================================

static const wchar_t* DEBUG_EVENT_NAME = L"Local\\DexCorralDebug";
static bool g_DebugEnabled = false;
static bool g_DebugChecked = false;

static bool IsDebugEnabled() {
    if (!g_DebugChecked) {
        HANDLE hEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, DEBUG_EVENT_NAME);
        if (hEvent) {
            g_DebugEnabled = true;
            CloseHandle(hEvent);
        }
        g_DebugChecked = true;
    }
    return g_DebugEnabled;
}

static void Log(const wchar_t* format, ...) {
    if (!IsDebugEnabled()) return;

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

// Forward declarations
static void MoveHiddenIconsOffScreenInProc();

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
        Log(L"RefreshHiddenIconCache: No mapped data, trying to open...");
        EnsureMappingOpen();
        if (!g_pMappedData) {
            // No shared memory available - clear the list
            Log(L"RefreshHiddenIconCache: Failed to open shared memory");
            if (!g_HiddenIcons.empty()) {
                g_HiddenIcons.clear();
                g_LastVersion = 0;
            }
            return;
        }
        Log(L"RefreshHiddenIconCache: Shared memory opened successfully");
    }

    // Check version - skip rebuild if unchanged (zero cost path)
    DWORD currentVersion = *(const DWORD*)g_pMappedData;
    if (currentVersion == g_LastVersion) return;

    Log(L"RefreshHiddenIconCache: Version changed %u -> %u", g_LastVersion, currentVersion);
    g_LastVersion = currentVersion;

    // Version changed - rebuild the list
    g_HiddenIcons.clear();
    const wchar_t* p = (const wchar_t*)(g_pMappedData + sizeof(DWORD));
    while (*p) {
        Log(L"RefreshHiddenIconCache: Hidden icon: '%s'", p);
        g_HiddenIcons.push_back(p);
        p += wcslen(p) + 1;
    }
    Log(L"RefreshHiddenIconCache: Total %d hidden icons", (int)g_HiddenIcons.size());

    // Move newly-hidden icons off-screen immediately (in-process, reliable)
    MoveHiddenIconsOffScreenInProc();
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

// Helper: get display name for a ListView item index (in-process, fast)
// Uses CallWindowProc to bypass our subclass and avoid any recursion
static bool GetItemDisplayName(int index, wchar_t* buf, int bufSize) {
    if (!g_OriginalListViewProc) return false;
    LVITEMW item = {};
    item.iSubItem = 0;
    item.cchTextMax = bufSize;
    item.pszText = buf;
    CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView, LVM_GETITEMTEXTW, index, (LPARAM)&item);
    return buf[0] != 0;
}

// ============================================================================
// Move hidden icons off-screen (in-process, reliable)
// ============================================================================

// Off-screen position: large positive values (negative gets clamped by Explorer's ListView)
static const int OFFSCREEN_X = 32000;
static const int OFFSCREEN_Y = 32000;

// Actual clamped position detected after first move (Explorer clamps 32000 to desktop edge)
static int g_ClampedX = -1;
static int g_ClampedY = -1;
static const int CLAMP_TOLERANCE = 20;  // Pixels of tolerance for position matching

static bool IsOffScreen(int x, int y) {
    // Check for raw off-screen values (before Explorer clamps them)
    if (x >= 30000 && y >= 30000) return true;
    if (g_ClampedX < 0) {
        // Clamped position not yet detected
        return false;
    }
    // Check if X matches the clamped edge position. We only check X because
    // Explorer auto-arranges stacked icons vertically, so Y varies when
    // multiple hidden icons are at the same off-screen position.
    return (abs(x - g_ClampedX) <= CLAMP_TOLERANCE);
}

static void MoveHiddenIconsOffScreenInProc() {
    Log(L"MoveHiddenIconsOffScreenInProc: Called, ListView=%p, OrigProc=%p, hidden=%d",
        g_hDesktopListView, g_OriginalListViewProc, (int)g_HiddenIcons.size());

    if (!g_hDesktopListView || !g_OriginalListViewProc || g_HiddenIcons.empty()) {
        Log(L"MoveHiddenIconsOffScreenInProc: Early exit (missing prereqs)");
        return;
    }

    int count = (int)CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
        LVM_GETITEMCOUNT, 0, 0);
    Log(L"MoveHiddenIconsOffScreenInProc: ListView has %d items", count);

    int moved = 0;
    for (int i = 0; i < count; i++) {
        wchar_t buf[MAX_PATH] = {};
        if (!GetItemDisplayName(i, buf, MAX_PATH)) continue;

        if (!ShouldHideIcon(buf)) continue;

        // Check if already off-screen
        POINT pt;
        CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
            LVM_GETITEMPOSITION, i, (LPARAM)&pt);

        Log(L"MoveHiddenIconsOffScreenInProc: Item %d '%s' at (%d,%d)", i, buf, pt.x, pt.y);

        if (IsOffScreen(pt.x, pt.y)) {
            Log(L"MoveHiddenIconsOffScreenInProc: Item %d already off-screen, skipping", i);
            continue;
        }

        // Move off-screen using large positive coords (negative gets clamped by Explorer)
        POINT offScreen = { OFFSCREEN_X, OFFSCREEN_Y };
        LRESULT result = CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
            LVM_SETITEMPOSITION32, i, (LPARAM)&offScreen);
        Log(L"MoveHiddenIconsOffScreenInProc: Moved item %d '%s' -> (%d,%d), result=%d",
            i, buf, OFFSCREEN_X, OFFSCREEN_Y, (int)result);

        // Verify the move and detect actual clamped position
        POINT verify;
        CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
            LVM_GETITEMPOSITION, i, (LPARAM)&verify);
        Log(L"MoveHiddenIconsOffScreenInProc: Verify item %d now at (%d,%d)", i, verify.x, verify.y);

        // Store the clamped position so IsOffScreen can detect it later
        if (g_ClampedX < 0 || g_ClampedY < 0) {
            g_ClampedX = verify.x;
            g_ClampedY = verify.y;
            Log(L"MoveHiddenIconsOffScreenInProc: Detected clamped position = (%d,%d)", g_ClampedX, g_ClampedY);
        }

        moved++;
    }

    Log(L"MoveHiddenIconsOffScreenInProc: Done, moved %d icons", moved);
}

// ============================================================================
// Post-sort compaction - close gaps left by hidden icons after sort/arrange
// ============================================================================

static const UINT_PTR COMPACT_TIMER_ID = 99;

static void CompactVisibleIcons() {
    if (!g_hDesktopListView || !g_OriginalListViewProc) return;

    int count = (int)CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
        LVM_GETITEMCOUNT, 0, 0);
    if (count <= 0) return;

    // Get icon spacing from the ListView (FALSE = icon/large icon view, TRUE = small icon view)
    DWORD spacing = (DWORD)CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
        LVM_GETITEMSPACING, FALSE, 0);
    int spacingX = LOWORD(spacing);
    int spacingY = HIWORD(spacing);
    if (spacingX <= 0) spacingX = 75;
    if (spacingY <= 0) spacingY = 75;

    // Get work area (desktop minus taskbar) — this is in screen coords,
    // which matches ListView client coords for the desktop
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    // Collect visible items with their current grid positions
    struct VisibleIcon {
        int index;
        int gridOrder;  // for sorting: col * 10000 + row
    };
    std::vector<VisibleIcon> visibleItems;

    RefreshHiddenIconCache();

    for (int i = 0; i < count; i++) {
        wchar_t buf[MAX_PATH] = {};
        if (!GetItemDisplayName(i, buf, MAX_PATH)) continue;
        if (ShouldHideIcon(buf)) continue;

        // Get current position
        POINT pt;
        CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
            LVM_GETITEMPOSITION, i, (LPARAM)&pt);

        // Calculate grid order (column-major: top-to-bottom, then left-to-right)
        int col = (pt.x - workArea.left + spacingX / 2) / spacingX;
        int row = (pt.y - workArea.top + spacingY / 2) / spacingY;
        if (col < 0) col = 0;
        if (row < 0) row = 0;
        visibleItems.push_back({ i, col * 10000 + row });
    }

    if (visibleItems.empty()) return;

    // Sort by grid position to preserve the sort order Explorer applied
    std::sort(visibleItems.begin(), visibleItems.end(),
        [](const VisibleIcon& a, const VisibleIcon& b) { return a.gridOrder < b.gridOrder; });

    // Calculate compact grid: column-major layout (matches Explorer's default)
    int maxRows = (workArea.bottom - workArea.top) / spacingY;
    if (maxRows < 1) maxRows = 1;

    // Small offset from work area edge (Explorer uses a small margin)
    int startX = workArea.left + spacingX / 2 - 16;
    int startY = workArea.top + 4;

    for (int i = 0; i < (int)visibleItems.size(); i++) {
        int col = i / maxRows;
        int row = i % maxRows;
        POINT newPos = { startX + col * spacingX, startY + row * spacingY };

        CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
            LVM_SETITEMPOSITION32, visibleItems[i].index, (LPARAM)&newPos);
    }

    // Refresh to show new positions
    RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

    Log(L"CompactVisibleIcons: Compacted %d visible icons (grid %dx%d, spacing %dx%d)",
        (int)visibleItems.size(), maxRows, (int)visibleItems.size() / maxRows + 1, spacingX, spacingY);
}

// ============================================================================
// ListView subclass - intercepts LVM_SETITEMPOSITION to protect hidden icons
// ============================================================================

static const UINT_PTR REHIDE_TIMER_ID = 100;

// Cooldown to prevent WM_PAINT from re-triggering compaction immediately after a rehide
static DWORD g_LastRehideTime = 0;
static const DWORD REHIDE_COOLDOWN_MS = 1000;  // 1 second cooldown after rehide+compact

static void RehideAndCompact() {
    // After a sort/arrange, hidden icons may have been moved back on-screen.
    // Move them off-screen again, then compact visible icons.
    Log(L"RehideAndCompact: Re-hiding icons after sort/arrange");
    MoveHiddenIconsOffScreenInProc();
    CompactVisibleIcons();
    g_LastRehideTime = GetTickCount();
}

static LRESULT CALLBACK ListViewSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Handle compaction timer
    if (uMsg == WM_TIMER && wParam == COMPACT_TIMER_ID) {
        KillTimer(hwnd, COMPACT_TIMER_ID);
        Log(L"ListViewSubclassProc: Compaction timer fired");
        CompactVisibleIcons();
        g_LastRehideTime = GetTickCount();
        return 0;
    }

    // Handle re-hide timer (after sort/arrange)
    if (uMsg == WM_TIMER && wParam == REHIDE_TIMER_ID) {
        KillTimer(hwnd, REHIDE_TIMER_ID);
        RehideAndCompact();
        return 0;
    }

    // Detect hidden icons that moved back on-screen (e.g. after Explorer sorts internally).
    // Check on every paint — if any hidden icon is no longer off-screen, schedule re-hide.
    // Skip check during cooldown period after a recent rehide/compact.
    if (uMsg == WM_PAINT && !g_HiddenIcons.empty() &&
        (GetTickCount() - g_LastRehideTime) > REHIDE_COOLDOWN_MS) {
        // Quick check: see if any hidden icon is back on-screen
        int count = (int)CallWindowProcW(g_OriginalListViewProc, hwnd,
            LVM_GETITEMCOUNT, 0, 0);
        bool needRehide = false;
        for (int i = 0; i < count && !needRehide; i++) {
            wchar_t buf[MAX_PATH] = {};
            if (!GetItemDisplayName(i, buf, MAX_PATH)) continue;
            if (!ShouldHideIcon(buf)) continue;
            POINT pt;
            CallWindowProcW(g_OriginalListViewProc, hwnd,
                LVM_GETITEMPOSITION, i, (LPARAM)&pt);
            bool offScreen = IsOffScreen(pt.x, pt.y);
            if (!offScreen) {
                Log(L"WM_PAINT check: '%s' at (%d,%d) NOT off-screen! clamped=(%d,%d) tol=%d",
                    buf, pt.x, pt.y, g_ClampedX, g_ClampedY, CLAMP_TOLERANCE);
                needRehide = true;
            }
        }
        if (needRehide) {
            Log(L"ListViewSubclassProc: Hidden icon found on-screen during paint, scheduling rehide");
            SetTimer(hwnd, REHIDE_TIMER_ID, 50, nullptr);
        }
    }

    if (uMsg == LVM_SETITEMPOSITION || uMsg == LVM_SETITEMPOSITION32) {
        // Check if this item is in the hidden list
        RefreshHiddenIconCache();
        if (!g_HiddenIcons.empty()) {
            int itemIndex = (int)wParam;
            wchar_t buf[MAX_PATH] = {};
            if (GetItemDisplayName(itemIndex, buf, MAX_PATH) && ShouldHideIcon(buf)) {
                // Force hidden icons to stay off-screen
                bool isOurCall = false;  // Is DexCorral setting to off-screen?

                if (uMsg == LVM_SETITEMPOSITION) {
                    int x = (short)LOWORD(lParam);
                    int y = (short)HIWORD(lParam);
                    isOurCall = IsOffScreen(x, y);
                    Log(L"ListViewSubclassProc: SETPOS item %d '%s' to (%d,%d), isOurCall=%d",
                        itemIndex, buf, x, y, isOurCall);
                } else {
                    POINT* pt = (POINT*)lParam;
                    isOurCall = (pt && IsOffScreen(pt->x, pt->y));
                    Log(L"ListViewSubclassProc: SETPOS32 item %d '%s' to (%d,%d), isOurCall=%d",
                        itemIndex, buf, pt ? pt->x : -1, pt ? pt->y : -1, isOurCall);
                }

                if (!isOurCall) {
                    // Someone (Explorer sort/arrange) is trying to move a hidden icon
                    // to a visible position. Block it and schedule compaction.
                    Log(L"ListViewSubclassProc: Blocking move, redirecting to off-screen");
                    SetTimer(hwnd, COMPACT_TIMER_ID, 100, nullptr);

                    if (uMsg == LVM_SETITEMPOSITION) {
                        return CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam,
                            MAKELPARAM(OFFSCREEN_X & 0xFFFF, OFFSCREEN_Y & 0xFFFF));
                    } else {
                        POINT offScreen = { OFFSCREEN_X, OFFSCREEN_Y };
                        return CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam,
                            (LPARAM)&offScreen);
                    }
                }
                // else: our own off-screen call, pass through
            }
        }
    }

    return CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// ShellDefView subclass - hides icons via NM_CUSTOMDRAW
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
                GetItemDisplayName(itemIndex, buf, MAX_PATH);

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

    // Subclass SHELLDLL_DefView for NM_CUSTOMDRAW (icon hiding)
    g_OriginalShellDefViewProc = (WNDPROC)SetWindowLongPtrW(
        g_hShellDefView, GWLP_WNDPROC, (LONG_PTR)ShellDefViewSubclassProc);

    // Subclass SysListView32 for LVM_SETITEMPOSITION (prevent sort/arrange from moving hidden icons)
    g_OriginalListViewProc = (WNDPROC)SetWindowLongPtrW(
        g_hDesktopListView, GWLP_WNDPROC, (LONG_PTR)ListViewSubclassProc);

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
    // Kill timers if pending
    if (g_hDesktopListView) {
        KillTimer(g_hDesktopListView, COMPACT_TIMER_ID);
        KillTimer(g_hDesktopListView, REHIDE_TIMER_ID);
    }

    // Restore ListView subclass first (inner-most subclass)
    if (g_hDesktopListView && g_OriginalListViewProc) {
        SetWindowLongPtrW(g_hDesktopListView, GWLP_WNDPROC, (LONG_PTR)g_OriginalListViewProc);
        g_OriginalListViewProc = nullptr;
    }

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
