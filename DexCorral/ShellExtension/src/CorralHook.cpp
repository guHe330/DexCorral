/**
 * CorralHook.cpp - Explorer.exe hook for hiding desktop icons
 *
 * Subclasses the desktop ListView to hide icons owned by corrals.
 * Uses pure draw suppression (CDRF_SKIPDEFAULT) for invisible icons,
 * input filtering to prevent interaction with hidden icons, and
 * in-process communication with the app worker thread via HookBridge.
 */

#include "CorralHook.h"
#include "HookBridge.h"
#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <ShlObj.h>
#include <oleidl.h>
#include <ole2.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")

// Global state
static HWND g_hDesktopListView = nullptr;
static HWND g_hShellDefView = nullptr;
static WNDPROC g_OriginalShellDefViewProc = nullptr;
static WNDPROC g_OriginalListViewProc = nullptr;
static bool g_HookActive = false;

// Cached hidden icon list - only rebuilt when version changes
static std::vector<std::wstring> g_HiddenIcons;
static DWORD g_LastVersion = 0;

// Auto-sort popup state
static const UINT_PTR AUTOSORT_TIMER_ID = 9901;
static const UINT_PTR POPUP_DISMISS_TIMER_ID = 9902;
static const UINT_PTR SORT_FIXUP_TIMER_ID = 9903;
static const DWORD AUTOSORT_DEBOUNCE_MS = 200;
static const DWORD POPUP_DISMISS_MS = 5000;
static const DWORD SORT_FIXUP_DELAY_MS = 300;  // Debounce delay after last Explorer position change
static HWND g_hAutoSortPopup = nullptr;
static int g_PendingMoveIconIndex = -1;
static POINT g_PendingMoveDropPos = {};     // Where user wanted the icon
static bool g_AutoSortTimerActive = false;
static bool g_InsideCustomSort = false;     // Re-entrancy guard
static bool g_CompactionPending = false;    // True while waiting for Explorer to finish sorting
static bool g_OurAutoArrange = false;       // Our own auto-arrange state (Explorer's is always OFF)
static bool g_AllowAutoArrangeStyle = false; // Temporarily allow LVS_AUTOARRANGE for sort commands

// ============================================================================
// Debug logging - always on, writes to %APPDATA%/DexCorral/CorralHook.log
// ============================================================================

static wchar_t g_LogPath[MAX_PATH] = {};

static void InitLogPath() {
    if (g_LogPath[0]) return;
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        // Ensure DexCorral directory exists
        wchar_t dir[MAX_PATH];
        swprintf_s(dir, L"%s\\DexCorral", appData);
        CreateDirectoryW(dir, nullptr);
        swprintf_s(g_LogPath, L"%s\\CorralHook.log", dir);
        CoTaskMemFree(appData);
    } else {
        // Fallback to temp
        GetTempPathW(MAX_PATH, g_LogPath);
        wcscat_s(g_LogPath, L"CorralHook.log");
    }
}

