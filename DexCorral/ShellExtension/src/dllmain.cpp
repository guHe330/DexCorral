#include <Windows.h>
#include <CommCtrl.h>
#include <ShlObj.h>
#include <stdio.h>
#include <new>
#include "CorralHook.h"
#include "HookBridge.h"
#include "ClassFactory.h"
#include "Registration.h"
#include "Guids.h"

#pragma comment(lib, "comctl32.lib")

static HMODULE g_hModule = nullptr;
static HANDLE g_hWorkerThread = nullptr;
static HANDLE g_hHookRetryThread = nullptr;
static HANDLE g_hStopEvent = nullptr;  // Signaled to tell worker thread to quit
static bool g_bInsideExplorer = false; // Only activate hook+app when loaded by Explorer
static LONG g_AppStarted = 0;          // Set to 1 once StartAppIfNeeded() has run

// RunApp is defined in App.cpp — compiled into the same DLL
extern "C" int RunApp(HANDLE hStopEvent);

// Debug log — gated by the DebugLogging config flag like every other log
// file. HookBridge::IsDebugLogging bootstraps the flag straight from
// config.json on first use, so startup failures that happen before the App
// reads the config are still captured when DebugLogging is enabled.
// Non-static so ShellExtension.cpp (same DLL) can call it via forward declaration.
static wchar_t g_DllMainLogPath[MAX_PATH] = {};

static void InitDllLogPath() {
    if (g_DllMainLogPath[0]) return;
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
        wchar_t dir[MAX_PATH];
        swprintf_s(dir, L"%s\\DexCorral", appData);
        CreateDirectoryW(dir, nullptr);
        swprintf_s(g_DllMainLogPath, L"%s\\dllmain.log", dir);
        CoTaskMemFree(appData);
    } else {
        GetTempPathW(MAX_PATH, g_DllMainLogPath);
        wcscat_s(g_DllMainLogPath, L"dllmain.log");
    }
}

