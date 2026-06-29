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
#include <Shlwapi.h>
#include <exdisp.h>   // IShellWindows — route to the desktop's IFolderView
#include <initguid.h> // Define PKEY_* constants in this TU (DECLSPEC_SELECTANY)
#include <propkey.h>
#include <oleidl.h>
#include <ole2.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

// Global state
static HWND g_hDesktopListView = nullptr;
static HWND g_hShellDefView = nullptr;
static WNDPROC g_OriginalShellDefViewProc = nullptr;
static WNDPROC g_OriginalListViewProc = nullptr;
static bool g_HookActive = false;

// Cached hidden icon list - only rebuilt when version changes
static std::vector<HiddenIconInfo> g_HiddenIcons;
static DWORD g_LastVersion = 0;

// Per-index hidden memo: -1 unknown (computed lazily), 0 visible, 1 hidden.
// Avoids a text fetch + linear list scan on every paint/hit-test. Invalidated
// whenever the hidden list version changes or item indices can shift
// (insert/delete/rename/sort).
static std::vector<int8_t> g_HiddenByIndex;

static void InvalidateHiddenIndexCache() {
    g_HiddenByIndex.clear();
}

// Desktop sort modes — we own desktop sorting entirely.
// Explorer's LVS_AUTOARRANGE is permanently off; sort commands are executed
// by the hook so hidden icons never participate in layout.
enum class DesktopSortMode
{
    PositionOrder, // Keep current position order, just remove gaps (compaction)
    Name,
    Size,
    Type,
    Date
};

// Auto-sort popup state
static const UINT_PTR AUTOSORT_TIMER_ID = 9901;
static const UINT_PTR POPUP_DISMISS_TIMER_ID = 9902;
static const UINT_PTR SORT_FIXUP_TIMER_ID = 9903;
static const UINT_PTR REVALIDATE_TIMER_ID = 9904;
static const DWORD REVALIDATE_DELAY_MS = 600;
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
static DesktopSortMode g_LastSortMode = DesktopSortMode::PositionOrder; // Last sort the user applied


// ============================================================================
// Debug logging - gated by the DebugLogging config flag, writes to
// %APPDATA%/DexCorral/CorralHook.log
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
    if (!HookBridge::IsDebugLogging()) return;
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

// Debug log defined in dllmain.cpp (same DLL) — gated by the DebugLogging
// config flag like every log. Used for the events worth finding in dllmain.log
// when logging is enabled (exceptions, safe mode transitions, drop-target state).
void DllLog(const wchar_t* format, ...);

// ============================================================================
// Crash containment — the hook must never take Explorer down.
//
// Every entry point Explorer calls into (both subclass procs, timer callbacks,
// the IDropTarget wrapper) is wrapped in SEH. If an exception escapes, the
// hook goes inert for the rest of the session: hidden icons are released and
// every entry point becomes a plain pass-through to the original handler.
// C++ exceptions are caught too — MSVC implements them on top of SEH.
// ============================================================================

static volatile LONG g_HookInert = 0;

static void GoInertAfterException(const wchar_t* where, DWORD exceptionCode) {
    if (InterlockedExchange(&g_HookInert, 1) != 0) return;

    DllLog(L"%s: unhandled exception 0x%08X — hook is now inert (pass-through), hidden icons released",
           where, exceptionCode);

    g_HiddenIcons.clear();
    InvalidateHiddenIndexCache();

    // Async invalidate only — formerly hidden icons reappear on the next paint
    if (g_hDesktopListView) {
        RedrawWindow(g_hDesktopListView, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}

// ============================================================================
// Safe mode — crash sentinel for an explorer.exe-resident hook.
//
// A "session pending" registry flag is armed immediately before the subclass
// procs are installed and cleared once Explorer survives a stability window
// (or shuts down cleanly). If Explorer dies while the flag is armed, the next
// session increments a failure counter; after N consecutive failures the hook
// stays disabled for one session (safe mode) and the counters reset so the
// session after that gets a fresh attempt.
// ============================================================================

static const wchar_t* SAFEMODE_REGKEY = L"Software\\DexCorral";
static const wchar_t* REGVAL_HOOK_PENDING = L"HookStartPending";
static const wchar_t* REGVAL_HOOK_FAILURES = L"HookFailureCount";
static const DWORD SAFEMODE_FAILURE_THRESHOLD = 3;
static const UINT_PTR STABILITY_TIMER_ID = 9905;
static const DWORD STABILITY_DELAY_MS = 60000; // Explorer alive this long => stable

static bool g_SafeMode = false;
static bool g_SafeModeEvaluated = false;
static bool g_CrashSentinelArmed = false;

static DWORD ReadSafeModeValue(const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER, SAFEMODE_REGKEY, name, RRF_RT_REG_DWORD,
                 nullptr, &value, &size);
    return value;
}

static void WriteSafeModeValue(const wchar_t* name, DWORD value) {
    RegSetKeyValueW(HKEY_CURRENT_USER, SAFEMODE_REGKEY, name, REG_DWORD,
                    &value, sizeof(value));
}

static void MarkHookSessionStable(const wchar_t* reason) {
    if (!g_CrashSentinelArmed) return;
    g_CrashSentinelArmed = false;
    WriteSafeModeValue(REGVAL_HOOK_PENDING, 0);
    WriteSafeModeValue(REGVAL_HOOK_FAILURES, 0);
    DllLog(L"MarkHookSessionStable: %s — crash sentinel cleared, failure count reset", reason);
}

static void CALLBACK StabilityTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    KillTimer(hwnd, STABILITY_TIMER_ID);
    MarkHookSessionStable(L"stability window elapsed");
}

// Returns true when the hook may be installed; false => safe mode this session.
// Evaluated once per session, before the first installation attempt.
static bool EvaluateSafeMode() {
    if (g_SafeModeEvaluated) return !g_SafeMode;
    g_SafeModeEvaluated = true;

    DWORD pending = ReadSafeModeValue(REGVAL_HOOK_PENDING);
    DWORD failures = ReadSafeModeValue(REGVAL_HOOK_FAILURES);

    if (pending) {
        // Last session armed the sentinel and never confirmed stability —
        // Explorer most likely died with our hook installed
        failures++;
        WriteSafeModeValue(REGVAL_HOOK_FAILURES, failures);
        WriteSafeModeValue(REGVAL_HOOK_PENDING, 0);
        DllLog(L"EvaluateSafeMode: previous session did not confirm hook stability — consecutive failures: %u", failures);
    }

    if (failures >= SAFEMODE_FAILURE_THRESHOLD) {
        g_SafeMode = true;
        // Reset so the next session attempts the hook again
        WriteSafeModeValue(REGVAL_HOOK_FAILURES, 0);
        DllLog(L"EvaluateSafeMode: SAFE MODE — hook disabled for this session after %u consecutive failures", failures);
        return false;
    }
    return true;
}

// ============================================================================
// Hidden icon cache - refreshed from HookBridge (in-process, no kernel calls)
// ============================================================================

// Deferred revalidation after a hidden-list change. The desktop ListView is
// owner-data: the defview updates items in place (e.g. after a file rename)
// without any message we can intercept, so a verdict computed against the
// pre-update item can be stale. Re-check once the shell has settled: drop the
// memo, repaint, and have the app re-park hidden icons with fresh identities.
static int g_RevalidatePass = 0;