static void Log(const wchar_t* format, ...) {
    InitLogPath();

    va_list args;
    va_start(args, format);
    wchar_t message[512];
    vswprintf_s(message, format, args);
    va_end(args);

    HANDLE hFile = CreateFileW(g_LogPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
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
// Hidden icon cache - refreshed from HookBridge (in-process, no kernel calls)
// ============================================================================

static void RefreshHiddenIconCache() {
    // Check version - skip rebuild if unchanged (just reads a volatile DWORD)
    DWORD currentVersion = HookBridge::GetVersion();
    if (currentVersion == g_LastVersion) return;

    Log(L"RefreshHiddenIconCache: Version changed %u -> %u", g_LastVersion, currentVersion);

    // Version changed - get updated list from HookBridge
    g_LastVersion = HookBridge::GetHiddenIconNames(g_HiddenIcons);

    for (const auto& name : g_HiddenIcons) {
        Log(L"RefreshHiddenIconCache: Hidden icon: '%s'", name.c_str());
    }
    Log(L"RefreshHiddenIconCache: Total %d hidden icons", (int)g_HiddenIcons.size());

    // Trigger repaint so newly hidden/unhidden icons update visually
    if (g_hDesktopListView) {
        RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
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
// Helper: get display name for a ListView item index (in-process, fast)
// ============================================================================

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
// Helper: check if click point hits a hidden icon
// ============================================================================

static bool IsClickOnHiddenIcon(HWND hwnd, LPARAM lParam) {
    if (g_HiddenIcons.empty()) return false;

    LVHITTESTINFO ht = {};
    ht.pt.x = GET_X_LPARAM(lParam);
    ht.pt.y = GET_Y_LPARAM(lParam);
    int hit = (int)CallWindowProcW(g_OriginalListViewProc, hwnd, LVM_HITTEST, 0, (LPARAM)&ht);
    if (hit < 0) return false;

    wchar_t buf[MAX_PATH] = {};
    if (!GetItemDisplayName(hit, buf, MAX_PATH)) return false;
    return ShouldHideIcon(buf);
}

// ============================================================================
// Helper: deselect any hidden icons (after rubber-band selection, etc.)
// ============================================================================

static void DeselectHiddenIcons(HWND hwnd) {
    if (g_HiddenIcons.empty()) return;

    int count = (int)CallWindowProcW(g_OriginalListViewProc, hwnd, LVM_GETITEMCOUNT, 0, 0);
    for (int i = 0; i < count; i++) {
        DWORD state = (DWORD)CallWindowProcW(g_OriginalListViewProc, hwnd,
            LVM_GETITEMSTATE, i, LVIS_SELECTED | LVIS_FOCUSED);
        if (!(state & (LVIS_SELECTED | LVIS_FOCUSED))) continue;

        wchar_t buf[MAX_PATH] = {};
        if (!GetItemDisplayName(i, buf, MAX_PATH)) continue;
        if (!ShouldHideIcon(buf)) continue;

        // Deselect and unfocus this hidden icon
        LVITEMW lvi = {};
        lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        lvi.state = 0;
        CallWindowProcW(g_OriginalListViewProc, hwnd, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
    }
}

// ============================================================================
// ListView subclass - input filtering for hidden icons
// ============================================================================

// Forward declaration (defined after auto-sort popup section)
static void EnsureExplorerAutoArrangeOff();

// ============================================================================
// Compact visible icons — after Explorer sorts, remove gaps left by hidden icons
// Reads Explorer's current position order (preserving its sort), then repositions
// visible icons into a contiguous grid with no gaps.
// ============================================================================

static void CompactVisibleIcons(HWND hwnd) {
    RefreshHiddenIconCache();

    int itemCount = (int)SendMessageW(hwnd, LVM_GETITEMCOUNT, 0, 0);
    if (itemCount == 0) return;

    // Get desktop icon spacing for grid alignment
    DWORD spacing = (DWORD)SendMessageW(hwnd, LVM_GETITEMSPACING, FALSE, 0);
    int spacingX = LOWORD(spacing);
    int spacingY = HIWORD(spacing);
    if (spacingX <= 0) spacingX = 75;
    if (spacingY <= 0) spacingY = 75;

    Log(L"CompactVisibleIcons: Grid spacing = %dx%d, item count = %d", spacingX, spacingY, itemCount);

    // Collect visible icons with their current positions (set by Explorer's sort)
    struct IconPos {
        int index;
        std::wstring name;
        POINT pos;
    };
    std::vector<IconPos> visibleIcons;
    visibleIcons.reserve(itemCount);

    for (int i = 0; i < itemCount; i++) {
        wchar_t buf[MAX_PATH] = {};
        if (!GetItemDisplayName(i, buf, MAX_PATH)) continue;
        if (ShouldHideIcon(buf)) continue;

        POINT pt = {};
        SendMessageW(hwnd, LVM_GETITEMPOSITION, i, (LPARAM)&pt);
        visibleIcons.push_back({ i, buf, pt });
    }

    Log(L"CompactVisibleIcons: %zu visible icons (out of %d total)", visibleIcons.size(), itemCount);
    if (visibleIcons.empty()) return;

    // Sort by Explorer's current position order (column-major: X first, then Y)
    // This preserves whatever sort order Explorer applied (name, size, type, date)
    std::sort(visibleIcons.begin(), visibleIcons.end(),
        [](const IconPos& a, const IconPos& b) {
            if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
            return a.pos.y < b.pos.y;
        });

    // Get work area for grid layout
    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    int maxRows = (workArea.bottom - workArea.top) / spacingY;
    if (maxRows < 1) maxRows = 20;

    // Reposition visible icons into contiguous grid slots (no gaps)
    // Explorer's LVS_AUTOARRANGE is always OFF (we own it), so no fighting
    for (size_t i = 0; i < visibleIcons.size(); i++) {
        int col = (int)i / maxRows;
        int row = (int)i % maxRows;

        int x = workArea.left + col * spacingX;
        int y = workArea.top + row * spacingY;

        // Only move if position actually changed
        if (visibleIcons[i].pos.x != x || visibleIcons[i].pos.y != y) {
            POINT ptPos = { x, y };
            SendMessageW(hwnd, LVM_SETITEMPOSITION32, visibleIcons[i].index, (LPARAM)&ptPos);
        }
    }

    Log(L"CompactVisibleIcons: Compacted %zu visible icons (hidden icons untouched)", visibleIcons.size());
}

// Timer callback: runs after Explorer finishes its deferred sort/arrange
static void CALLBACK SortFixupTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    KillTimer(hwnd, SORT_FIXUP_TIMER_ID);
    g_CompactionPending = false;

    // Strip LVS_AUTOARRANGE now that Explorer is done repositioning
    if (g_AllowAutoArrangeStyle) {
        g_AllowAutoArrangeStyle = false;
        EnsureExplorerAutoArrangeOff();
        Log(L"SortFixupTimer: Stripped LVS_AUTOARRANGE after sort");
    }

    RefreshHiddenIconCache();
    if (g_HiddenIcons.empty()) return;

    Log(L"SortFixupTimer: Deferred compaction (Explorer idle)");
    g_InsideCustomSort = true;
    CompactVisibleIcons(g_hDesktopListView);
    g_InsideCustomSort = false;
}

static void ScheduleCompaction(HWND hwnd, const wchar_t* source) {
    RefreshHiddenIconCache();
    if (g_HiddenIcons.empty()) return;

    g_CompactionPending = true;
    Log(L"ScheduleCompaction [%s]: Scheduling deferred compaction (%dms)", source, SORT_FIXUP_DELAY_MS);
    KillTimer(hwnd, SORT_FIXUP_TIMER_ID);
    SetTimer(hwnd, SORT_FIXUP_TIMER_ID, SORT_FIXUP_DELAY_MS, SortFixupTimerProc);
}

// ============================================================================
// Auto-sort popup - "Disable auto-sort?" dialog
// ============================================================================

static const wchar_t* POPUP_CLASS_NAME = L"DexCorralAutoSortPopup";
static const int POPUP_WIDTH = 300;
static const int POPUP_HEIGHT = 80;
static const int POPUP_BTN_W = 90;
static const int POPUP_BTN_H = 26;
static const int IDC_DISABLE_BTN = 1001;
static const int IDC_KEEP_BTN = 1002;

static bool IsAutoArrangeOn() {
    return g_OurAutoArrange;
}

// Strip LVS_AUTOARRANGE from Explorer — we manage auto-arrange ourselves
static void EnsureExplorerAutoArrangeOff() {
    if (!g_hDesktopListView) return;
    LONG_PTR style = GetWindowLongPtrW(g_hDesktopListView, GWL_STYLE);
    if (style & LVS_AUTOARRANGE) {
        SetWindowLongPtrW(g_hDesktopListView, GWL_STYLE, style & ~LVS_AUTOARRANGE);
        Log(L"EnsureExplorerAutoArrangeOff: Cleared LVS_AUTOARRANGE from Explorer");
    }
}

static void DismissAutoSortPopup() {
    if (g_hAutoSortPopup) {
        KillTimer(g_hAutoSortPopup, POPUP_DISMISS_TIMER_ID);
        DestroyWindow(g_hAutoSortPopup);
        g_hAutoSortPopup = nullptr;
    }
}

static LRESULT CALLBACK AutoSortPopupProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_DISABLE_BTN) {
            Log(L"AutoSortPopup: User chose DISABLE");
            g_OurAutoArrange = false;
            // Restore icon to where user dropped it
            if (g_PendingMoveIconIndex >= 0 && g_hDesktopListView) {
                POINT pt = g_PendingMoveDropPos;
                SendMessageW(g_hDesktopListView, LVM_SETITEMPOSITION32,
                    g_PendingMoveIconIndex, (LPARAM)&pt);
                Log(L"AutoSortPopup: Restored icon %d to (%d, %d)",
                    g_PendingMoveIconIndex, pt.x, pt.y);
            }
            g_PendingMoveIconIndex = -1;
            DismissAutoSortPopup();
            return 0;
        }
        if (LOWORD(wParam) == IDC_KEEP_BTN) {
            Log(L"AutoSortPopup: User chose KEEP SORTING");
            g_PendingMoveIconIndex = -1;
            DismissAutoSortPopup();
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == POPUP_DISMISS_TIMER_ID) {
            Log(L"AutoSortPopup: Auto-dismissed (timeout)");
            g_PendingMoveIconIndex = -1;
            DismissAutoSortPopup();
            return 0;
        }
        break;

    case WM_DESTROY:
        g_hAutoSortPopup = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static void RegisterAutoSortPopupClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = AutoSortPopupProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = POPUP_CLASS_NAME;
    RegisterClassExW(&wc);
}

static void ShowAutoSortPopup() {
    DismissAutoSortPopup();

    POINT cursorPos;
    GetCursorPos(&cursorPos);

    // Position popup near cursor, clamped to screen
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int x = cursorPos.x + 16;
    int y = cursorPos.y + 16;
    if (x + POPUP_WIDTH > workArea.right) x = workArea.right - POPUP_WIDTH;
    if (y + POPUP_HEIGHT > workArea.bottom) y = workArea.bottom - POPUP_HEIGHT;

    g_hAutoSortPopup = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        POPUP_CLASS_NAME, L"DexCorral",
        WS_POPUP | WS_BORDER | WS_VISIBLE,
        x, y, POPUP_WIDTH, POPUP_HEIGHT,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!g_hAutoSortPopup) {
        Log(L"ShowAutoSortPopup: CreateWindowEx failed, err=%d", GetLastError());
        return;
    }

    // Static label
    CreateWindowExW(0, L"STATIC", L"Auto-sort moved your icon back.",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        8, 8, POPUP_WIDTH - 16, 20,
        g_hAutoSortPopup, nullptr, GetModuleHandleW(nullptr), nullptr);

    // Buttons
    int btnY = 38;
    int gap = 16;
    int totalW = POPUP_BTN_W * 2 + gap;
    int startX = (POPUP_WIDTH - totalW) / 2;

    CreateWindowExW(0, L"BUTTON", L"Disable",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        startX, btnY, POPUP_BTN_W, POPUP_BTN_H,
        g_hAutoSortPopup, (HMENU)(INT_PTR)IDC_DISABLE_BTN, GetModuleHandleW(nullptr), nullptr);

    CreateWindowExW(0, L"BUTTON", L"Keep sorting",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        startX + POPUP_BTN_W + gap, btnY, POPUP_BTN_W, POPUP_BTN_H,
        g_hAutoSortPopup, (HMENU)(INT_PTR)IDC_KEEP_BTN, GetModuleHandleW(nullptr), nullptr);

    // Auto-dismiss timer
    SetTimer(g_hAutoSortPopup, POPUP_DISMISS_TIMER_ID, POPUP_DISMISS_MS, nullptr);

    Log(L"ShowAutoSortPopup: Shown at (%d, %d)", x, y);
}

// Timer callback for debounced re-sort after icon move
static void CALLBACK AutoSortTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    KillTimer(hwnd, AUTOSORT_TIMER_ID);
    g_AutoSortTimerActive = false;

    if (!IsAutoArrangeOn() || g_InsideCustomSort) return;

    Log(L"AutoSortTimer: Re-compacting after manual icon move");
    g_InsideCustomSort = true;
    CompactVisibleIcons(g_hDesktopListView);
    g_InsideCustomSort = false;

    ShowAutoSortPopup();
}

