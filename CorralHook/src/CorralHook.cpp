/**
 * CorralHook.cpp - Explorer.exe hook for hiding desktop icons
 *
 * Implements a DLL injected into explorer.exe that subclasses the desktop ListView
 * to hide icons owned by corrals. Uses pure draw suppression (CDRF_SKIPDEFAULT) for
 * invisible icons, input filtering to prevent interaction with hidden icons, and
 * shared memory IPC for receiving the list of hidden icons from DexCorral.exe.
 */

#include "CorralHook.h"
#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <oleidl.h>
#include <ole2.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")

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

static LRESULT CALLBACK ListViewSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
// IDropTarget wrapper - intercepts OLE drag-drop on hidden icons
// Explorer's internal IDropTarget does hit-testing via direct function calls
// (not SendMessage), so our LVM_HITTEST interception doesn't catch it.
// We wrap the original IDropTarget and reject drops on hidden icons.
// ============================================================================

static IDropTarget* g_pOriginalDropTarget = nullptr;

// Always-on logging for IDropTarget debugging (writes to separate file)
static void LogDT(const wchar_t* format, ...) {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"CorralHook_DropTarget.log");

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
        RefreshHiddenIconCache();
        POINT ptScreen = { pt.x, pt.y };
        if (IsPointOnHiddenIcon(ptScreen)) {
            m_bOverHidden = true;
            DWORD tempEffect = *pdwEffect;
            m_pOriginal->DragEnter(pDataObj, grfKeyState, pt, &tempEffect);
            *pdwEffect = DROPEFFECT_NONE;
            LogDT(L"DragEnter: BLOCKED (hidden icon)");
            return S_OK;
        }
        m_bOverHidden = false;
        HRESULT hr = m_pOriginal->DragEnter(pDataObj, grfKeyState, pt, pdwEffect);
        LogDT(L"DragEnter: forwarded, effect=0x%X hr=0x%08X", *pdwEffect, hr);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        RefreshHiddenIconCache();
        POINT ptScreen = { pt.x, pt.y };
        if (IsPointOnHiddenIcon(ptScreen)) {
            if (!m_bOverHidden) {
                LogDT(L"DragOver: entering hidden icon at (%d,%d)", pt.x, pt.y);
            }
            m_bOverHidden = true;
            DWORD tempEffect = *pdwEffect;
            m_pOriginal->DragOver(grfKeyState, pt, &tempEffect);
            *pdwEffect = DROPEFFECT_NONE;
            return S_OK;
        }
        if (m_bOverHidden) {
            LogDT(L"DragOver: leaving hidden icon at (%d,%d)", pt.x, pt.y);
        }
        m_bOverHidden = false;
        return m_pOriginal->DragOver(grfKeyState, pt, pdwEffect);
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override {
        LogDT(L"DragLeave");
        m_bOverHidden = false;
        m_pDataObj = nullptr;
        return m_pOriginal->DragLeave();
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState,
        POINTL pt, DWORD* pdwEffect) override {
        LogDT(L"Drop: pt=(%d,%d) effect=0x%X", pt.x, pt.y, *pdwEffect);
        RefreshHiddenIconCache();
        POINT ptScreen = { pt.x, pt.y };
        if (IsPointOnHiddenIcon(ptScreen)) {
            LogDT(L"Drop: BLOCKED (hidden icon)");
            *pdwEffect = DROPEFFECT_NONE;
            m_pOriginal->DragLeave();
            return S_OK;
        }
        HRESULT hr = m_pOriginal->Drop(pDataObj, grfKeyState, pt, pdwEffect);
        LogDT(L"Drop: forwarded, effect=0x%X hr=0x%08X", *pdwEffect, hr);
        return hr;
    }

private:
    IDropTarget* m_pOriginal;
    IDataObject* m_pDataObj = nullptr;
    LONG m_refCount = 1;
    bool m_bOverHidden = false;
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

    // Open shared memory and do initial read
    EnsureMappingOpen();
    g_LastVersion = 0;  // Force re-read
    RefreshHiddenIconCache();

    // Hook IDropTarget to intercept OLE drag-drop on hidden icons
    InstallDropTargetHook();

    // Trigger repaint so hidden icons disappear
    RedrawWindow(g_hDesktopListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

    Log(L"InitializeCorralHook: Hook active, %d icons to hide", (int)g_HiddenIcons.size());
    return true;
}

void CleanupCorralHook() {
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
