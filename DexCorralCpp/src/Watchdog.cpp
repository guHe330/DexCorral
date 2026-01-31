// DexCorralCpp Watchdog
// Monitors the main application and restarts it if it crashes.
// Does NOT restart if main app signals graceful exit (user Exit or Windows shutdown).

#include <Windows.h>
#include <shellapi.h>
#include <string>

static const wchar_t* GRACEFUL_EXIT_EVENT = L"DexCorralCppGracefulExit";
static const wchar_t* WATCHDOG_MUTEX = L"DexCorralCppWatchdogMutex";

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Prevent multiple watchdog instances
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, WATCHDOG_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // Parse command line for main app PID
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc < 2) {
        LocalFree(argv);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    DWORD mainPid = (DWORD)_wtoi(argv[1]);
    LocalFree(argv);

    if (mainPid == 0) {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // Open handle to main process
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, mainPid);
    if (!hProcess) {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // Open graceful exit event NOW (before process terminates)
    // This keeps the event object alive even after main process exits
    HANDLE hEvent = OpenEventW(SYNCHRONIZE, FALSE, GRACEFUL_EXIT_EVENT);

    // Wait for main process to terminate
    WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);

    // Check if it was a graceful exit
    if (hEvent) {
        DWORD result = WaitForSingleObject(hEvent, 0);
        CloseHandle(hEvent);

        if (result == WAIT_OBJECT_0) {
            // Graceful exit - don't restart
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
    }

    // Crash detected - restart main application
    // Get path to main exe (watchdog is in same directory)
    wchar_t watchdogPath[MAX_PATH];
    GetModuleFileNameW(nullptr, watchdogPath, MAX_PATH);

    // Replace "Watchdog.exe" with ".exe" in path
    std::wstring mainPath(watchdogPath);
    size_t pos = mainPath.rfind(L"Watchdog.exe");
    if (pos != std::wstring::npos) {
        mainPath.replace(pos, 12, L".exe");
    } else {
        // Fallback: just look for DexCorralCpp.exe in same directory
        pos = mainPath.rfind(L'\\');
        if (pos != std::wstring::npos) {
            mainPath = mainPath.substr(0, pos + 1) + L"DexCorralCpp.exe";
        }
    }

    // Brief delay before restart
    Sleep(500);

    // Restart main application
    ShellExecuteW(nullptr, L"open", mainPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    if (hMutex) CloseHandle(hMutex);
    return 0;
}