static LRESULT CALLBACK ListViewSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Intercept LVM_SETITEMPOSITION32
    if (uMsg == LVM_SETITEMPOSITION32 && !g_InsideCustomSort) {
        LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);

        // Debounce: if compaction is pending, Explorer is still moving icons — restart timer
        if (g_CompactionPending) {
            KillTimer(hwnd, SORT_FIXUP_TIMER_ID);
            SetTimer(hwnd, SORT_FIXUP_TIMER_ID, SORT_FIXUP_DELAY_MS, SortFixupTimerProc);
            return result;
        }

        // Detect manual icon moves during auto-arrange (for popup)
        if (IsAutoArrangeOn() && !g_HiddenIcons.empty()) {
            int index = (int)wParam;
            wchar_t buf[MAX_PATH] = {};
            if (GetItemDisplayName(index, buf, MAX_PATH) && !ShouldHideIcon(buf)) {
                // Visible icon was moved — save drop position and debounce re-sort
                POINT* pt = (POINT*)lParam;
                g_PendingMoveIconIndex = index;
                g_PendingMoveDropPos = *pt;
                Log(L"ListViewSubclassProc: Visible icon '%s' moved to (%d,%d) while auto-arrange ON",
                    buf, pt->x, pt->y);

                // Debounce: cancel previous timer, start new one
                if (g_AutoSortTimerActive) {
                    KillTimer(hwnd, AUTOSORT_TIMER_ID);
                }
                SetTimer(hwnd, AUTOSORT_TIMER_ID, AUTOSORT_DEBOUNCE_MS, AutoSortTimerProc);
                g_AutoSortTimerActive = true;
            }
        }
        return result;
    }

    // Intercept LVM_SORTITEMS / LVM_SORTITEMSEX - the actual "Sort by Name/Date/Size/Type"
    // Desktop context menu "Sort by" goes through these messages
    if (uMsg == LVM_SORTITEMS || uMsg == LVM_SORTITEMSEX) {
        RefreshHiddenIconCache();
        const wchar_t* msgName = (uMsg == LVM_SORTITEMS) ? L"LVM_SORTITEMS" : L"LVM_SORTITEMSEX";
        Log(L"ListViewSubclassProc: %s received (wParam=0x%IX, lParam=0x%IX)", msgName, wParam, lParam);

        // Let Windows sort first, then fix up positions
        LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
        Log(L"ListViewSubclassProc: %s returned %lld, scheduling compaction", msgName, (long long)result);
        ScheduleCompaction(hwnd, msgName);
        return result;
    }

    // Intercept LVM_ARRANGE - "Auto arrange icons" / "Align icons to grid"
    if (uMsg == LVM_ARRANGE) {
        RefreshHiddenIconCache();
        Log(L"ListViewSubclassProc: LVM_ARRANGE received (wParam=%u)", (UINT)wParam);

        if (!g_HiddenIcons.empty()) {
            // Let Windows arrange first, then fix up
            LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
            Log(L"ListViewSubclassProc: LVM_ARRANGE returned %lld, scheduling compaction", (long long)result);
            ScheduleCompaction(hwnd, L"LVM_ARRANGE");
            return result;
        }

        Log(L"ListViewSubclassProc: No hidden icons - passing LVM_ARRANGE to Windows");
        return CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
    }

    // Intercept LVM_HITTEST - makes hidden icons invisible to ALL hit testing:
    // clicks, drag-drop targets, context menus, tooltips, etc.
    if (uMsg == LVM_HITTEST || uMsg == LVM_SUBITEMHITTEST) {
        RefreshHiddenIconCache();
        LRESULT hit = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
        if (hit >= 0 && !g_HiddenIcons.empty()) {
            wchar_t buf[MAX_PATH] = {};
            if (GetItemDisplayName((int)hit, buf, MAX_PATH) && ShouldHideIcon(buf)) {
                // Clear the hit test result so caller thinks nothing was hit
                LVHITTESTINFO* ht = (LVHITTESTINFO*)lParam;
                ht->iItem = -1;
                ht->iSubItem = -1;
                ht->flags = LVHT_NOWHERE;
                Log(L"ListViewSubclassProc: Filtered hidden icon '%s' from hit test", buf);
                return -1;
            }
        }
        return hit;
    }

    // Suppress mouse hover on hidden icons — prevents "open with" tooltip and
    // hot-tracking highlight that Explorer shows when the cursor passes over an icon
    if (uMsg == WM_MOUSEMOVE) {
        RefreshHiddenIconCache();
        if (!g_HiddenIcons.empty() && IsClickOnHiddenIcon(hwnd, lParam)) {
            return 0;
        }
    }

    // Refresh cache on relevant messages (cheap - just checks version counter)
    switch (uMsg) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
        RefreshHiddenIconCache();
        if (IsClickOnHiddenIcon(hwnd, lParam)) {
            Log(L"ListViewSubclassProc: Swallowed click on hidden icon (msg=0x%04X)", uMsg);
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        // After mouse up (end of rubber-band selection), deselect any hidden icons
        {
            LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
            RefreshHiddenIconCache();
            DeselectHiddenIcons(hwnd);
            return result;
        }

    case WM_KEYDOWN:
        // For arrow keys, let default handler run then skip hidden icons
        if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT) {
            RefreshHiddenIconCache();
            if (!g_HiddenIcons.empty()) {
                LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
                // Check if newly focused item is hidden - if so, send another arrow key to skip it
                int focused = (int)CallWindowProcW(g_OriginalListViewProc, hwnd,
                    LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                if (focused >= 0) {
                    wchar_t buf[MAX_PATH] = {};
                    if (GetItemDisplayName(focused, buf, MAX_PATH) && ShouldHideIcon(buf)) {
                        // Skip this hidden icon by sending another arrow key in the same direction
                        Log(L"ListViewSubclassProc: Skipping hidden icon %d '%s' on arrow key", focused, buf);
                        CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
                    }
                }
                return result;
            }
        }
        break;
    }

    // Prevent Explorer from setting LVS_AUTOARRANGE — we own auto-arrange
    // Exception: g_AllowAutoArrangeStyle is set during sort commands (Explorer needs it to reposition)
    if (uMsg == WM_STYLECHANGING && wParam == GWL_STYLE && !g_AllowAutoArrangeStyle) {
        STYLESTRUCT* ss = (STYLESTRUCT*)lParam;
        if ((ss->styleNew & LVS_AUTOARRANGE) && !(ss->styleOld & LVS_AUTOARRANGE)) {
            ss->styleNew &= ~LVS_AUTOARRANGE;
            Log(L"ListViewSubclassProc: Blocked Explorer from setting LVS_AUTOARRANGE");
        }
    }

    return CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// ShellDefView subclass - hides icons via NM_CUSTOMDRAW
