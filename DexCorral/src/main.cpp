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
#include <shlobj.h>
#include <strsafe.h>
#include <string>
#include "Strings.h"

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

typedef HRESULT(__stdcall *ScopedRegFunc)(int);
typedef HRESULT(__stdcall *CleanupLegacyFunc)();

// Mirrors InstallScope in ShellExtension/include/Registration.h. Kept as a
// plain int across the DLL boundary so the exe needs no shell-extension header.
static const int kScopeUser    = 0;
static const int kScopeMachine = 1;

// True if this process runs elevated. A machine-scope register writes HKLM and
// would fail late without it; checking up front gives a usable error instead.
static bool IsProcessElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;

    TOKEN_ELEVATION elevation = {};
    DWORD size = sizeof(elevation);
    const bool ok = GetTokenInformation(token, TokenElevation, &elevation, size, &size) != 0;
    CloseHandle(token);
    return ok && elevation.TokenIsElevated != 0;
}

// Machine scope when the exe sits under %ProgramFiles%, user scope otherwise.
// Only used when no --scope was passed; both installers pass one explicitly.
static int InferScopeFromExePath(const wchar_t *exePath)
{
    PWSTR dir = nullptr;
    const KNOWNFOLDERID *folders[] = { &FOLDERID_ProgramFiles, &FOLDERID_ProgramFilesX86 };
    for (const KNOWNFOLDERID *folder : folders)
    {
        if (FAILED(SHGetKnownFolderPath(*folder, 0, nullptr, &dir)) || !dir)
            continue;
        const size_t len = wcslen(dir);
        const bool under = len > 0 && _wcsnicmp(exePath, dir, len) == 0 &&
                           (exePath[len] == L'\\' || exePath[len] == 0);
        CoTaskMemFree(dir);
        dir = nullptr;
        if (under) return kScopeMachine;
    }
    return kScopeUser;
}

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

    bool doRegister    = false;
    bool doUnregister  = false;
    bool doStartup     = false;
    bool doCleanupOld  = false;
    bool silent        = false;
    bool force         = false;
    int  scope         = -1;   // -1 = not given, infer from the exe's location

    for (int i = 1; i < argc; i++)
    {
        if      (_wcsicmp(argv[i], L"--register")   == 0 || _wcsicmp(argv[i], L"-register")   == 0) doRegister   = true;
        else if (_wcsicmp(argv[i], L"--unregister") == 0 || _wcsicmp(argv[i], L"-unregister") == 0) doUnregister = true;
        else if (_wcsicmp(argv[i], L"--startup")    == 0 || _wcsicmp(argv[i], L"-startup")    == 0) doStartup    = true;
        else if (_wcsicmp(argv[i], L"--silent")     == 0 || _wcsicmp(argv[i], L"-silent")     == 0) silent       = true;
        else if (_wcsicmp(argv[i], L"--force")      == 0 || _wcsicmp(argv[i], L"-force")      == 0) force        = true;
        else if (_wcsicmp(argv[i], L"--cleanup-legacy") == 0)                                       doCleanupOld = true;
        else if (_wcsicmp(argv[i], L"--scope=user")     == 0)                                       scope        = kScopeUser;
        else if (_wcsicmp(argv[i], L"--scope=machine")  == 0)                                       scope        = kScopeMachine;
    }
    LocalFree(argv);

    // Resolve path to DexCorralHook.dll (always next to this EXE)
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t dllPath[MAX_PATH];
    wcscpy_s(dllPath, MAX_PATH, exePath);
    wchar_t *lastSlash = wcsrchr(dllPath, L'\\');
    if (lastSlash)
        wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - dllPath + 1), L"DexCorralHook.dll");

    if (scope < 0)
        scope = InferScopeFromExePath(exePath);

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
                const std::wstring msg = TrFmt(Str::Reg_NeedsWin11,
                                               std::to_wstring(kMinWindowsBuild),
                                               std::to_wstring(build));
                MessageBoxW(nullptr, msg.c_str(), Tr(Str::App_Name), MB_ICONERROR);
            }
            return 1;
        }
    }

    // ── HKLM work needs elevation; say so before touching anything ────────────
    // --cleanup-legacy always qualifies: the keys it removes are all in HKLM/HKCR,
    // so unelevated it would report success having done nothing.
    if ((doCleanupOld || ((doRegister || doUnregister) && scope == kScopeMachine)) &&
        !IsProcessElevated())
    {
        if (!silent)
            MessageBoxW(nullptr, Tr(Str::Reg_NeedsElevation), Tr(Str::App_Name), MB_ICONERROR);
        return 1;
    }

    // ── --register / --unregister / --cleanup-legacy ──────────────────────────
    if (doRegister || doUnregister || doCleanupOld)
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

        // Removes the pre-1.0.28 scopeless layout. Runs before a register so an
        // upgrade cannot leave the old HKCR CLSID shadowing the new one.
        if (doCleanupOld)
        {
            auto fn = (CleanupLegacyFunc)GetProcAddress(hDll, "DexCorralCleanupLegacy");
            if (fn) fn();   // best effort: old keys may already be gone, or unreachable
        }

        if (doRegister)
        {
            auto fn = (ScopedRegFunc)GetProcAddress(hDll, "DexCorralRegister");
            if (!fn)
            {
                if (!silent)
                    MessageBoxW(nullptr, Tr(Str::Reg_RegisterProcMissing), Tr(Str::App_Name), MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn(scope);
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
            auto fn = (ScopedRegFunc)GetProcAddress(hDll, "DexCorralUnregister");
            if (!fn)
            {
                if (!silent)
                    MessageBoxW(nullptr, Tr(Str::Reg_UnregisterProcMissing), Tr(Str::App_Name), MB_ICONERROR);
                FreeLibrary(hDll);
                return 1;
            }
            hr = fn(scope);
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