static void CALLBACK RevalidateTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    KillTimer(hwnd, REVALIDATE_TIMER_ID);
    if (g_HookInert) return;

    DWORD exCode = 0;
    __try {
        InvalidateHiddenIndexCache();
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

        HWND appWnd = HookBridge::GetAppMessageWindow();
        if (appWnd) {
            PostMessage(appWnd, WM_APP + 100, 0, 0);
        }
        Log(L"RevalidateTimer: pass %d — memo dropped, repainted, repark posted", g_RevalidatePass);

        // Second, later pass for slow shells (e.g. OneDrive-backed desktops where
        // the defview can take seconds to process a file operation)
        if (g_RevalidatePass == 0) {
            g_RevalidatePass = 1;
            SetTimer(hwnd, REVALIDATE_TIMER_ID, 2000, RevalidateTimerProc);
        }
    } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        GoInertAfterException(L"RevalidateTimerProc", exCode);
    }
}

static void RefreshHiddenIconCache() {
    // Check version - skip rebuild if unchanged (just reads a volatile DWORD)
    DWORD currentVersion = HookBridge::GetVersion();
    if (currentVersion == g_LastVersion) return;

    Log(L"RefreshHiddenIconCache: Version changed %u -> %u", g_LastVersion, currentVersion);

    // Version changed - get updated list from HookBridge
    g_LastVersion = HookBridge::GetHiddenIcons(g_HiddenIcons);
    InvalidateHiddenIndexCache();

    for (const auto& info : g_HiddenIcons) {
        Log(L"RefreshHiddenIconCache: Hidden icon: '%s' (%s)",
            info.displayName.c_str(), info.parsingName.c_str());
    }
    Log(L"RefreshHiddenIconCache: Total %d hidden icons", (int)g_HiddenIcons.size());

    // Trigger repaint so newly hidden/unhidden icons update visually
    if (g_hDesktopListView) {
        RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

        // Re-check once the shell has settled — the defview may still be
        // processing the file operation (rename) that caused this change,
        // in which case verdicts computed right now are against stale items
        g_RevalidatePass = 0;
        KillTimer(g_hDesktopListView, REVALIDATE_TIMER_ID);
        SetTimer(g_hDesktopListView, REVALIDATE_TIMER_ID, REVALIDATE_DELAY_MS, RevalidateTimerProc);
    }
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
// Icon identity — PIDL-based matching with per-index memoization
//
// Identity is matched by canonical parsing name (full path or ::{CLSID}) so
// display-name collisions (folder "Project" vs file "Project.txt" with hidden
// extensions, user vs Public desktop) can never hide the wrong icon. Falls
// back to display-name matching when no parsing name is available on either
// side.
// ============================================================================

// Item PIDLs come from the desktop's IFolderView (the documented route; we
// are in-process on Explorer's UI thread). The old lParam-as-PIDL ListView
// trick does NOT work on modern Windows — the desktop ListView is owner-data,
// so LVM_GETITEM with LVIF_PARAM returns 0.
static IFolderView* g_pDesktopFolderView = nullptr;

static void ReleaseDesktopFolderView() {
    if (g_pDesktopFolderView) {
        g_pDesktopFolderView->Release();
        g_pDesktopFolderView = nullptr;
    }
}

// Raymond Chen's canonical route: ShellWindows -> desktop window ->
// top-level browser -> active shell view -> IFolderView
static bool EnsureDesktopFolderView() {
    if (g_pDesktopFolderView) return true;

    // Rate-limit acquisition attempts: callers run on paint/hit-test paths,
    // and a CoCreateInstance per call would be far too expensive while the
    // view is unavailable (e.g. early Explorer startup)
    static DWORD s_lastFailTick = 0;
    if (s_lastFailTick != 0 && GetTickCount() - s_lastFailTick < 2000) {
        return false;
    }

    IShellWindows* psw = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                                IID_PPV_ARGS(&psw))) || !psw) {
        return false;
    }

    VARIANT vtLoc;
    VariantInit(&vtLoc);
    vtLoc.vt = VT_I4;
    vtLoc.lVal = CSIDL_DESKTOP;
    VARIANT vtEmpty;
    VariantInit(&vtEmpty);
    long lhwnd = 0;
    IDispatch* pdisp = nullptr;

    if (psw->FindWindowSW(&vtLoc, &vtEmpty, SWC_DESKTOP, &lhwnd,
                          SWFO_NEEDDISPATCH, &pdisp) == S_OK && pdisp) {
        IServiceProvider* psp = nullptr;
        if (SUCCEEDED(pdisp->QueryInterface(IID_PPV_ARGS(&psp))) && psp) {
            IShellBrowser* psb = nullptr;
            if (SUCCEEDED(psp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&psb))) && psb) {
                IShellView* psv = nullptr;
                if (SUCCEEDED(psb->QueryActiveShellView(&psv)) && psv) {
                    psv->QueryInterface(IID_PPV_ARGS(&g_pDesktopFolderView));
                    psv->Release();
                }
                psb->Release();
            }
            psp->Release();
        }
        pdisp->Release();
    }
    psw->Release();

    if (!g_pDesktopFolderView) {
        s_lastFailTick = GetTickCount() ? GetTickCount() : 1;
        Log(L"EnsureDesktopFolderView: FAILED to acquire desktop IFolderView");
        return false;
    }
    s_lastFailTick = 0;
    return true;
}

// Returns the item's child PIDL; caller frees with CoTaskMemFree.
static PITEMID_CHILD GetItemChildPidl(int index) {
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!EnsureDesktopFolderView()) return nullptr;
        PITEMID_CHILD pidl = nullptr;
        if (SUCCEEDED(g_pDesktopFolderView->Item(index, &pidl)) && pidl) {
            return pidl;
        }
        // The desktop view may have been rebuilt — drop the cached view and retry once
        ReleaseDesktopFolderView();
    }
    return nullptr;
}

static const int PARSING_NAME_CCH = MAX_PATH * 2;

// No C++ objects with destructors in here — wrapped by SEH below
static BOOL GetParsingNameFromPidlImpl(PCUITEMID_CHILD pidl, wchar_t* buf, int cch) {
    IShellFolder* psf = nullptr;
    if (FAILED(SHGetDesktopFolder(&psf)) || !psf) return FALSE;

    STRRET sr = {};
    BOOL ok = FALSE;
    if (SUCCEEDED(psf->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &sr))) {
        ok = SUCCEEDED(StrRetToBufW(&sr, pidl, buf, cch));
    }
    psf->Release();
    return ok && buf[0] != 0;
}

static BOOL GetParsingNameFromPidlGuarded(PCUITEMID_CHILD pidl, wchar_t* buf, int cch) {
    __try {
        return GetParsingNameFromPidlImpl(pidl, buf, cch);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
}

static BOOL GetItemParsingName(int index, wchar_t* buf, int cch) {
    PITEMID_CHILD pidl = GetItemChildPidl(index);
    if (!pidl) return FALSE;
    BOOL ok = GetParsingNameFromPidlGuarded(pidl, buf, cch);
    CoTaskMemFree(pidl);
    return ok;
}

static bool ShouldHideIconByIndex(int index) {
    if (index < 0 || g_HiddenIcons.empty() || !g_OriginalListViewProc) return false;

    int itemCount = (int)CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
        LVM_GETITEMCOUNT, 0, 0);
    if (index >= itemCount) return false;

    // (Re)size memo when the item count changed — indices have shifted
    if ((int)g_HiddenByIndex.size() != itemCount) {
        g_HiddenByIndex.assign(itemCount, -1);
    }
    if (g_HiddenByIndex[index] >= 0) {
        return g_HiddenByIndex[index] == 1;
    }

    wchar_t parsing[PARSING_NAME_CCH] = {};
    bool haveParsing = GetItemParsingName(index, parsing, PARSING_NAME_CCH) != FALSE;

    wchar_t display[MAX_PATH] = {};
    bool haveDisplay = GetItemDisplayName(index, display, MAX_PATH);

    bool hidden = false;
    for (const auto& entry : g_HiddenIcons) {
        if (haveParsing && !entry.parsingName.empty()) {
            // Both sides have a canonical identity — match strictly on it
            if (_wcsicmp(entry.parsingName.c_str(), parsing) == 0) { hidden = true; break; }
        } else if (haveDisplay) {
            if (_wcsicmp(entry.displayName.c_str(), display) == 0) { hidden = true; break; }
        }
    }

    // Only memoize verdicts backed by a canonical identity. A display-name
    // fallback verdict (parsing name unavailable, e.g. IFolderView not
    // acquired yet) may be wrong for colliding names — recompute it next time
    // instead of letting it stick until the next invalidation.
    if (haveParsing) {
        g_HiddenByIndex[index] = hidden ? 1 : 0;
    }
    return hidden;
}