// ============================================================================

// ============================================================================
// Explorer desktop context menu WM_COMMAND IDs (Windows 11, empirically observed)
//
// Sort by:
//   31492 = Sort by Name
//   31493 = Sort by Size
//   31494 = Sort by Item type
//   31495 = Sort by Date modified
//
// View options (toggles):
//   28785 = Auto arrange icons   (toggle on/off)
//   28788 = Align icons to grid  (toggle on/off)
//
// Detecting toggle state after WM_COMMAND:
//   Auto arrange: GetWindowLongPtrW(ListView, GWL_STYLE) & LVS_AUTOARRANGE
//   Align to grid: SendMessageW(ListView, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0)
//                   & LVS_EX_SNAPTOGRID
//
// Note: These command IDs are undocumented and may vary across Windows versions.
// The LVM_SORTITEMS / LVM_ARRANGE messages are NOT sent by Explorer on Win11;
// sorting is done internally, so WM_COMMAND is the only interception point.
// ============================================================================

static LRESULT CALLBACK ShellDefViewSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // ---- Fake the auto-arrange checkmark in the context menu ----
    if (uMsg == WM_INITMENUPOPUP) {
        // Let Explorer build the menu first
        LRESULT result = CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);

        // Override the auto-arrange checkmark with our state
        HMENU hMenu = (HMENU)wParam;
        int menuItemCount = GetMenuItemCount(hMenu);
        Log(L"WM_INITMENUPOPUP: hMenu=%p, itemCount=%d, menuIndex=%d", hMenu, menuItemCount, (int)LOWORD(lParam));

        // Log all item IDs in this submenu for debugging
        for (int i = 0; i < menuItemCount && i < 20; i++) {
            UINT id = GetMenuItemID(hMenu, i);
            Log(L"  MenuItem[%d] id=%u", i, id);
        }

        UINT state = GetMenuState(hMenu, 28785, MF_BYCOMMAND);
        if (state != (UINT)-1) {
            CheckMenuItem(hMenu, 28785, MF_BYCOMMAND | (g_OurAutoArrange ? MF_CHECKED : MF_UNCHECKED));
            Log(L"WM_INITMENUPOPUP: Set auto-arrange checkmark to %s", g_OurAutoArrange ? L"ON" : L"OFF");
        }
        return result;
    }

    // ---- Intercept WM_COMMAND from Explorer desktop context menu ----
    if (uMsg == WM_COMMAND) {
        WORD cmdId = LOWORD(wParam);
        WORD notifyCode = HIWORD(wParam);
        Log(L"ShellDefViewSubclassProc: WM_COMMAND cmdId=%u notifyCode=%u lParam=0x%IX", cmdId, notifyCode, lParam);

        RefreshHiddenIconCache();

        // Auto-arrange: we own this — swallow the command, toggle our flag
        if (cmdId == 28785) {
            g_OurAutoArrange = !g_OurAutoArrange;
            Log(L"  Auto arrange toggled to %s (ours, Explorer not notified)", g_OurAutoArrange ? L"ON" : L"OFF");
            if (g_OurAutoArrange) {
                // Turning ON: compact immediately
                ScheduleCompaction(g_hDesktopListView, L"AutoArrange ON");
            }
            return 0;  // Swallow — Explorer never sees this
        }

        // Sort commands: Explorer needs LVS_AUTOARRANGE ON to actually reposition icons.
        // We keep it on until compaction fires (Explorer repositions asynchronously).
        bool isSortCmd = (cmdId >= 31492 && cmdId <= 31495);
        if (isSortCmd) {
            const wchar_t* sortNames[] = { L"Name", L"Size", L"Item type", L"Date modified" };
            Log(L"  Sort by %s — enabling LVS_AUTOARRANGE for Explorer (will strip after compaction)", sortNames[cmdId - 31492]);

            // Enable LVS_AUTOARRANGE so Explorer repositions icons during its deferred sort
            g_AllowAutoArrangeStyle = true;
            LONG_PTR style = GetWindowLongPtrW(g_hDesktopListView, GWL_STYLE);
            SetWindowLongPtrW(g_hDesktopListView, GWL_STYLE, style | LVS_AUTOARRANGE);

            LRESULT result = CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);

            // Force Explorer to reposition icons according to the new sort column.
            // Win11 Explorer updates internal sort state but may not reposition via ListView.
            CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView, LVM_ARRANGE, LVA_DEFAULT, 0);
            Log(L"  Sent LVM_ARRANGE after sort to force repositioning");

            // Don't strip LVS_AUTOARRANGE yet — Explorer may still reposition asynchronously.
            // SortFixupTimerProc will strip it after compaction.
            ScheduleCompaction(g_hDesktopListView, L"Sort command");
            return result;
        }

        // All other commands: let Explorer handle, then compact if needed
        LRESULT result = CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);

        bool needsCompaction = false;
        switch (cmdId) {
        case 28788: {
            LRESULT exStyle = CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
                LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
            bool snapToGrid = (exStyle & LVS_EX_SNAPTOGRID) != 0;
            Log(L"  Align icons to grid is now %s", snapToGrid ? L"ON" : L"OFF");
            needsCompaction = true;
            break;
        }
        }

        // Ensure Explorer didn't sneak LVS_AUTOARRANGE back on
        EnsureExplorerAutoArrangeOff();

        if (needsCompaction) {
            ScheduleCompaction(g_hDesktopListView, L"WM_COMMAND");
        }
        return result;
    }

    if (uMsg == WM_NOTIFY) {
        NMHDR* nmhdr = (NMHDR*)lParam;

        // Suppress hot-tracking (hover highlight) on hidden icons
        if (nmhdr->code == LVN_HOTTRACK && nmhdr->hwndFrom == g_hDesktopListView) {
            NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
            if (nmlv->iItem >= 0 && !g_HiddenIcons.empty()) {
                wchar_t buf[MAX_PATH] = {};
                if (GetItemDisplayName(nmlv->iItem, buf, MAX_PATH) && ShouldHideIcon(buf)) {
                    return -1;  // Cancel hot-tracking for this item
                }
            }
        }

        if (nmhdr->code == NM_CUSTOMDRAW && nmhdr->hwndFrom == g_hDesktopListView) {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)lParam;

            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT: {
                // Call original chain first so other third-party hooks get
                // their PREPAINT notification, then ensure we also get per-item callbacks.
                LRESULT originalResult = CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);
                return originalResult | CDRF_NOTIFYITEMDRAW;
            }

            case CDDS_ITEMPREPAINT: {
                // Check version counter - no kernel calls if unchanged
                RefreshHiddenIconCache();

                if (!g_HiddenIcons.empty()) {
                    // Get this icon's display name
                    int itemIndex = (int)cd->nmcd.dwItemSpec;
                    wchar_t buf[MAX_PATH] = {};
                    GetItemDisplayName(itemIndex, buf, MAX_PATH);

                    if (buf[0] && ShouldHideIcon(buf)) {
                        return CDRF_SKIPDEFAULT;
                    }
                }

                // Not a DexCorral icon - let the original chain decide.
                // This preserves other hooks' ability to hide their own icons.
                return CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);
            }
            }
        }
    }

    return CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// IDropTarget wrapper - intercepts OLE drag-drop on hidden icons
