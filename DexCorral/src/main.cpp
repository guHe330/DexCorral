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
#include <string>
#include "Strings.h"

// DexCorral Registration Tool
// The actual application runs as a shell extension (DexCorralHook.dll) loaded by Explorer.
// This EXE registers/unregisters the shell extension, and injects it at login via --startup.

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
            MessageBoxW(nullptr, Tr(Str::Reg_NoDesktopWindow),
                        Tr(Str::App_Name), MB_ICONWARNING);
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
            MessageBoxW(nullptr, Tr(Str::Reg_HookLoadFailed), Tr(Str::App_Name), MB_ICONERROR);
        return 1;
    }

    auto hookProc = (HOOKPROC)GetProcAddress(hDll, "WakeHookProc");
    if (!hookProc)
    {
        if (!silent)
            MessageBoxW(nullptr, Tr(Str::Reg_WakeProcMissing), Tr(Str::App_Name), MB_ICONERROR);
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
    // Use the language chosen during installation for this tool's dialogs
    // (the app itself prefers config.json's "Language"; see App::Initialize).
    SetLanguage(GetInstallerLanguage());

    // Parse command line
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return 1;

    bool doRegister   = false;
    bool doUnregister = false;
    bool doStartup    = false;
    bool silent       = false;

    for (int i = 1; i < argc; i++)
    {
        if      (_wcsicmp(argv[i], L"--register")   == 0 || _wcsicmp(argv[i], L"-register")   == 0) doRegister   = true;
        else if (_wcsicmp(argv[i], L"--unregister") == 0 || _wcsicmp(argv[i], L"-unregister") == 0) doUnregister = true;
        else if (_wcsicmp(argv[i], L"--startup")    == 0 || _wcsicmp(argv[i], L"-startup")    == 0) doStartup    = true;
        else if (_wcsicmp(argv[i], L"--silent")     == 0 || _wcsicmp(argv[i], L"-silent")     == 0) silent       = true;
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

    // ── --register / --unregister ─────────────────────────────────────────────
    if (doRegister || doUnregister)
    {
        HMODULE hDll = LoadLibraryW(dllPath);
        if (!hDll)
        {
            if (!silent)
                MessageBoxW(nullptr, Tr(Str::Reg_HookLoadFailedHint),
                            Tr(Str::App_Name), MB_ICONERROR);
            return 1;
        }

        HRESULT hr;
        if (doRegister)
        {
            auto fn = (DllRegisterServerFunc)GetProcAddress(hDll, "DllRegisterServer");
            if (!fn)
            {
                if (!silent)
                    MessageBoxW(nullptr, Tr(Str::Reg_RegisterProcMissing), Tr(Str::App_Name), MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn();
            if (SUCCEEDED(hr))
            {
                if (!silent)
                    MessageBoxW(nullptr,
                                Tr(Str::Reg_RegisterSuccess),
                                Tr(Str::App_Name), MB_ICONINFORMATION);
            }
            else
            {
                if (!silent)
                    MessageBoxW(nullptr,
                                Tr(Str::Reg_RegisterFailed),
                                Tr(Str::App_Name), MB_ICONERROR);
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
                    MessageBoxW(nullptr, Tr(Str::Reg_UnregisterProcMissing), Tr(Str::App_Name), MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn();
            if (SUCCEEDED(hr))
            {
                if (!silent)
                    MessageBoxW(nullptr,
                                Tr(Str::Reg_UnregisterSuccess),
                                Tr(Str::App_Name), MB_ICONINFORMATION);
            }
            else
            {
                if (!silent)
                    MessageBoxW(nullptr, Tr(Str::Reg_UnregisterFailed), Tr(Str::App_Name), MB_ICONERROR);
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
                    Tr(Str::Reg_Usage),
                    Tr(Str::App_Name), MB_ICONINFORMATION);
    }
    return 0;
}