// ============================================================================
// Identity-based icon positioning — executed on the Explorer UI thread on
// behalf of the app (requests arrive via HookBridge + registered message).
// Requests with a parsing name match canonically; requests without one match
// by display name but never touch hidden icons, so a free icon can't be
// confused with a hidden name-twin.
// ============================================================================

static void PositionItemsByIdentity(HWND hwnd, const std::vector<IconPositionRequest>& requests) {
    if (requests.empty()) return;
    RefreshHiddenIconCache();

    int itemCount = (int)CallWindowProcW(g_OriginalListViewProc, hwnd, LVM_GETITEMCOUNT, 0, 0);
    int moved = 0;

    for (int i = 0; i < itemCount; i++) {
        wchar_t parsing[PARSING_NAME_CCH] = {};
        bool haveParsing = GetItemParsingName(i, parsing, PARSING_NAME_CCH) != FALSE;
        wchar_t display[MAX_PATH] = {};
        bool haveDisplay = GetItemDisplayName(i, display, MAX_PATH);

        for (const auto& req : requests) {
            bool match = false;
            if (!req.parsingName.empty()) {
                if (haveParsing)
                    match = _wcsicmp(req.parsingName.c_str(), parsing) == 0;
                else if (haveDisplay) // degraded mode: no parsing names available
                    match = _wcsicmp(req.displayName.c_str(), display) == 0;
            } else if (haveDisplay) {
                match = _wcsicmp(req.displayName.c_str(), display) == 0 &&
                        !ShouldHideIconByIndex(i);
            }
            if (match) {
                // Through the original proc: bypasses our own interception
                POINT pt = { req.pt.x, req.pt.y };
                CallWindowProcW(g_OriginalListViewProc, hwnd, LVM_SETITEMPOSITION32, i, (LPARAM)&pt);
                moved++;
                break;
            }
        }
    }

    Log(L"PositionItemsByIdentity: %d of %zu requests applied", moved, requests.size());
}

