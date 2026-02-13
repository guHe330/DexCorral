#include <Windows.h>
#include <CommCtrl.h>
#include <stdio.h>
#include <new>
#include "CorralHook.h"
#include "ClassFactory.h"
#include "Registration.h"
#include "Guids.h"

#pragma comment(lib, "comctl32.lib")

static const wchar_t* DEBUG_EVENT_NAME = L"Local\\DexCorralDebug";
static HMODULE g_hModule = nullptr;
static HANDLE g_hWorkerThread = nullptr;
static HANDLE g_hStopEvent = nullptr;  // Signaled to tell worker thread to quit
static bool g_bInsideExplorer = false; // Only activate hook+app when loaded by Explorer

// RunApp is defined in App.cpp — compiled into the same DLL
extern "C" int RunApp(HANDLE hStopEvent);

static bool IsDebugEnabled() {
    HANDLE hEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, DEBUG_EVENT_NAME);
    if (hEvent) {
        CloseHandle(hEvent);
        return true;
    }
    return false;
}

// Log to file for debugging (since we can't use console in Explorer)
static void Log(const wchar_t* message) {
    if (!IsDebugEnabled()) return;

    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"CorralHook.log");

    HANDLE hFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t buffer[512];
        swprintf_s(buffer, L"[%02d:%02d:%02d.%03d] %s\r\n",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);

        DWORD written;
        WriteFile(hFile, buffer, (DWORD)(wcslen(buffer) * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(hFile);
    }
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
    Log(L"AppWorkerThread: Starting");

    OleInitialize(nullptr);

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    RunApp(g_hStopEvent);

    OleUninitialize();
    Log(L"AppWorkerThread: Exiting");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        g_bInsideExplorer = IsRunningInsideExplorer();
        Log(L"CorralHook: DLL_PROCESS_ATTACH");

        if (!g_bInsideExplorer) {
            // Loaded by registration tool — only COM exports needed, no hook/app
            Log(L"CorralHook: Not in Explorer, skipping hook and app initialization");
            break;
        }

        if (InitializeCorralHook()) {
            Log(L"CorralHook: Initialized successfully");
        } else {
            Log(L"CorralHook: Failed to initialize");
        }

        // Create stop event for worker thread
        g_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        // Launch worker thread for App message loop
        g_hWorkerThread = CreateThread(nullptr, 0, AppWorkerThread, nullptr, 0, nullptr);
        if (g_hWorkerThread) {
            Log(L"CorralHook: Worker thread launched");
        } else {
            Log(L"CorralHook: Failed to launch worker thread");
        }
        break;

    case DLL_PROCESS_DETACH:
        Log(L"CorralHook: DLL_PROCESS_DETACH");

        if (!g_bInsideExplorer) break;  // Nothing to clean up

        // Signal worker thread to stop
        if (g_hStopEvent) {
            SetEvent(g_hStopEvent);
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
        break;
    }
    return TRUE;
}

// === COM Exports ===

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (IsEqualCLSID(rclsid, CLSID_DexCorralShellExt)) {
        auto* factory = new (std::nothrow) DexCorralClassFactory();
        if (!factory) return E_OUTOFMEMORY;

        HRESULT hr = factory->QueryInterface(riid, ppv);
        factory->Release();
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