// Explorer's internal IDropTarget does hit-testing via direct function calls
// (not SendMessage), so our LVM_HITTEST interception doesn't catch it.
// We wrap the original IDropTarget and reject drops on hidden icons.
// ============================================================================

static IDropTarget* g_pOriginalDropTarget = nullptr;

// Always-on logging for IDropTarget debugging (writes to separate file)
static void LogDT(const wchar_t* format, ...) {
    // Reuse same directory as main log
    InitLogPath();
    wchar_t path[MAX_PATH];
    wcscpy_s(path, g_LogPath);
    // Replace CorralHook.log -> CorralHook_DropTarget.log
    wchar_t* dot = wcsrchr(path, L'.');
    if (dot) {
        wcscpy_s(dot, MAX_PATH - (dot - path), L"_DropTarget.log");
    }

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

// Check if a screen-space point is over a hidden icon
static bool IsPointOnHiddenIcon(POINT ptScreen) {
    if (!g_hDesktopListView || g_HiddenIcons.empty()) return false;

    // Convert screen coords to ListView client coords
    POINT ptClient = ptScreen;
    ScreenToClient(g_hDesktopListView, &ptClient);

    // Do hit test via the original proc to get the true hit, then check if it's hidden
    LVHITTESTINFO ht = {};
    ht.pt = ptClient;
    int hit = (int)CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
        LVM_HITTEST, 0, (LPARAM)&ht);
    if (hit < 0) return false;

    wchar_t buf[MAX_PATH] = {};
    if (!GetItemDisplayName(hit, buf, MAX_PATH)) return false;
    bool hidden = ShouldHideIcon(buf);
    if (hidden) {
        LogDT(L"IsPointOnHiddenIcon: screen(%d,%d) client(%d,%d) hit=%d name='%s' -> HIDDEN",
              ptScreen.x, ptScreen.y, ptClient.x, ptClient.y, hit, buf);
    }
    return hidden;
}

