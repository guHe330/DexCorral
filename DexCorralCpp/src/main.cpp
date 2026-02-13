#include <Windows.h>
#include <shellapi.h>
#include <string>

// DexCorral Registration Tool
// The actual application runs as a shell extension (DexCorralHook.dll) loaded by Explorer.
// This EXE registers/unregisters the shell extension.

typedef HRESULT(__stdcall* DllRegisterServerFunc)();
typedef HRESULT(__stdcall* DllUnregisterServerFunc)();

static const wchar_t* DEBUG_EVENT_NAME = L"Local\\DexCorralDebug";

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Parse command line
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;

    bool doRegister = false;
    bool doUnregister = false;
    bool createDebugEvent = false;

    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"--register") == 0 || _wcsicmp(argv[i], L"-register") == 0) {
            doRegister = true;
        }
        else if (_wcsicmp(argv[i], L"--unregister") == 0 || _wcsicmp(argv[i], L"-unregister") == 0) {
            doUnregister = true;
        }
        else if (_wcsicmp(argv[i], L"--debug") == 0 || _wcsicmp(argv[i], L"-debug") == 0) {
            createDebugEvent = true;
        }
    }
    LocalFree(argv);

    // Create debug event if requested (hook DLL checks for this)
    HANDLE hDebugEvent = nullptr;
    if (createDebugEvent) {
        hDebugEvent = CreateEventW(nullptr, TRUE, TRUE, DEBUG_EVENT_NAME);
    }

    // Find DexCorralHook.dll next to this EXE
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(dllPath, L'\\');
    if (lastSlash) {
        wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - dllPath + 1), L"DexCorralHook.dll");
    }

    if (doRegister || doUnregister) {
        HMODULE hDll = LoadLibraryW(dllPath);
        if (!hDll) {
            MessageBoxW(nullptr, L"Failed to load DexCorralHook.dll.\nMake sure it's in the same folder as this EXE.",
                L"DexCorral", MB_ICONERROR);
            return 1;
        }

        HRESULT hr;
        if (doRegister) {
            auto fn = (DllRegisterServerFunc)GetProcAddress(hDll, "DllRegisterServer");
            if (!fn) {
                MessageBoxW(nullptr, L"DllRegisterServer not found in DexCorralHook.dll.",
                    L"DexCorral", MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn();
            if (SUCCEEDED(hr)) {
                MessageBoxW(nullptr,
                    L"DexCorral shell extension registered successfully.\n\n"
                    L"Restart Explorer for changes to take effect:\n"
                    L"  1. Open Task Manager\n"
                    L"  2. Find 'Windows Explorer'\n"
                    L"  3. Right-click > Restart",
                    L"DexCorral", MB_ICONINFORMATION);
            } else {
                MessageBoxW(nullptr,
                    L"Failed to register shell extension.\n\n"
                    L"Try running as Administrator.",
                    L"DexCorral", MB_ICONERROR);
            }
        }

        if (doUnregister) {
            auto fn = (DllUnregisterServerFunc)GetProcAddress(hDll, "DllUnregisterServer");
            if (!fn) {
                MessageBoxW(nullptr, L"DllUnregisterServer not found in DexCorralHook.dll.",
                    L"DexCorral", MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn();
            if (SUCCEEDED(hr)) {
                MessageBoxW(nullptr,
                    L"DexCorral shell extension unregistered successfully.\n\n"
                    L"Restart Explorer for changes to take effect.",
                    L"DexCorral", MB_ICONINFORMATION);
            } else {
                MessageBoxW(nullptr, L"Failed to unregister shell extension.",
                    L"DexCorral", MB_ICONERROR);
            }
        }

        FreeLibrary(hDll);
    } else {
        MessageBoxW(nullptr,
            L"DexCorral - Desktop Icon Organizer\n\n"
            L"Usage:\n"
            L"  DexCorral.exe --register     Register shell extension\n"
            L"  DexCorral.exe --unregister   Unregister shell extension\n"
            L"  DexCorral.exe --debug        Enable debug logging\n\n"
            L"After registration, restart Explorer to activate.",
            L"DexCorral", MB_ICONINFORMATION);
    }

    if (hDebugEvent) CloseHandle(hDebugEvent);
    return 0;
}
