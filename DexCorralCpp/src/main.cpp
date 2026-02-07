#include "App.h"
#include <Windows.h>
#include <CommCtrl.h>

#pragma comment(lib, "comctl32.lib")

static const wchar_t* MUTEX_NAME = L"DexCorralSingleInstanceMutex";

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Single instance check
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running
        if (hMutex) CloseHandle(hMutex);
        return 0;
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
        if (hMutex) CloseHandle(hMutex);
        return result;
    }
    catch (...) {
        OleUninitialize();
        if (hMutex) CloseHandle(hMutex);
        MessageBoxW(nullptr, L"Fatal error occurred", L"DexCorral Error", MB_ICONERROR);
        return 1;
    }
}