class DropTargetWrapper : public IDropTarget {
public:
    DropTargetWrapper(IDropTarget* pOriginal) : m_pOriginal(pOriginal) {
        m_pOriginal->AddRef();
    }

    ~DropTargetWrapper() {
        m_pOriginal->Release();
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropTarget)) {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        LONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    // IDropTarget
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
        POINTL pt, DWORD* pdwEffect) override {
        LogDT(L"DragEnter: pt=(%d,%d) effect=0x%X", pt.x, pt.y, *pdwEffect);
        m_pDataObj = pDataObj;
        m_lastKeyState = grfKeyState;
        RefreshHiddenIconCache();
        POINT ptScreen = { pt.x, pt.y };
        if (IsPointOnHiddenIcon(ptScreen)) {
            // Over hidden icon: do NOT enter the original at all.
            // The original would highlight the icon and show "open with" tooltip.
            m_bOverHidden = true;
            m_bOriginalActive = false;
            *pdwEffect = DROPEFFECT_NONE;
            LogDT(L"DragEnter: BLOCKED (hidden icon) - original NOT entered");
            return S_OK;
        }
        m_bOverHidden = false;
        m_bOriginalActive = true;
        HRESULT hr = m_pOriginal->DragEnter(pDataObj, grfKeyState, pt, pdwEffect);
        LogDT(L"DragEnter: forwarded, effect=0x%X hr=0x%08X", *pdwEffect, hr);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        RefreshHiddenIconCache();
        m_lastKeyState = grfKeyState;
        POINT ptScreen = { pt.x, pt.y };
        bool overHidden = IsPointOnHiddenIcon(ptScreen);

        if (overHidden) {
            if (!m_bOverHidden) {
                LogDT(L"DragOver: entering hidden icon at (%d,%d)", pt.x, pt.y);
                // Transitioning to hidden: tell original to leave so it clears highlight/tooltip
                if (m_bOriginalActive) {
                    m_pOriginal->DragLeave();
                    m_bOriginalActive = false;
                    LogDT(L"DragOver: called DragLeave on original to clear highlight");
                }
            }
            m_bOverHidden = true;
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }

        if (m_bOverHidden) {
            LogDT(L"DragOver: leaving hidden icon at (%d,%d)", pt.x, pt.y);
            // Transitioning from hidden to visible: re-enter original
            if (m_pDataObj && !m_bOriginalActive) {
                DWORD enterEffect = *pdwEffect;
                m_pOriginal->DragEnter(m_pDataObj, m_lastKeyState, pt, &enterEffect);
                m_bOriginalActive = true;
                LogDT(L"DragOver: re-entered original after hidden");
            }
        }
        m_bOverHidden = false;
        return m_pOriginal->DragOver(grfKeyState, pt, pdwEffect);
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override {
        LogDT(L"DragLeave");
        m_bOverHidden = false;
        m_pDataObj = nullptr;
        if (m_bOriginalActive) {
            m_bOriginalActive = false;
            return m_pOriginal->DragLeave();
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState,
        POINTL pt, DWORD* pdwEffect) override {
        LogDT(L"Drop: pt=(%d,%d) effect=0x%X", pt.x, pt.y, *pdwEffect);
        RefreshHiddenIconCache();
        POINT ptScreen = { pt.x, pt.y };
        if (IsPointOnHiddenIcon(ptScreen)) {
            LogDT(L"Drop: BLOCKED (hidden icon)");
            *pdwEffect = DROPEFFECT_NONE;
            if (m_bOriginalActive) {
                m_pOriginal->DragLeave();
                m_bOriginalActive = false;
            }
            return S_OK;
        }
        if (!m_bOriginalActive && m_pDataObj) {
            // Original was left (we were over hidden). Re-enter before drop.
            DWORD enterEffect = *pdwEffect;
            m_pOriginal->DragEnter(pDataObj, grfKeyState, pt, &enterEffect);
            m_bOriginalActive = true;
        }
        HRESULT hr = m_pOriginal->Drop(pDataObj, grfKeyState, pt, pdwEffect);
        m_bOriginalActive = false;
        LogDT(L"Drop: forwarded, effect=0x%X hr=0x%08X", *pdwEffect, hr);
        return hr;
    }

private:
    IDropTarget* m_pOriginal;
    IDataObject* m_pDataObj = nullptr;
    LONG m_refCount = 1;
    bool m_bOverHidden = false;
    bool m_bOriginalActive = false;  // Whether original's DragEnter was called without DragLeave
    DWORD m_lastKeyState = 0;
};

static DropTargetWrapper* g_pDropTargetWrapper = nullptr;
static HWND g_hDropTargetWindow = nullptr;  // Which window we hooked (ListView or ShellDefView)
static UINT_PTR g_DropTargetTimerId = 0;
static int g_DropTargetRetryCount = 0;
static const int DROP_TARGET_MAX_RETRIES = 30;  // 30 retries * 500ms = 15 seconds
static const UINT DROP_TARGET_TIMER_ID = 0xDC01;
static const UINT DROP_TARGET_TIMER_MS = 500;

static bool TryInstallDropTargetHook() {
    if (g_pDropTargetWrapper) return true;  // Already installed

    // Try SysListView32 first, then SHELLDLL_DefView
    HWND candidates[] = { g_hDesktopListView, g_hShellDefView };
    const wchar_t* names[] = { L"SysListView32", L"SHELLDLL_DefView" };

    for (int i = 0; i < 2; i++) {
        if (!candidates[i]) {
            LogDT(L"TryInstall: %s hwnd is NULL, skipping", names[i]);
            continue;
        }

        IDropTarget* pDropTarget = (IDropTarget*)GetPropW(candidates[i], L"OleDropTargetInterface");
        if (!pDropTarget) {
            LogDT(L"TryInstall: No OleDropTargetInterface on %s (hwnd=%p)", names[i], candidates[i]);
            continue;
        }

        LogDT(L"TryInstall: Found IDropTarget on %s (hwnd=%p, pDT=%p)", names[i], candidates[i], pDropTarget);

        g_pOriginalDropTarget = pDropTarget;
        g_hDropTargetWindow = candidates[i];
        g_pDropTargetWrapper = new DropTargetWrapper(g_pOriginalDropTarget);

        HRESULT hrRevoke = RevokeDragDrop(g_hDropTargetWindow);
        LogDT(L"TryInstall: RevokeDragDrop on %s returned 0x%08X", names[i], hrRevoke);

        HRESULT hr = RegisterDragDrop(g_hDropTargetWindow, g_pDropTargetWrapper);
        if (SUCCEEDED(hr)) {
            // Verify our wrapper is now set
            IDropTarget* pVerify = (IDropTarget*)GetPropW(g_hDropTargetWindow, L"OleDropTargetInterface");
            LogDT(L"TryInstall: SUCCESS on %s. Verify prop=%p (wrapper=%p, match=%s)",
                  names[i], pVerify, g_pDropTargetWrapper,
                  (pVerify == g_pDropTargetWrapper) ? L"YES" : L"NO");
            return true;
        }

        LogDT(L"TryInstall: RegisterDragDrop FAILED on %s (0x%08X)", names[i], hr);
        RegisterDragDrop(g_hDropTargetWindow, g_pOriginalDropTarget);
        g_pDropTargetWrapper->Release();
        g_pDropTargetWrapper = nullptr;
        g_pOriginalDropTarget = nullptr;
        g_hDropTargetWindow = nullptr;
    }

    return false;
}

// Timer callback for deferred IDropTarget hook installation
static void CALLBACK DropTargetRetryTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    g_DropTargetRetryCount++;
    LogDT(L"RetryTimer: attempt %d/%d", g_DropTargetRetryCount, DROP_TARGET_MAX_RETRIES);

    if (TryInstallDropTargetHook()) {
        KillTimer(hwnd, idEvent);
        g_DropTargetTimerId = 0;
        LogDT(L"RetryTimer: SUCCESS after %d retries", g_DropTargetRetryCount);
        return;
    }

    if (g_DropTargetRetryCount >= DROP_TARGET_MAX_RETRIES) {
        KillTimer(hwnd, idEvent);
        g_DropTargetTimerId = 0;
        LogDT(L"RetryTimer: GAVE UP after %d retries", g_DropTargetRetryCount);
    }
}

