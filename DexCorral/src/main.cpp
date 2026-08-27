/**
 * DexCorral - a free and open source Windows desktop icon organizer
 * Copyright (C) 2026 Gunter Heiss
 *
 * For more information see: https://dexcorral.com
 * The DexCorral project is hosted on GitHub: https://github.com/guHe330/DexCorral
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <Windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <string>

// DexCorral Registration Tool
// The actual application runs as a shell extension (DexCorralHook.dll) loaded by Explorer.
// This EXE registers/unregisters the shell extension, and injects it at login via --startup.

// DexCorral targets Windows 11 only. Windows 10 is end of life and none of the
// Explorer integration here is tested on it, so registration refuses to run
// below this build unless --force is passed.
static const DWORD kMinWindowsBuild = 22000;  // Windows 11 21H2

// GetVersionEx and the VersionHelpers macros are capped by the application
// manifest and report an old version on newer Windows, so ask the kernel.
// A failed probe returns 0 and is treated as supported: an unknown version is
// never a reason to block someone who is probably on Windows 11 anyway.
static DWORD GetWindowsBuildNumber()
{
    typedef LONG(WINAPI *RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return 0;

    auto fn = (RtlGetVersionFunc)GetProcAddress(hNtdll, "RtlGetVersion");
    if (!fn) return 0;

    RTL_OSVERSIONINFOW info = { sizeof(info) };
    if (fn(&info) != 0) return 0;  // != STATUS_SUCCESS
    return info.dwBuildNumber;
}

typedef HRESULT(__stdcall *DllRegisterServerFunc)();
typedef HRESULT(__stdcall *DllUnregisterServerFunc)();

// ─── --startup: inject DexCorralHook.dll into Explorer via WH_GETMESSAGE ───────
//
// Sets a thread-specific WH_GETMESSAGE hook on Explorer's Progman (desktop) thread.
// Windows loads DexCorralHook.dll into Explorer's address space as a result.
// The WakeHookProc inside the DLL calls StartAppIfNeeded(), which starts the App
// (corrals, tray icon, hook). We then unhook and exit; the DLL stays resident in
// Explorer because DllCanUnloadNow() returns S_FALSE.
//
static int DoStartup(const wchar_t* dllPath, bool silent)
{
    // Wait up to 30 s for Explorer's Progman (desktop host) window
    HWND hProgman = nullptr;
    for (int i = 0; i < 60 && !hProgman; i++)
    {
        hProgman = FindWindowW(L"Progman", nullptr);
        if (!hProgman) Sleep(500);
    }
    if (!hProgman)
    {
        if (!silent)
            MessageBoxW(nullptr, L"Could not find Explorer's desktop window.\nMake sure Explorer is running.",
                        L"DexCorral", MB_ICONWARNING);
        return 1;
    }

    DWORD tid = GetWindowThreadProcessId(hProgman, nullptr);
    if (!tid) return 1;

    // Load the DLL in this process solely to pass its HMODULE to SetWindowsHookEx.
    // Windows uses the module path to inject the same DLL file into Explorer's process.
    HMODULE hDll = LoadLibraryW(dllPath);
    if (!hDll)
    {
        if (!silent)
            MessageBoxW(nullptr, L"Failed to load DexCorralHook.dll.", L"DexCorral", MB_ICONERROR);
        return 1;
    }

    auto hookProc = (HOOKPROC)GetProcAddress(hDll, "WakeHookProc");
    if (!hookProc)
    {
        if (!silent)
            MessageBoxW(nullptr, L"WakeHookProc not found in DexCorralHook.dll.", L"DexCorral", MB_ICONERROR);
        FreeLibrary(hDll);
        return 1;
    }

    // Install the hook — this injects the DLL into Explorer's Progman thread.
    HHOOK hHook = SetWindowsHookExW(WH_GETMESSAGE, hookProc, hDll, tid);
    if (hHook)
    {
        // Poll until DexCorralMessageWindow appears, confirming the App is running
        // inside Explorer. Keep the hook alive and nudge Progman every 500 ms so
        // WakeHookProc fires even if Explorer is busy at login time.
        // Timeout: 30 s (60 × 500 ms).
        for (int i = 0; i < 60; i++)
        {
            if (FindWindowW(L"DexCorralMessageWindow", nullptr))
                break;
            PostMessage(hProgman, WM_NULL, 0, 0);
            Sleep(500);
        }

        UnhookWindowsHookEx(hHook);
    }

    FreeLibrary(hDll);
    return hHook ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Parse command line
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return 1;

    bool doRegister   = false;
    bool doUnregister = false;
    bool doStartup    = false;
    bool silent       = false;
    bool force        = false;

    for (int i = 1; i < argc; i++)
    {
        if      (_wcsicmp(argv[i], L"--register")   == 0 || _wcsicmp(argv[i], L"-register")   == 0) doRegister   = true;
        else if (_wcsicmp(argv[i], L"--unregister") == 0 || _wcsicmp(argv[i], L"-unregister") == 0) doUnregister = true;
        else if (_wcsicmp(argv[i], L"--startup")    == 0 || _wcsicmp(argv[i], L"-startup")    == 0) doStartup    = true;
        else if (_wcsicmp(argv[i], L"--silent")     == 0 || _wcsicmp(argv[i], L"-silent")     == 0) silent       = true;
        else if (_wcsicmp(argv[i], L"--force")      == 0 || _wcsicmp(argv[i], L"-force")      == 0) force        = true;
    }
    LocalFree(argv);

    // Resolve path to DexCorralHook.dll (always next to this EXE)
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
    wchar_t *lastSlash = wcsrchr(dllPath, L'\\');
    if (lastSlash)
        wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - dllPath + 1), L"DexCorralHook.dll");

    // ── --startup: inject into Explorer and exit ──────────────────────────────
    if (doStartup)
        return DoStartup(dllPath, silent);

    // ── Windows 11 check (registration only) ──────────────────────────────────
    // --unregister and --startup are deliberately not gated: a forced install
    // must stay removable and startable.
    if (doRegister && !force)
    {
        DWORD build = GetWindowsBuildNumber();
        if (build != 0 && build < kMinWindowsBuild)
        {
            if (!silent)
            {
                wchar_t msg[512];
                StringCchPrintfW(msg, ARRAYSIZE(msg),
                                 L"DexCorral requires Windows 11 (build %u or newer).\n"
                                 L"This system reports build %u.\n\n"
                                 L"Windows 10 is end of life and DexCorral is neither tested nor "
                                 L"supported on it.\n\n"
                                 L"To register anyway, at your own risk:\n"
                                 L"  DexCorral.exe --register --force\n\n"
                                 L"Please do not file bug reports from unsupported Windows versions.",
                                 kMinWindowsBuild, build);
                MessageBoxW(nullptr, msg, L"DexCorral", MB_ICONERROR);
            }
            return 1;
        }
    }

    // ── --register / --unregister ─────────────────────────────────────────────
    if (doRegister || doUnregister)
    {
        HMODULE hDll = LoadLibraryW(dllPath);
        if (!hDll)
        {
            if (!silent)
                MessageBoxW(nullptr, L"Failed to load DexCorralHook.dll.\nMake sure it's in the same folder as this EXE.",
                            L"DexCorral", MB_ICONERROR);
            return 1;
        }

        HRESULT hr;
        if (doRegister)
        {
            auto fn = (DllRegisterServerFunc)GetProcAddress(hDll, "DllRegisterServer");
            if (!fn)
            {
                if (!silent)
                    MessageBoxW(nullptr, L"DllRegisterServer not found in DexCorralHook.dll.", L"DexCorral", MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn();
            if (SUCCEEDED(hr))
            {
                if (!silent)
                    MessageBoxW(nullptr,
                                L"DexCorral shell extension registered successfully.\n\n"
                                L"Restart Explorer for changes to take effect:\n"
                                L"  1. Open Task Manager\n"
                                L"  2. Find 'Windows Explorer'\n"
                                L"  3. Right-click > Restart",
                                L"DexCorral", MB_ICONINFORMATION);
            }
            else
            {
                if (!silent)
                    MessageBoxW(nullptr,
                                L"Failed to register shell extension.\n\nTry running as Administrator.",
                                L"DexCorral", MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
        }

        if (doUnregister)
        {
            auto fn = (DllUnregisterServerFunc)GetProcAddress(hDll, "DllUnregisterServer");
            if (!fn)
            {
                if (!silent)
                    MessageBoxW(nullptr, L"DllUnregisterServer not found in DexCorralHook.dll.", L"DexCorral", MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn();
            if (SUCCEEDED(hr))
            {
                if (!silent)
                    MessageBoxW(nullptr,
                                L"DexCorral shell extension unregistered successfully.\n\n"
                                L"Restart Explorer for changes to take effect.",
                                L"DexCorral", MB_ICONINFORMATION);
            }
            else
            {
                if (!silent)
                    MessageBoxW(nullptr, L"Failed to unregister shell extension.", L"DexCorral", MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
        }

        FreeLibrary(hDll);
        return 0;
    }

    // ── No recognised args: show usage ────────────────────────────────────────
    if (!silent)
    {
        MessageBoxW(nullptr,
                    L"DexCorral - Desktop Icon Organizer\n\n"
                    L"Usage:\n"
                    L"  DexCorral.exe --register     Register shell extension\n"
                    L"  DexCorral.exe --unregister   Unregister shell extension\n"
                    L"  DexCorral.exe --startup      Inject into Explorer and start (used by Run key)\n"
                    L"  DexCorral.exe --silent       Suppress message dialogs\n"
                    L"  DexCorral.exe --force        Register on an unsupported Windows version\n\n"
                    L"DexCorral requires Windows 11; Windows 10 is unsupported and untested.\n"
                    L"After registration, restart Explorer or use --startup to activate.",
                    L"DexCorral", MB_ICONINFORMATION);
    }
    return 0;
}
