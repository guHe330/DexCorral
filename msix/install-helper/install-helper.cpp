// install-helper.exe
// Custom install/uninstall actions for DexCorral MSIX package
// Handles Explorer hook registration and cleanup
//
// This executable is called by the MSIX installer to perform actions that
// require elevation or special handling (like COM registration, hook setup, etc.)

#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <iostream>

#pragma comment(lib, "shlwapi.lib")

// Forward declarations
bool RegisterExplorerHook(const std::wstring& dllPath);
bool UnregisterExplorerHook();
void RestartExplorer();
void LogMessage(const std::wstring& message);

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        std::wcerr << L"Usage: install-helper.exe [install|uninstall|repair]" << std::endl;
        return 1;
    }

    std::wstring command = argv[1];

    // Get installation directory (where this exe is located)
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring installDir = exePath;
    installDir = installDir.substr(0, installDir.find_last_of(L"\\"));

    std::wstring hookDllPath = installDir + L"\\DexCorralHook.dll";

    if (command == L"install") {
        LogMessage(L"Installing DexCorral...");

        // Verify DexCorralHook.dll exists
        if (!PathFileExistsW(hookDllPath.c_str())) {
            LogMessage(L"ERROR: DexCorralHook.dll not found at: " + hookDllPath);
            return 1;
        }

        // Register the Explorer hook (optional during install)
        // The main app will handle injection at runtime
        // This could set up registry entries if needed

        LogMessage(L"Installation actions complete");
        return 0;
    }
    else if (command == L"uninstall") {
        LogMessage(L"Uninstalling DexCorral...");

        // Stop all DexCorral processes
        system("taskkill /F /IM DexCorral.exe 2>nul");
        system("taskkill /F /IM DexCorral.Watchdog.exe 2>nul");
        Sleep(1000);

        // Unregister hook and restart Explorer to unload DLL
        UnregisterExplorerHook();

        LogMessage(L"Uninstallation actions complete");
        return 0;
    }
    else if (command == L"repair") {
        LogMessage(L"Repairing DexCorral...");

        // Repair actions (restart Explorer, re-register hook, etc.)
        UnregisterExplorerHook();
        Sleep(1000);
        RestartExplorer();

        LogMessage(L"Repair actions complete");
        return 0;
    }
    else {
        std::wcerr << L"Unknown command: " << command << std::endl;
        return 1;
    }
}

bool RegisterExplorerHook(const std::wstring& dllPath) {
    // Placeholder for hook registration
    // In practice, DexCorral handles injection at runtime
    // This could set up any registry entries needed for the hook
    LogMessage(L"Hook registration: " + dllPath);
    return true;
}

bool UnregisterExplorerHook() {
    // Placeholder for hook cleanup
    LogMessage(L"Hook unregistration");

    // Restart Explorer to unload the DLL
    RestartExplorer();

    return true;
}

void RestartExplorer() {
    LogMessage(L"Restarting Explorer...");

    // Kill Explorer
    system("taskkill /F /IM explorer.exe");
    Sleep(3000);

    // Restart Explorer
    ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOW);

    LogMessage(L"Explorer restarted");
}

void LogMessage(const std::wstring& message) {
    // Log to both console and a log file
    std::wcout << L"[DexCorral Install Helper] " << message << std::endl;

    // Also write to a log file in %TEMP%
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring logPath = std::wstring(tempPath) + L"DexCorral_Install.log";

    FILE* logFile = _wfopen(logPath.c_str(), L"a");
    if (logFile) {
        fwprintf(logFile, L"[%s] %s\n", L"TIMESTAMP_PLACEHOLDER", message.c_str());
        fclose(logFile);
    }
}
