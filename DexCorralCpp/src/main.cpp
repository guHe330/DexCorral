#include "App.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <shellapi.h>

#pragma comment(lib, "comctl32.lib")

static const wchar_t* MUTEX_NAME = L"DexCorralSingleInstanceMutex";
static const wchar_t* DEBUG_EVENT_NAME = L"Local\\DexCorralDebug";

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Single instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // Check for -debug flag — creates a named event the hook DLL checks for logging
    HANDLE hDebugEvent = nullptr;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++) {
            if (_wcsicmp(argv[i], L"-debug") == 0 || _wcsicmp(argv[i], L"--debug") == 0) {
                hDebugEvent = CreateEventW(nullptr, TRUE, TRUE, DEBUG_EVENT_NAME);
                break;
            }
        }
        LocalFree(argv);
    }

    // Initialize common controls (for trackbar/slider)
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // Initialize OLE (superset of COM, required for drag-drop)
    OleInitialize(nullptr);

    try {
        App app;
        int result = app.Run();
        OleUninitialize();
        if (hDebugEvent) CloseHandle(hDebugEvent);
        if (hMutex) CloseHandle(hMutex);
        return result;
    }
    catch (...) {
        OleUninitialize();
        if (hDebugEvent) CloseHandle(hDebugEvent);
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr, L"Fatal error occurred", L"DexCorral Error", MB_ICONERROR);
        return 1;
    }
}