void DllLog(const wchar_t* format, ...) {
    if (!HookBridge::IsDebugLogging()) return;
    InitDllLogPath();

    wchar_t message[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(message, format, args);
    va_end(args);

    HANDLE hFile = CreateFileW(g_DllMainLogPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buffer[1200];
        swprintf_s(buffer, L"[%02d:%02d:%02d.%03d] %s\r\n",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);

        DWORD written;
        WriteFile(hFile, buffer, (DWORD)(wcslen(buffer) * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(hFile);
    }
}

// Kept as alias so internal callers don't all need renaming
static void Log(const wchar_t* format, ...) {
    if (!HookBridge::IsDebugLogging()) return;
    wchar_t message[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(message, format, args);
    va_end(args);
    DllLog(L"%s", message);
}

static bool IsRunningInsideExplorer() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    const wchar_t* exeName = lastSlash ? lastSlash + 1 : exePath;
    return _wcsicmp(exeName, L"explorer.exe") == 0;
}

// Worker thread: runs the App message loop (corral windows, tray icon, config, etc.)
static DWORD WINAPI AppWorkerThread(LPVOID param) {
    Log(L"AppWorkerThread: Starting (tid=%lu)", GetCurrentThreadId());

    HRESULT hrOle = OleInitialize(nullptr);
    Log(L"AppWorkerThread: OleInitialize hr=0x%08X", (unsigned)hrOle);

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    BOOL iccOk = InitCommonControlsEx(&icc);
    Log(L"AppWorkerThread: InitCommonControlsEx=%d", iccOk);

    Log(L"AppWorkerThread: Calling RunApp...");
    int ret = RunApp(g_hStopEvent);
    Log(L"AppWorkerThread: RunApp returned %d", ret);

    OleUninitialize();
    Log(L"AppWorkerThread: Exiting");
    return 0;
}

// Custom message to tell the App to refresh hidden icons after deferred hook init
static const UINT WM_HOOK_READY = WM_APP + 100;

// Retry thread: periodically attempts InitializeCorralHook() when the desktop
// ListView isn't available yet (e.g., Explorer still starting up after PC restart).
static DWORD WINAPI HookRetryThread(LPVOID param) {
    const int MAX_RETRIES = 60;       // 60 retries
    const DWORD RETRY_INTERVAL = 500; // 500ms between retries = 30 seconds total

    for (int i = 0; i < MAX_RETRIES; i++) {
        // Check if we should stop (DLL unloading)
        if (WaitForSingleObject(g_hStopEvent, RETRY_INTERVAL) != WAIT_TIMEOUT) {
            Log(L"HookRetryThread: Stop event signaled, aborting");
            return 0;
        }

        if (InitializeCorralHook()) {
            Log(L"HookRetryThread: Hook initialized successfully");

            // Notify the App to re-push hidden icon data now that the hook is active
            HWND hAppMsg = FindWindowW(L"DexCorralMessageWindow", nullptr);
            if (hAppMsg) {
                PostMessageW(hAppMsg, WM_HOOK_READY, 0, 0);
                Log(L"HookRetryThread: Posted WM_HOOK_READY to App");
            }
            return 0;
        }
    }

    Log(L"HookRetryThread: Gave up after all retries");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        g_bInsideExplorer = IsRunningInsideExplorer();

        // Initialize HookBridge (critical section) regardless of context
        HookBridge::Initialize();
        HookBridge::SetDllModule(hModule);

        {
            wchar_t exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            Log(L"CorralHook: DLL_PROCESS_ATTACH — host=%s insideExplorer=%d pid=%lu",
                exePath, g_bInsideExplorer, GetCurrentProcessId());
        }

        // Do NOT start threads here. The loader lock is held during DllMain — starting
        // threads that call OleInitialize() or load DLLs risks a deadlock, causing
        // Explorer to silently abort the load when loaded via SharedTaskScheduler.
        // Actual startup is deferred to StartAppIfNeeded(), called from Exec() (the
        // SharedTaskScheduler path) or overlay handler methods (GetPriority/IsMemberOf).
        Log(L"CorralHook: DLL_PROCESS_ATTACH done — waiting for deferred StartAppIfNeeded");
        break;

    case DLL_PROCESS_DETACH:
        Log(L"CorralHook: DLL_PROCESS_DETACH (appStarted=%d)", g_AppStarted);

        if (!g_bInsideExplorer || !g_AppStarted) break;  // Nothing to clean up

        // Signal worker thread to stop
        if (g_hStopEvent) {
            SetEvent(g_hStopEvent);
        }

        // Wait for hook retry thread to finish
        if (g_hHookRetryThread) {
            WaitForSingleObject(g_hHookRetryThread, 2000);
            CloseHandle(g_hHookRetryThread);
            g_hHookRetryThread = nullptr;
        }

        // Wait for worker thread to finish (up to 5 seconds)
        if (g_hWorkerThread) {
            WaitForSingleObject(g_hWorkerThread, 5000);
            CloseHandle(g_hWorkerThread);
            g_hWorkerThread = nullptr;
        }

        if (g_hStopEvent) {
            CloseHandle(g_hStopEvent);
            g_hStopEvent = nullptr;
        }

        CleanupCorralHook();
        HookBridge::Cleanup();
        break;
    }
    return TRUE;
}

// === Deferred startup ===

// Called from Exec() (SharedTaskScheduler) or overlay handler methods or right-click.
// At that point the loader lock is no longer held, so thread creation is safe.
// caller is a short string identifying the callsite, e.g. L"Exec", L"GetPriority".
void StartAppIfNeeded(const wchar_t* caller) {
    if (!g_bInsideExplorer) {
        DllLog(L"StartAppIfNeeded [%s]: not inside Explorer, skipping", caller);
        return;
    }
    if (InterlockedCompareExchange(&g_AppStarted, 1, 0) != 0) {
        DllLog(L"StartAppIfNeeded [%s]: already started, no-op", caller);
        return;
    }

    DllLog(L"StartAppIfNeeded [%s]: FIRST CALL — launching hook + App threads", caller);

    g_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_hStopEvent) {
        DllLog(L"StartAppIfNeeded [%s]: CreateEvent FAILED err=%lu", caller, GetLastError());
    }

    if (InitializeCorralHook()) {
        DllLog(L"StartAppIfNeeded [%s]: Hook initialized immediately", caller);
    } else {
        DllLog(L"StartAppIfNeeded [%s]: Desktop ListView not ready, starting retry thread", caller);
        g_hHookRetryThread = CreateThread(nullptr, 0, HookRetryThread, nullptr, 0, nullptr);
        if (!g_hHookRetryThread)
            DllLog(L"StartAppIfNeeded [%s]: HookRetryThread CreateThread FAILED err=%lu", caller, GetLastError());
        else
            DllLog(L"StartAppIfNeeded [%s]: HookRetryThread started tid=%lu", caller, GetThreadId(g_hHookRetryThread));
    }

    g_hWorkerThread = CreateThread(nullptr, 0, AppWorkerThread, nullptr, 0, nullptr);
    if (g_hWorkerThread) {
        DllLog(L"StartAppIfNeeded [%s]: AppWorkerThread started tid=%lu", caller, GetThreadId(g_hWorkerThread));
    } else {
        DllLog(L"StartAppIfNeeded [%s]: AppWorkerThread CreateThread FAILED err=%lu", caller, GetLastError());
    }
}

// === Startup injection hook ===

// Called by DexCorral.exe --startup via SetWindowsHookEx(WH_GETMESSAGE, ..., explorerThreadId).
// Windows injects this DLL into Explorer's address space. On the first message processed by
// Explorer's Progman thread, this proc fires, starts the App, and returns. DexCorral.exe
// then calls UnhookWindowsHookEx and exits — the DLL stays resident (DllCanUnloadNow → S_FALSE).
extern "C" __declspec(dllexport) LRESULT CALLBACK WakeHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    // Only call StartAppIfNeeded once — g_AppStarted goes 0→1 on first call and
    // stays 1 thereafter, so we skip the function entirely for every subsequent message.
    if (code >= 0 && g_AppStarted == 0)
        StartAppIfNeeded(L"WakeHookProc");
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

// === COM Exports ===

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualCLSID(rclsid, CLSID_DexCorralShellExt)) {
        DllLog(L"DllGetClassObject: Creating DexCorralClassFactory (pid=%lu)", GetCurrentProcessId());
        auto* factory = new (std::nothrow) DexCorralClassFactory();
        if (!factory) return E_OUTOFMEMORY;

        HRESULT hr = factory->QueryInterface(riid, ppv);
        factory->Release();
        DllLog(L"DllGetClassObject: QueryInterface hr=0x%08X", (unsigned)hr);
        return hr;
    }

    *ppv = nullptr;
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() {
    // Never unload while Explorer is running — our hook is active
    return S_FALSE;
}

STDAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);
    return RegisterShellExtension(dllPath);
}

STDAPI DllUnregisterServer() {
    return UnregisterShellExtension();
}

// Legacy export — kept for compatibility
extern "C" __declspec(dllexport) BOOL IsHookInstalled() {
    return IsCorralHookActive() ? TRUE : FALSE;
}