static void InstallDropTargetHook() {
    LogDT(L"InstallDropTargetHook: ListView=%p ShellDefView=%p", g_hDesktopListView, g_hShellDefView);

    if (!g_hDesktopListView) {
        LogDT(L"InstallDropTargetHook: No ListView, aborting");
        return;
    }

    // Try immediately first
    if (TryInstallDropTargetHook()) {
        LogDT(L"InstallDropTargetHook: Immediate install succeeded");
        return;
    }

    // Not available yet - start a retry timer
    g_DropTargetRetryCount = 0;
    g_DropTargetTimerId = SetTimer(g_hDesktopListView, DROP_TARGET_TIMER_ID,
        DROP_TARGET_TIMER_MS, DropTargetRetryTimerProc);
    if (g_DropTargetTimerId) {
        LogDT(L"InstallDropTargetHook: Deferred, timer=%llu, interval=%dms", g_DropTargetTimerId, DROP_TARGET_TIMER_MS);
    } else {
        LogDT(L"InstallDropTargetHook: SetTimer FAILED, GetLastError=%d", GetLastError());
    }
}

static void UninstallDropTargetHook() {
    if (g_DropTargetTimerId && g_hDesktopListView) {
        KillTimer(g_hDesktopListView, g_DropTargetTimerId);
        g_DropTargetTimerId = 0;
        LogDT(L"UninstallDropTargetHook: Killed retry timer");
    }

    if (!g_pDropTargetWrapper || !g_hDropTargetWindow) {
        LogDT(L"UninstallDropTargetHook: Nothing to restore (wrapper=%p, window=%p)",
              g_pDropTargetWrapper, g_hDropTargetWindow);
        return;
    }

    RevokeDragDrop(g_hDropTargetWindow);
    RegisterDragDrop(g_hDropTargetWindow, g_pOriginalDropTarget);
    LogDT(L"UninstallDropTargetHook: Original IDropTarget restored on %p", g_hDropTargetWindow);

    g_pDropTargetWrapper->Release();
    g_pDropTargetWrapper = nullptr;
    g_pOriginalDropTarget = nullptr;
    g_hDropTargetWindow = nullptr;
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

    // Subclass SysListView32 for input filtering on hidden icons
    g_OriginalListViewProc = (WNDPROC)SetWindowLongPtrW(
        g_hDesktopListView, GWLP_WNDPROC, (LONG_PTR)ListViewSubclassProc);

    g_HookActive = true;

    // Take over auto-arrange: capture Explorer's current state, then disable it
    LONG_PTR lvStyle = GetWindowLongPtrW(g_hDesktopListView, GWL_STYLE);
    g_OurAutoArrange = (lvStyle & LVS_AUTOARRANGE) != 0;
    if (g_OurAutoArrange) {
        SetWindowLongPtrW(g_hDesktopListView, GWL_STYLE, lvStyle & ~LVS_AUTOARRANGE);
    }
    Log(L"InitializeCorralHook: Took over auto-arrange (was %s)", g_OurAutoArrange ? L"ON" : L"OFF");

    // Register popup window class for auto-sort dialog
    RegisterAutoSortPopupClass();

    // Force initial read from HookBridge
    g_LastVersion = 0;
    RefreshHiddenIconCache();

    // Hook IDropTarget to intercept OLE drag-drop on hidden icons
    InstallDropTargetHook();

    // Trigger repaint so hidden icons disappear
    RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

    Log(L"InitializeCorralHook: Hook active, %d icons to hide", (int)g_HiddenIcons.size());
    return true;
}