// Builds a snapshot of all desktop icons (identity + current position) for
// the app's push-out-of-way cache
static void BuildIconSnapshot(HWND hwnd, std::vector<DesktopIconInfo>& out) {
    int itemCount = (int)CallWindowProcW(g_OriginalListViewProc, hwnd, LVM_GETITEMCOUNT, 0, 0);
    out.reserve(itemCount);

    for (int i = 0; i < itemCount; i++) {
        DesktopIconInfo info;

        wchar_t display[MAX_PATH] = {};
        if (GetItemDisplayName(i, display, MAX_PATH)) {
            info.displayName = display;
        }
        wchar_t parsing[PARSING_NAME_CCH] = {};
        if (GetItemParsingName(i, parsing, PARSING_NAME_CCH)) {
            info.parsingName = parsing;
        }
        if (info.displayName.empty() && info.parsingName.empty()) continue;

        POINT pt = {};
        CallWindowProcW(g_OriginalListViewProc, hwnd, LVM_GETITEMPOSITION, i, (LPARAM)&pt);
        info.pt = { pt.x, pt.y };

        out.push_back(std::move(info));
    }
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
    return ShouldHideIconByIndex(hit);
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

        if (!ShouldHideIconByIndex(i)) continue;

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
// Sort keys — fetched in-process via the item's shell PIDL
// ============================================================================

struct DesktopSortKey {
    ULONGLONG size = 0;
    double dateModified = 0.0; // VT_DATE serial value
    wchar_t type[80] = {};
    BOOL isFolder = FALSE;
    BOOL isFileSystem = TRUE;
    BOOL valid = FALSE;
};

// No C++ objects with destructors in here — wrapped by SEH below
static BOOL FetchSortKeyImpl(IShellFolder2* psf2, PCUITEMID_CHILD pidl, DesktopSortKey* out) {
    SFGAOF attrs = SFGAO_FOLDER | SFGAO_FILESYSTEM;
    if (SUCCEEDED(psf2->GetAttributesOf(1, (PCUITEMID_CHILD_ARRAY)&pidl, &attrs))) {
        out->isFolder = (attrs & SFGAO_FOLDER) != 0;
        out->isFileSystem = (attrs & SFGAO_FILESYSTEM) != 0;
    }

    VARIANT v;
    VariantInit(&v);
    if (SUCCEEDED(psf2->GetDetailsEx(pidl, &PKEY_Size, &v))) {
        if (v.vt == VT_UI8) out->size = v.ullVal;
        else if (v.vt == VT_UI4) out->size = v.ulVal;
        VariantClear(&v);
    }
    VariantInit(&v);
    if (SUCCEEDED(psf2->GetDetailsEx(pidl, &PKEY_DateModified, &v))) {
        if (v.vt == VT_DATE) out->dateModified = v.date;
        VariantClear(&v);
    }
    VariantInit(&v);
    if (SUCCEEDED(psf2->GetDetailsEx(pidl, &PKEY_ItemTypeText, &v))) {
        if (v.vt == VT_BSTR && v.bstrVal) wcsncpy_s(out->type, v.bstrVal, _TRUNCATE);
        VariantClear(&v);
    }

    out->valid = TRUE;
    return TRUE;
}

static BOOL FetchSortKeyGuarded(IShellFolder2* psf2, PCUITEMID_CHILD pidl, DesktopSortKey* out) {
    __try {
        return FetchSortKeyImpl(psf2, pidl, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
}

static BOOL FetchSortKey(IShellFolder2* psf2, int index, DesktopSortKey* out) {
    PITEMID_CHILD pidl = GetItemChildPidl(index);
    if (!pidl) return FALSE;
    BOOL ok = FetchSortKeyGuarded(psf2, pidl, out);
    CoTaskMemFree(pidl);
    return ok;
}

// ============================================================================
// Compact visible icons — the hook's own layout/sort engine.
// Explorer never positions icons (LVS_AUTOARRANGE is permanently off); this
// places visible icons into a contiguous grid, optionally sorted, and never
// touches hidden icons.
// ============================================================================

static void CompactVisibleIcons(HWND hwnd, DesktopSortMode mode = DesktopSortMode::PositionOrder) {
    RefreshHiddenIconCache();

    int itemCount = (int)SendMessageW(hwnd, LVM_GETITEMCOUNT, 0, 0);
    if (itemCount == 0) return;

    // Get desktop icon spacing for grid alignment
    DWORD spacing = (DWORD)SendMessageW(hwnd, LVM_GETITEMSPACING, FALSE, 0);
    int spacingX = LOWORD(spacing);
    int spacingY = HIWORD(spacing);
    if (spacingX <= 0) spacingX = 75;
    if (spacingY <= 0) spacingY = 75;

    Log(L"CompactVisibleIcons: Grid spacing = %dx%d, item count = %d, mode = %d",
        spacingX, spacingY, itemCount, (int)mode);

    // Collect visible icons with their current positions
    struct IconPos {
        int index;
        std::wstring name;
        POINT pos;
        DesktopSortKey key;
    };
    std::vector<IconPos> visibleIcons;
    visibleIcons.reserve(itemCount);

    for (int i = 0; i < itemCount; i++) {
        wchar_t buf[MAX_PATH] = {};
        if (!GetItemDisplayName(i, buf, MAX_PATH)) continue;
        if (ShouldHideIconByIndex(i)) continue;

        POINT pt = {};
        SendMessageW(hwnd, LVM_GETITEMPOSITION, i, (LPARAM)&pt);
        visibleIcons.push_back({ i, buf, pt, {} });
    }

    Log(L"CompactVisibleIcons: %zu visible icons (out of %d total)", visibleIcons.size(), itemCount);
    if (visibleIcons.empty()) return;

    if (mode == DesktopSortMode::PositionOrder) {
        // Keep current position order (column-major: X first, then Y)
        std::sort(visibleIcons.begin(), visibleIcons.end(),
            [](const IconPos& a, const IconPos& b) {
                if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
                return a.pos.y < b.pos.y;
            });
    } else {
        // Fetch sort keys via shell PIDLs (in-process, cheap)
        IShellFolder* psf = nullptr;
        IShellFolder2* psf2 = nullptr;
        if (SUCCEEDED(SHGetDesktopFolder(&psf)) && psf) {
            psf->QueryInterface(IID_PPV_ARGS(&psf2));
            psf->Release();
        }
        if (psf2) {
            for (auto& icon : visibleIcons) {
                FetchSortKey(psf2, icon.index, &icon.key);
            }
            psf2->Release();
        }

        // Explorer-like grouping: special shell items first, then folders, then files
        std::sort(visibleIcons.begin(), visibleIcons.end(),
            [mode](const IconPos& a, const IconPos& b) {
                int ga = a.key.valid ? (a.key.isFileSystem ? (a.key.isFolder ? 1 : 2) : 0) : 2;
                int gb = b.key.valid ? (b.key.isFileSystem ? (b.key.isFolder ? 1 : 2) : 0) : 2;
                if (ga != gb) return ga < gb;

                switch (mode) {
                case DesktopSortMode::Size:
                    if (a.key.size != b.key.size) return a.key.size < b.key.size;
                    break;
                case DesktopSortMode::Type: {
                    int cmp = _wcsicmp(a.key.type, b.key.type);
                    if (cmp != 0) return cmp < 0;
                    break;
                }
                case DesktopSortMode::Date:
                    // Newest first, matching Explorer's desktop behavior
                    if (a.key.dateModified != b.key.dateModified)
                        return a.key.dateModified > b.key.dateModified;
                    break;
                default:
                    break; // Name: fall through to name tie-break
                }
                return StrCmpLogicalW(a.name.c_str(), b.name.c_str()) < 0;
            });
    }

    // Get work area for grid layout
    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    int maxRows = (workArea.bottom - workArea.top) / spacingY;
    if (maxRows < 1) maxRows = 20;

    // Reposition visible icons into contiguous grid slots (no gaps), batched
    // into a single repaint via WM_SETREDRAW
    bool anyMoved = false;
    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    for (size_t i = 0; i < visibleIcons.size(); i++) {
        int col = (int)i / maxRows;
        int row = (int)i % maxRows;

        int x = workArea.left + col * spacingX;
        int y = workArea.top + row * spacingY;

        // Only move if position actually changed
        if (visibleIcons[i].pos.x != x || visibleIcons[i].pos.y != y) {
            POINT ptPos = { x, y };
            SendMessageW(hwnd, LVM_SETITEMPOSITION32, visibleIcons[i].index, (LPARAM)&ptPos);
            anyMoved = true;
        }
    }
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    if (anyMoved) {
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    Log(L"CompactVisibleIcons: Placed %zu visible icons (hidden icons untouched, moved=%d)",
        visibleIcons.size(), anyMoved ? 1 : 0);
}

// Timer callback: runs after a burst of item changes (file adds/removes,
// align-to-grid, stray repositioning) has settled
static void CALLBACK SortFixupTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    KillTimer(hwnd, SORT_FIXUP_TIMER_ID);
    g_CompactionPending = false;
    if (g_HookInert) return;

    DWORD exCode = 0;
    __try {
        RefreshHiddenIconCache();

        if (!g_HiddenIcons.empty() || g_OurAutoArrange) {
            Log(L"SortFixupTimer: Deferred compaction (Explorer idle)");
            g_InsideCustomSort = true;
            // When our auto-arrange is on, keep the last sort the user applied
            CompactVisibleIcons(g_hDesktopListView,
                g_OurAutoArrange ? g_LastSortMode : DesktopSortMode::PositionOrder);
            g_InsideCustomSort = false;
        }

        // Tell the app thread to re-park hidden icons under their corrals.
        // The app knows each icon's exact screen position within its corral,
        // which the hook cannot determine. This also prevents hidden icons from
        // acting as mouse-over or drop targets at incorrect desktop positions.
        HWND appWnd = HookBridge::GetAppMessageWindow();
        if (appWnd) {
            PostMessage(appWnd, WM_APP + 100, 0, 0);
            Log(L"SortFixupTimer: Posted repark notification to app");
        }
    } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        g_InsideCustomSort = false;
        GoInertAfterException(L"SortFixupTimerProc", exCode);
    }
}

static void ScheduleCompaction(HWND hwnd, const wchar_t* source) {
    RefreshHiddenIconCache();
    if (g_HiddenIcons.empty() && !g_OurAutoArrange) return;

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

    if (g_HookInert || !IsAutoArrangeOn() || g_InsideCustomSort) return;

    DWORD exCode = 0;
    __try {
        Log(L"AutoSortTimer: Re-compacting after manual icon move");
        g_InsideCustomSort = true;
        CompactVisibleIcons(g_hDesktopListView, g_LastSortMode);
        g_InsideCustomSort = false;

        ShowAutoSortPopup();
    } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        g_InsideCustomSort = false;
        GoInertAfterException(L"AutoSortTimerProc", exCode);
    }
}

// Registered messages: app asks the hook to position icons by shell identity
// or to produce an identity snapshot of all desktop icons
static UINT g_PositionIconsMsg = 0;
static UINT g_IconSnapshotMsg = 0;

static LRESULT CALLBACK ListViewSubclassProcImpl(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // App-requested identity-based positioning (runs here, on the UI thread)
    if (g_PositionIconsMsg != 0 && uMsg == g_PositionIconsMsg) {
        std::vector<IconPositionRequest> requests;
        HookBridge::TakeIconPositionRequests(requests);
        PositionItemsByIdentity(hwnd, requests);
        return 1; // Sentinel: tells the app side the hook handled it
    }

    // App-requested icon snapshot (identity + position per icon)
    if (g_IconSnapshotMsg != 0 && uMsg == g_IconSnapshotMsg) {
        std::vector<DesktopIconInfo> snapshot;
        BuildIconSnapshot(hwnd, snapshot);
        HookBridge::SetIconSnapshot(snapshot);
        return 1;
    }

    // Intercept icon position writes. Hidden (corral-owned) icons are immovable
    // by anything except DexCorral itself: Explorer's arrange/align/sort, other
    // tools, and stray repositioning are all swallowed. This is what keeps
    // hidden icons out of every layout operation.
    if (uMsg == LVM_SETITEMPOSITION || uMsg == LVM_SETITEMPOSITION32) {
        if (!g_InsideCustomSort && !HookBridge::IsAppIconMoveActive()) {
            RefreshHiddenIconCache();
            if (ShouldHideIconByIndex((int)wParam)) {
                Log(L"ListViewSubclassProc: Swallowed position write for hidden icon %d", (int)wParam);
                return TRUE; // Report success; position unchanged
            }
        }

        LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);

        if (g_InsideCustomSort) return result;

        // Debounce: if compaction is pending, icons are still being moved — restart timer
        if (g_CompactionPending) {
            KillTimer(hwnd, SORT_FIXUP_TIMER_ID);
            SetTimer(hwnd, SORT_FIXUP_TIMER_ID, SORT_FIXUP_DELAY_MS, SortFixupTimerProc);
            return result;
        }

        // Detect manual icon moves during auto-arrange (for popup)
        if (uMsg == LVM_SETITEMPOSITION32 && IsAutoArrangeOn() && !HookBridge::IsAppIconMoveActive()) {
            int index = (int)wParam;
            if (!ShouldHideIconByIndex(index)) {
                // Visible icon was moved — save drop position and debounce re-sort
                POINT* pt = (POINT*)lParam;
                g_PendingMoveIconIndex = index;
                g_PendingMoveDropPos = *pt;
                Log(L"ListViewSubclassProc: Visible icon %d moved to (%d,%d) while auto-arrange ON",
                    index, pt->x, pt->y);

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

    // New/removed desktop items shift item indices — invalidate the per-index
    // memo. When our auto-arrange is on, also re-run compaction after the
    // burst settles. The desktop ListView is owner-data, so adds/removes
    // arrive as LVM_SETITEMCOUNT (the classic insert/delete messages are kept
    // for older shells).
    if (uMsg == LVM_SETITEMCOUNT ||
        uMsg == LVM_INSERTITEMW || uMsg == LVM_INSERTITEMA ||
        uMsg == LVM_DELETEITEM || uMsg == LVM_DELETEALLITEMS) {
        LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
        InvalidateHiddenIndexCache();
        if (g_OurAutoArrange && !g_InsideCustomSort) {
            ScheduleCompaction(hwnd, L"item insert/delete");
        }
        return result;
    }

    // Renames (item text) and identity changes (lParam/PIDL) invalidate the memo
    if (uMsg == LVM_SETITEMTEXTW || uMsg == LVM_SETITEMTEXTA) {
        LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
        InvalidateHiddenIndexCache();
        return result;
    }
    if (uMsg == LVM_SETITEMW || uMsg == LVM_SETITEMA) {
        LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
        const LVITEMW* item = (const LVITEMW*)lParam;
        if (item && (item->mask & (LVIF_TEXT | LVIF_PARAM))) {
            InvalidateHiddenIndexCache();
        }
        return result;
    }

    // Intercept LVM_SORTITEMS / LVM_SORTITEMSEX - the actual "Sort by Name/Date/Size/Type"
    // Desktop context menu "Sort by" goes through these messages
    if (uMsg == LVM_SORTITEMS || uMsg == LVM_SORTITEMSEX) {
        RefreshHiddenIconCache();
        const wchar_t* msgName = (uMsg == LVM_SORTITEMS) ? L"LVM_SORTITEMS" : L"LVM_SORTITEMSEX";
        Log(L"ListViewSubclassProc: %s received (wParam=0x%IX, lParam=0x%IX)", msgName, wParam, lParam);

        // Let Windows sort first, then fix up positions. Sorting reindexes
        // items, so the per-index memo is stale afterwards.
        LRESULT result = CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
        InvalidateHiddenIndexCache();
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
        if (hit >= 0 && ShouldHideIconByIndex((int)hit)) {
            // Clear the hit test result so caller thinks nothing was hit
            LVHITTESTINFO* ht = (LVHITTESTINFO*)lParam;
            ht->iItem = -1;
            ht->iSubItem = -1;
            ht->flags = LVHT_NOWHERE;
            Log(L"ListViewSubclassProc: Filtered hidden icon %d from hit test", (int)hit);
            return -1;
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
                if (focused >= 0 && ShouldHideIconByIndex(focused)) {
                    // Skip this hidden icon by sending another arrow key in the same direction
                    Log(L"ListViewSubclassProc: Skipping hidden icon %d on arrow key", focused);
                    CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
                }
                return result;
            }
        }
        break;
    }

    // Prevent Explorer from setting LVS_AUTOARRANGE — we own auto-arrange.
    // No exceptions: Explorer never positions icons, the hook does.
    if (uMsg == WM_STYLECHANGING && wParam == GWL_STYLE) {
        STYLESTRUCT* ss = (STYLESTRUCT*)lParam;
        if ((ss->styleNew & LVS_AUTOARRANGE) && !(ss->styleOld & LVS_AUTOARRANGE)) {
            ss->styleNew &= ~LVS_AUTOARRANGE;
            Log(L"ListViewSubclassProc: Blocked Explorer from setting LVS_AUTOARRANGE");
        }
    }

    return CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
}

// SEH guard around the real proc. No C++ objects with destructors in here
// (C2712). On an escaped exception the hook goes inert and the message is
// passed through — an exception must never propagate into Explorer.
static LRESULT CALLBACK ListViewSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_HookInert) {
        DWORD exCode = 0;
        __try {
            return ListViewSubclassProcImpl(hwnd, uMsg, wParam, lParam);
        } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
            GoInertAfterException(L"ListViewSubclassProc", exCode);
        }
    }
    return CallWindowProcW(g_OriginalListViewProc, hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// ShellDefView subclass - hides icons via NM_CUSTOMDRAW
// ============================================================================

// ============================================================================
// Explorer desktop context menu WM_COMMAND IDs
//
// Empirically observed on Windows 11:
//   31492-31495 = Sort by Name / Size / Item type / Date modified
//   28785       = Auto arrange icons   (toggle)
//   28788       = Align icons to grid  (toggle)
//
// These IDs are undocumented and may vary across Windows builds, so they are
// only the initial fallback: WM_INITMENUPOPUP re-resolves them by scanning the
// menu captions (English shell UI; on localized systems the captions won't
// match and the numeric fallbacks stay in effect).
//
// Detecting toggle state after WM_COMMAND:
//   Auto arrange: GetWindowLongPtrW(ListView, GWL_STYLE) & LVS_AUTOARRANGE
//   Align to grid: SendMessageW(ListView, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0)
//                   & LVS_EX_SNAPTOGRID
//
// Note: The LVM_SORTITEMS / LVM_ARRANGE messages are NOT sent by Explorer on
// Win11; sorting is done internally, so WM_COMMAND is the only interception point.
// ============================================================================

static UINT g_CmdAutoArrange = 28785;
static UINT g_CmdAlignToGrid = 28788;
static UINT g_CmdSortCommands[4] = { 31492, 31493, 31494, 31495 }; // Name, Size, Item type, Date modified

// Menu caption with '&' accelerator markers and trailing shortcut text removed
static bool GetMenuCaption(HMENU hMenu, int pos, wchar_t* buf, int cch) {
    if (GetMenuStringW(hMenu, pos, buf, cch, MF_BYPOSITION) <= 0) return false;
    wchar_t* dst = buf;
    for (const wchar_t* src = buf; *src; src++) {
        if (*src == L'&') continue;
        if (*src == L'\t') break;
        *dst++ = *src;
    }
    *dst = 0;
    return buf[0] != 0;
}

// Re-resolve the undocumented command IDs from the menu Explorer just built.
// Called for every popup of the desktop context menu; only adopts an ID when
// a caption matches, so unrelated submenus are harmless.
static void ResolveDesktopMenuCommandIds(HMENU hMenu) {
    int count = GetMenuItemCount(hMenu);
    if (count <= 0) return;

    // The sort captions "Name"/"Size" are too generic on their own — only
    // trust them when the menu also carries a distinctive sort caption.
    bool looksLikeSortMenu = false;
    for (int i = 0; i < count; i++) {
        wchar_t caption[128];
        if (GetMenuCaption(hMenu, i, caption, 128) &&
            (_wcsicmp(caption, L"Date modified") == 0 || _wcsicmp(caption, L"Item type") == 0)) {
            looksLikeSortMenu = true;
            break;
        }
    }

    for (int i = 0; i < count; i++) {
        UINT id = GetMenuItemID(hMenu, i);
        if (id == (UINT)-1 || id == 0) continue; // submenu or separator

        wchar_t caption[128];
        if (!GetMenuCaption(hMenu, i, caption, 128)) continue;

        UINT* target = nullptr;
        const wchar_t* what = nullptr;
        if (_wcsicmp(caption, L"Auto arrange icons") == 0) {
            target = &g_CmdAutoArrange; what = L"auto-arrange";
        } else if (_wcsicmp(caption, L"Align icons to grid") == 0) {
            target = &g_CmdAlignToGrid; what = L"align-to-grid";
        } else if (looksLikeSortMenu) {
            if      (_wcsicmp(caption, L"Name") == 0)          { target = &g_CmdSortCommands[0]; what = L"sort-by-name"; }
            else if (_wcsicmp(caption, L"Size") == 0)          { target = &g_CmdSortCommands[1]; what = L"sort-by-size"; }
            else if (_wcsicmp(caption, L"Item type") == 0)     { target = &g_CmdSortCommands[2]; what = L"sort-by-type"; }
            else if (_wcsicmp(caption, L"Date modified") == 0) { target = &g_CmdSortCommands[3]; what = L"sort-by-date"; }
            else {
                Log(L"ResolveDesktopMenuCommandIds: unrecognized sort menu item '%s' (id=%u)", caption, id);
            }
        }

        if (target && *target != id) {
            DllLog(L"ResolveDesktopMenuCommandIds: %s command id is %u on this Windows build (expected %u) — adopting menu value",
                   what, id, *target);
            *target = id;
        }
    }
}

static LRESULT CALLBACK ShellDefViewSubclassProcImpl(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

        // The undocumented command IDs vary across Windows builds — re-resolve
        // them from the captions of the menu Explorer just built
        ResolveDesktopMenuCommandIds(hMenu);

        UINT state = GetMenuState(hMenu, g_CmdAutoArrange, MF_BYCOMMAND);
        if (state != (UINT)-1) {
            CheckMenuItem(hMenu, g_CmdAutoArrange, MF_BYCOMMAND | (g_OurAutoArrange ? MF_CHECKED : MF_UNCHECKED));
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
        if (cmdId == g_CmdAutoArrange) {
            g_OurAutoArrange = !g_OurAutoArrange;
            Log(L"  Auto arrange toggled to %s (ours, Explorer not notified)", g_OurAutoArrange ? L"ON" : L"OFF");
            if (g_OurAutoArrange) {
                // Turning ON: compact immediately
                ScheduleCompaction(g_hDesktopListView, L"AutoArrange ON");
            }
            return 0;  // Swallow — Explorer never sees this
        }

        // Sort commands: the hook sorts the desktop itself.
        // Explorer is forwarded the command only so its internal sort state stays
        // consistent — it cannot reposition icons (LVS_AUTOARRANGE stays off and
        // WM_STYLECHANGING blocks re-enabling). Hidden icons never participate.
        int sortIdx = -1;
        for (int i = 0; i < 4; i++) {
            if (cmdId == g_CmdSortCommands[i]) { sortIdx = i; break; }
        }
        if (sortIdx >= 0) {
            static const DesktopSortMode sortModes[] = {
                DesktopSortMode::Name, DesktopSortMode::Size,
                DesktopSortMode::Type, DesktopSortMode::Date
            };
            const wchar_t* sortNames[] = { L"Name", L"Size", L"Item type", L"Date modified" };
            Log(L"  Sort by %s — sorting in-hook (Explorer does not reposition)", sortNames[sortIdx]);

            LRESULT result = CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);
            EnsureExplorerAutoArrangeOff();

            // Win11 Explorer sorts its item list internally (no LVM_SORTITEMS),
            // which reindexes items — the per-index memo is stale
            InvalidateHiddenIndexCache();

            g_LastSortMode = sortModes[sortIdx];
            g_InsideCustomSort = true;
            CompactVisibleIcons(g_hDesktopListView, g_LastSortMode);
            g_InsideCustomSort = false;

            // Backstop: if Explorer still repositions something asynchronously,
            // a deferred compaction restores the contiguous layout (no-op otherwise)
            ScheduleCompaction(g_hDesktopListView, L"Sort command");
            return result;
        }

        // All other commands: let Explorer handle, then compact if needed
        LRESULT result = CallWindowProcW(g_OriginalShellDefViewProc, hwnd, uMsg, wParam, lParam);

        bool needsCompaction = false;
        if (cmdId == g_CmdAlignToGrid) {
            LRESULT exStyle = CallWindowProcW(g_OriginalListViewProc, g_hDesktopListView,
                LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
            bool snapToGrid = (exStyle & LVS_EX_SNAPTOGRID) != 0;
            Log(L"  Align icons to grid is now %s", snapToGrid ? L"ON" : L"OFF");
            needsCompaction = true;
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
            if (nmlv->iItem >= 0 && ShouldHideIconByIndex(nmlv->iItem)) {
                return -1;  // Cancel hot-tracking for this item
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

                if (ShouldHideIconByIndex((int)cd->nmcd.dwItemSpec)) {
                    return CDRF_SKIPDEFAULT;
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

// SEH guard around the real proc. No C++ objects with destructors in here
// (C2712). On an escaped exception the hook goes inert and the message is
// passed through — an exception must never propagate into Explorer.
static LRESULT CALLBACK ShellDefViewSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_HookInert) {
        DWORD exCode = 0;
        __try {
            return ShellDefViewSubclassProcImpl(hwnd, uMsg, wParam, lParam);
        } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
            GoInertAfterException(L"ShellDefViewSubclassProc", exCode);
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

// IDropTarget debug logging (separate file). Gated like Log(): DragOver runs
// at mouse-move frequency during any desktop drag, and an open/write/close per
// tick is far too expensive to leave on in release builds.
static void LogDT(const wchar_t* format, ...) {
    if (!HookBridge::IsDebugLogging()) return;
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

    bool hidden = ShouldHideIconByIndex(hit);
    if (hidden) {
        LogDT(L"IsPointOnHiddenIcon: screen(%d,%d) client(%d,%d) hit=%d -> HIDDEN",
              ptScreen.x, ptScreen.y, ptClient.x, ptClient.y, hit);
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

    // IDropTarget — all four methods are SEH-guarded: they run on Explorer's
    // thread during every desktop drag, and an escaped exception would take
    // Explorer down. When inert, calls are forwarded straight to the original.
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
        POINTL pt, DWORD* pdwEffect) override {
        if (g_HookInert) {
            m_bOverHidden = false;
            m_bOriginalActive = true;
            return m_pOriginal->DragEnter(pDataObj, grfKeyState, pt, pdwEffect);
        }
        DWORD exCode = 0;
        __try {
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
        } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
            GoInertAfterException(L"DropTargetWrapper::DragEnter", exCode);
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (g_HookInert) {
            return m_pOriginal->DragOver(grfKeyState, pt, pdwEffect);
        }
        DWORD exCode = 0;
        __try {
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
        } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
            GoInertAfterException(L"DropTargetWrapper::DragOver", exCode);
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override {
        if (g_HookInert) {
            m_pDataObj = nullptr;
            return m_pOriginal->DragLeave();
        }
        DWORD exCode = 0;
        __try {
            LogDT(L"DragLeave");
            m_bOverHidden = false;
            m_pDataObj = nullptr;
            if (m_bOriginalActive) {
                m_bOriginalActive = false;
                return m_pOriginal->DragLeave();
            }
            return S_OK;
        } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
            GoInertAfterException(L"DropTargetWrapper::DragLeave", exCode);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState,
        POINTL pt, DWORD* pdwEffect) override {
        if (g_HookInert) {
            return m_pOriginal->Drop(pDataObj, grfKeyState, pt, pdwEffect);
        }
        DWORD exCode = 0;
        __try {
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
        } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
            GoInertAfterException(L"DropTargetWrapper::Drop", exCode);
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
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
static HWND g_hDropTargetWindow = nullptr;  // Which window we hooked (ListView, DefView, or their parent)
static UINT_PTR g_DropTargetTimerId = 0;
static int g_DropTargetRetryCount = 0;
static int g_DropTargetRewrapCount = 0;
static const UINT DROP_TARGET_TIMER_ID = 0xDC01;
// Explorer registers the desktop drop target lazily — possibly long after
// login, sometimes only when the first drag starts. Poll fast for a short
// window after init, then keep polling slowly forever instead of giving up.
// The slow cadence doubles as the ownership re-verify interval once installed.
static const int DROP_TARGET_FAST_RETRIES = 30;  // 30 * 500ms = 15s fast phase
static const UINT DROP_TARGET_FAST_MS = 500;
static const UINT DROP_TARGET_VERIFY_MS = 5000;
// If something keeps re-registering the drop target behind us, stop fighting
// after this many re-wraps (each one is DllLog'd, so the tug-of-war is visible)
static const int DROP_TARGET_MAX_REWRAPS = 8;

static bool TryInstallDropTargetHook() {
    if (g_pDropTargetWrapper) return true;  // Already installed

    // Try SysListView32 first, then SHELLDLL_DefView, then their top-level
    // parent (Progman/WorkerW) — OLE dispatches a drag to the first window up
    // the parent chain from the cursor that has a registered drop target
    HWND candidates[] = { g_hDesktopListView, g_hShellDefView,
                          g_hShellDefView ? GetAncestor(g_hShellDefView, GA_PARENT) : nullptr };
    const wchar_t* names[] = { L"SysListView32", L"SHELLDLL_DefView", L"DefView parent" };

    for (int i = 0; i < 3; i++) {
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
            // With DebugLogging on, this line in dllmain.log is the one-stop
            // answer to "is the drop blocker active"
            DllLog(L"CorralHook: drop-target wrapper installed on %s (hwnd=%p, original=%p)",
                   names[i], g_hDropTargetWindow, g_pOriginalDropTarget);
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

// Maintenance timer: runs for the life of the hook. Until the wrapper is
// installed it keeps retrying (Explorer registers the desktop drop target
// lazily, so a fixed retry window is not enough). Once installed it keeps
// verifying we are still the registered target — anything calling
// RegisterDragDrop on the window after us silently removes the wrapper.
static void CALLBACK DropTargetMaintenanceTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (g_HookInert) {
        KillTimer(hwnd, idEvent);
        g_DropTargetTimerId = 0;
        return;
    }

    DWORD exCode = 0;
    __try {
        if (!g_pDropTargetWrapper) {
            // Install phase
            g_DropTargetRetryCount++;
            if (TryInstallDropTargetHook()) {
                LogDT(L"Maintenance: installed after %d retries", g_DropTargetRetryCount);
                SetTimer(hwnd, DROP_TARGET_TIMER_ID, DROP_TARGET_VERIFY_MS, DropTargetMaintenanceTimerProc);
            } else if (g_DropTargetRetryCount == DROP_TARGET_FAST_RETRIES) {
                DllLog(L"CorralHook: drop target not registered by Explorer after %d fast retries — "
                       L"continuing to poll every %u ms", g_DropTargetRetryCount, DROP_TARGET_VERIFY_MS);
                SetTimer(hwnd, DROP_TARGET_TIMER_ID, DROP_TARGET_VERIFY_MS, DropTargetMaintenanceTimerProc);
            }
            return;
        }

        // Verify phase: are we still the registered drop target?
        if (!IsWindow(g_hDropTargetWindow)) {
            DllLog(L"CorralHook: drop-target window %p destroyed — wrapper lost, retrying install",
                   g_hDropTargetWindow);
            g_pDropTargetWrapper->Release();
            g_pDropTargetWrapper = nullptr;
            g_pOriginalDropTarget = nullptr;
            g_hDropTargetWindow = nullptr;
            return; // Next tick re-enters the install phase
        }

        IDropTarget* current = (IDropTarget*)GetPropW(g_hDropTargetWindow, L"OleDropTargetInterface");
        if (current == g_pDropTargetWrapper) {
            return; // Still ours
        }

        if (g_DropTargetRewrapCount >= DROP_TARGET_MAX_REWRAPS) {
            return; // Cap reached earlier — leave the foreign target alone (already logged)
        }
        g_DropTargetRewrapCount++;
        DllLog(L"CorralHook: drop target re-registered behind us (now %p) — re-wrapping (%d/%d)",
               current, g_DropTargetRewrapCount, DROP_TARGET_MAX_REWRAPS);
        if (g_DropTargetRewrapCount == DROP_TARGET_MAX_REWRAPS) {
            DllLog(L"CorralHook: re-wrap cap reached — leaving the registered drop target in place from now on");
        }

        // Drop our bookkeeping and wrap whatever is registered now. The new
        // owner stays functional — it becomes the wrapper's forward target.
        g_pDropTargetWrapper->Release();
        g_pDropTargetWrapper = nullptr;
        g_pOriginalDropTarget = nullptr;
        g_hDropTargetWindow = nullptr;
        TryInstallDropTargetHook();
    } __except (exCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        KillTimer(hwnd, idEvent);
        g_DropTargetTimerId = 0;
        GoInertAfterException(L"DropTargetMaintenanceTimerProc", exCode);
    }
}

static void InstallDropTargetHook() {
    LogDT(L"InstallDropTargetHook: ListView=%p ShellDefView=%p", g_hDesktopListView, g_hShellDefView);

    if (!g_hDesktopListView) {
        DllLog(L"CorralHook: drop-target wrapper not installed — no desktop ListView");
        return;
    }

    bool installed = TryInstallDropTargetHook();
    if (!installed) {
        LogDT(L"InstallDropTargetHook: drop target not registered yet — deferring to maintenance timer");
    }

    // The maintenance timer runs for the life of the hook: it finishes the
    // install once Explorer registers its drop target (which happens lazily,
    // sometimes only when the first drag starts) and afterwards re-verifies
    // that the wrapper is still the registered target.
    g_DropTargetRetryCount = 0;
    g_DropTargetRewrapCount = 0;
    g_DropTargetTimerId = SetTimer(g_hDesktopListView, DROP_TARGET_TIMER_ID,
        installed ? DROP_TARGET_VERIFY_MS : DROP_TARGET_FAST_MS, DropTargetMaintenanceTimerProc);
    if (!g_DropTargetTimerId) {
        DllLog(L"CorralHook: drop-target maintenance SetTimer FAILED, GetLastError=%d", GetLastError());
    }
}

static void UninstallDropTargetHook() {
    if (g_DropTargetTimerId && g_hDesktopListView) {
        KillTimer(g_hDesktopListView, g_DropTargetTimerId);
        g_DropTargetTimerId = 0;
        LogDT(L"UninstallDropTargetHook: Killed maintenance timer");
    }

    if (!g_pDropTargetWrapper || !g_hDropTargetWindow) {
        LogDT(L"UninstallDropTargetHook: Nothing to restore (wrapper=%p, window=%p)",
              g_pDropTargetWrapper, g_hDropTargetWindow);
        return;
    }

    if (IsWindow(g_hDropTargetWindow)) {
        // Only restore if we are still the registered target — overwriting a
        // foreign registration would break whoever installed it after us
        // (same policy as the WNDPROC restore in CleanupCorralHook)
        IDropTarget* current = (IDropTarget*)GetPropW(g_hDropTargetWindow, L"OleDropTargetInterface");
        if (current == g_pDropTargetWrapper) {
            RevokeDragDrop(g_hDropTargetWindow);
            RegisterDragDrop(g_hDropTargetWindow, g_pOriginalDropTarget);
            LogDT(L"UninstallDropTargetHook: Original IDropTarget restored on %p", g_hDropTargetWindow);
        } else {
            DllLog(L"CorralHook: drop target no longer ours at cleanup (%p) — leaving it in place", current);
        }
    }

    g_pDropTargetWrapper->Release();
    g_pDropTargetWrapper = nullptr;
    g_pOriginalDropTarget = nullptr;
    g_hDropTargetWindow = nullptr;
}

// ============================================================================
// Initialization & Cleanup
// ============================================================================

bool InitializeCorralHook() {
    // Safe mode: if recent sessions kept dying with the hook installed, skip
    // installation entirely for this session. Returning true stops the retry
    // thread; the app still runs, with all desktop icons left visible.
    if (!EvaluateSafeMode()) {
        return true;
    }

    HWND shellDefView = nullptr;
    g_hDesktopListView = FindDesktopListView(&shellDefView);

    if (!g_hDesktopListView || !shellDefView) {
        Log(L"InitializeCorralHook: Desktop ListView not found");
        return false;
    }

    g_hShellDefView = shellDefView;
    Log(L"InitializeCorralHook: Found ListView=%p, ShellDefView=%p",
        g_hDesktopListView, g_hShellDefView);

    // Arm the crash sentinel before touching Explorer's window procs. Cleared
    // by the stability timer below or by a clean shutdown; if Explorer dies
    // first, the next session counts it as a hook failure.
    WriteSafeModeValue(REGVAL_HOOK_PENDING, 1);
    g_CrashSentinelArmed = true;

    // Subclass SHELLDLL_DefView for NM_CUSTOMDRAW (icon hiding)
    g_OriginalShellDefViewProc = (WNDPROC)SetWindowLongPtrW(
        g_hShellDefView, GWLP_WNDPROC, (LONG_PTR)ShellDefViewSubclassProc);

    // Subclass SysListView32 for input filtering on hidden icons
    g_OriginalListViewProc = (WNDPROC)SetWindowLongPtrW(
        g_hDesktopListView, GWLP_WNDPROC, (LONG_PTR)ListViewSubclassProc);

    g_HookActive = true;

    // Explorer surviving this long with our hooks installed counts as stable
    SetTimer(g_hDesktopListView, STABILITY_TIMER_ID, STABILITY_DELAY_MS, StabilityTimerProc);

    // Take over auto-arrange: capture Explorer's current state, then disable it
    LONG_PTR lvStyle = GetWindowLongPtrW(g_hDesktopListView, GWL_STYLE);
    g_OurAutoArrange = (lvStyle & LVS_AUTOARRANGE) != 0;
    if (g_OurAutoArrange) {
        SetWindowLongPtrW(g_hDesktopListView, GWL_STYLE, lvStyle & ~LVS_AUTOARRANGE);
    }
    Log(L"InitializeCorralHook: Took over auto-arrange (was %s)", g_OurAutoArrange ? L"ON" : L"OFF");

    // Register popup window class for auto-sort dialog
    RegisterAutoSortPopupClass();

    // Registered messages for identity-based icon positioning and snapshots
    g_PositionIconsMsg = RegisterWindowMessageW(L"DexCorral_PositionIconsByPath");
    g_IconSnapshotMsg = RegisterWindowMessageW(L"DexCorral_GetIconSnapshot");

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
        KillTimer(g_hDesktopListView, REVALIDATE_TIMER_ID);
        KillTimer(g_hDesktopListView, STABILITY_TIMER_ID);
    }
    UnregisterClassW(POPUP_CLASS_NAME, GetModuleHandleW(nullptr));

    // Restore IDropTarget before removing subclass (drop target uses subclass proc)
    UninstallDropTargetHook();

    // Restore the subclasses, innermost first — but only when the current
    // WNDPROC is still ours. If another tool subclassed after us, restoring
    // would cut it out of the chain and break it; the safe move is to leave
    // our proc installed but inert (pure pass-through). The DLL never unloads
    // while Explorer runs (DllCanUnloadNow → S_FALSE), so the proc stays valid.
    if (g_hDesktopListView && g_OriginalListViewProc) {
        WNDPROC current = (WNDPROC)GetWindowLongPtrW(g_hDesktopListView, GWLP_WNDPROC);
        if (current == ListViewSubclassProc) {
            SetWindowLongPtrW(g_hDesktopListView, GWLP_WNDPROC, (LONG_PTR)g_OriginalListViewProc);
            g_OriginalListViewProc = nullptr;
        } else {
            InterlockedExchange(&g_HookInert, 1);
            DllLog(L"CleanupCorralHook: ListView WNDPROC is no longer ours (%p) — leaving subclass in place, inert", current);
            // g_OriginalListViewProc stays set: the inert proc still forwards to it
        }
    }

    if (g_hShellDefView && g_OriginalShellDefViewProc) {
        WNDPROC current = (WNDPROC)GetWindowLongPtrW(g_hShellDefView, GWLP_WNDPROC);
        if (current == ShellDefViewSubclassProc) {
            SetWindowLongPtrW(g_hShellDefView, GWLP_WNDPROC, (LONG_PTR)g_OriginalShellDefViewProc);
            g_OriginalShellDefViewProc = nullptr;
        } else {
            InterlockedExchange(&g_HookInert, 1);
            DllLog(L"CleanupCorralHook: ShellDefView WNDPROC is no longer ours (%p) — leaving subclass in place, inert", current);
        }
    }

    // Restore Explorer's auto-arrange if we had it on
    if (g_OurAutoArrange && g_hDesktopListView) {
        LONG_PTR style = GetWindowLongPtrW(g_hDesktopListView, GWL_STYLE);
        SetWindowLongPtrW(g_hDesktopListView, GWL_STYLE, style | LVS_AUTOARRANGE);
        Log(L"CleanupCorralHook: Restored LVS_AUTOARRANGE to Explorer");
    }
    g_OurAutoArrange = false;

    ReleaseDesktopFolderView();

    g_HookActive = false;
    g_HiddenIcons.clear();
    InvalidateHiddenIndexCache();
    g_LastVersion = 0;

    // Refresh desktop to show all icons again
    if (g_hDesktopListView) {
        RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }

    // Reaching a clean shutdown means this session did not crash
    MarkHookSessionStable(L"clean shutdown");

    Log(L"CleanupCorralHook: Hook cleaned up");
}

bool IsCorralHookActive() { return g_HookActive; }

bool IsCorralHookSafeMode() { return g_SafeMode; }
