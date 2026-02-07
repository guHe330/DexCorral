#include <Windows.h>
#include <stdio.h>
#include "CorralHook.h"

static const wchar_t* DEBUG_EVENT_NAME = L"Local\\DexCorralDebug";

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

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        Log(L"CorralHook: DLL_PROCESS_ATTACH");

        if (InitializeCorralHook()) {
            Log(L"CorralHook: Initialized successfully");
        } else {
            Log(L"CorralHook: Failed to initialize");
        }
        break;

    case DLL_PROCESS_DETACH:
        Log(L"CorralHook: DLL_PROCESS_DETACH");
        CleanupCorralHook();
        break;
    }
    return TRUE;
}

// Exported function to check if hook is working
extern "C" __declspec(dllexport) BOOL IsHookInstalled() {
    return IsCorralHookActive() ? TRUE : FALSE;
}