void CleanupCorralHook() {
    // Dismiss auto-sort popup if showing
    DismissAutoSortPopup();
    if (g_hDesktopListView) {
        if (g_AutoSortTimerActive) {
            KillTimer(g_hDesktopListView, AUTOSORT_TIMER_ID);
            g_AutoSortTimerActive = false;
        }
        KillTimer(g_hDesktopListView, SORT_FIXUP_TIMER_ID);
    }
    UnregisterClassW(POPUP_CLASS_NAME, GetModuleHandleW(nullptr));

    // Restore IDropTarget before removing subclass (drop target uses subclass proc)
    UninstallDropTargetHook();

    // Restore ListView subclass first (inner-most subclass)
    if (g_hDesktopListView && g_OriginalListViewProc) {
        SetWindowLongPtrW(g_hDesktopListView, GWLP_WNDPROC, (LONG_PTR)g_OriginalListViewProc);
        g_OriginalListViewProc = nullptr;
    }

    if (g_hShellDefView && g_OriginalShellDefViewProc) {
        SetWindowLongPtrW(g_hShellDefView, GWLP_WNDPROC, (LONG_PTR)g_OriginalShellDefViewProc);
        g_OriginalShellDefViewProc = nullptr;
    }

    // Restore Explorer's auto-arrange if we had it on
    if (g_OurAutoArrange && g_hDesktopListView) {
        LONG_PTR style = GetWindowLongPtrW(g_hDesktopListView, GWL_STYLE);
        SetWindowLongPtrW(g_hDesktopListView, GWL_STYLE, style | LVS_AUTOARRANGE);
        Log(L"CleanupCorralHook: Restored LVS_AUTOARRANGE to Explorer");
    }
    g_OurAutoArrange = false;

    g_HookActive = false;
    g_HiddenIcons.clear();
    g_LastVersion = 0;

    // Refresh desktop to show all icons again
    if (g_hDesktopListView) {
        RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    Log(L"CleanupCorralHook: Hook cleaned up");
}

bool IsCorralHookActive() { return g_HookActive; }
