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
#include "Conflicts.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <algorithm>
#include <cwctype>
#include <vector>

#pragma comment(lib, "version.lib")

namespace
{
    const wchar_t *const kProductName = L"Stardock Fences";

    // The Fences module Explorer loads, one name per architecture
    const wchar_t *const kModuleNames[] = {L"DesktopDock64.dll", L"DesktopDockARM64.dll", L"DesktopDock.dll"};

    const wchar_t *const kUninstallKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

    std::wstring Lower(std::wstring s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
        return s;
    }

    bool ContainsWord(const std::wstring &text, const std::wstring &word)
    {
        return Lower(text).find(Lower(word)) != std::wstring::npos;
    }

    /// One string from a file's version resource (first translation), or empty.
    std::wstring VersionString(const std::vector<BYTE> &block, const wchar_t *name)
    {
        struct LangCodePage
        {
            WORD language;
            WORD codePage;
        };
        LangCodePage *translations = nullptr;
        UINT size = 0;
        if (!VerQueryValueW(block.data(), L"\\VarFileInfo\\Translation", (LPVOID *)&translations, &size) ||
            size < sizeof(LangCodePage))
            return L"";

        wchar_t query[96];
        swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\%s", translations[0].language, translations[0].codePage, name);
        wchar_t *value = nullptr;
        UINT length = 0;
        if (!VerQueryValueW(block.data(), query, (LPVOID *)&value, &length) || !value || length == 0)
            return L"";
        return value;
    }

    /// True if the file's version resource says Stardock Fences; `version` gets "6.5.4.1".
    bool FileIsFences(const std::wstring &path, std::wstring &version)
    {
        DWORD handle = 0;
        DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
        if (size == 0)
            return false;

        std::vector<BYTE> block(size);
        if (!GetFileVersionInfoW(path.c_str(), 0, size, block.data()))
            return false;

        if (!IsFencesVersionInfo(VersionString(block, L"CompanyName"), VersionString(block, L"FileDescription")))
            return false;

        VS_FIXEDFILEINFO *fixed = nullptr;
        UINT fixedSize = 0;
        if (VerQueryValueW(block.data(), L"\\", (LPVOID *)&fixed, &fixedSize) && fixed && fixedSize >= sizeof(*fixed))
        {
            wchar_t buf[64];
            swprintf_s(buf, L"%u.%u.%u.%u", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
                       HIWORD(fixed->dwFileVersionLS), LOWORD(fixed->dwFileVersionLS));
            version = buf;
        }
        return true;
    }

    std::wstring ReadRegString(HKEY key, const wchar_t *name)
    {
        wchar_t buf[512] = {};
        DWORD size = sizeof(buf) - sizeof(wchar_t);
        DWORD type = 0;
        if (RegQueryValueExW(key, name, nullptr, &type, (BYTE *)buf, &size) != ERROR_SUCCESS ||
            (type != REG_SZ && type != REG_EXPAND_SZ))
            return L"";
        return buf;
    }

    /// Looks through one "installed programs" list. `version` gets DisplayVersion.
    bool InstallListHasFences(HKEY root, REGSAM view, std::wstring &version)
    {
        HKEY list = nullptr;
        if (RegOpenKeyExW(root, kUninstallKey, 0, KEY_READ | view, &list) != ERROR_SUCCESS)
            return false;

        bool found = false;
        wchar_t name[256];
        for (DWORD index = 0; !found; index++)
        {
            DWORD nameLength = _countof(name);
            LONG result = RegEnumKeyExW(list, index, name, &nameLength, nullptr, nullptr, nullptr, nullptr);
            if (result == ERROR_NO_MORE_ITEMS)
                break;
            if (result != ERROR_SUCCESS)
                continue;

            HKEY entry = nullptr;
            if (RegOpenKeyExW(list, name, 0, KEY_READ | view, &entry) != ERROR_SUCCESS)
                continue;

            if (IsFencesInstallEntry(ReadRegString(entry, L"DisplayName"), ReadRegString(entry, L"Publisher")))
            {
                found = true;
                version = ReadRegString(entry, L"DisplayVersion");
            }
            RegCloseKey(entry);
        }
        RegCloseKey(list);
        return found;
    }

    bool IsFencesInstalled(std::wstring &version)
    {
        // Machine-wide in either registry view, or per-user
        return InstallListHasFences(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, version) ||
               InstallListHasFences(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, version) ||
               InstallListHasFences(HKEY_CURRENT_USER, 0, version);
    }

    bool IsFencesLoadedHere(std::wstring &version)
    {
        for (const wchar_t *moduleName : kModuleNames)
        {
            HMODULE module = GetModuleHandleW(moduleName);
            if (!module)
                continue;
            wchar_t path[MAX_PATH] = {};
            if (GetModuleFileNameW(module, path, MAX_PATH) && FileIsFences(path, version))
                return true;
        }
        return false;
    }

    bool IsFencesLoadedInShell(std::wstring &version)
    {
        HWND shell = GetShellWindow();
        if (!shell)
            return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(shell, &pid);
        if (pid == 0)
            return false;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE)
            return false;

        bool found = false;
        MODULEENTRY32W entry = {};
        entry.dwSize = sizeof(entry);
        if (Module32FirstW(snapshot, &entry))
        {
            do
            {
                if (IsFencesModuleName(entry.szModule) && FileIsFences(entry.szExePath, version))
                    found = true;
            } while (!found && Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return found;
    }

    ConflictInfo Combine(bool loaded, const std::wstring &loadedVersion, bool installed, const std::wstring &installedVersion)
    {
        ConflictInfo info;
        info.loaded = loaded;
        info.installed = installed;
        info.found = loaded || installed;
        if (info.found)
        {
            // The loaded module is what is actually running, so its version wins
            const std::wstring &version = !loadedVersion.empty() ? loadedVersion : installedVersion;
            info.product = kProductName;
            if (!version.empty())
                info.product += L" " + version;
        }
        return info;
    }
} // namespace

bool IsFencesModuleName(const std::wstring &fileName)
{
    for (const wchar_t *moduleName : kModuleNames)
    {
        if (_wcsicmp(fileName.c_str(), moduleName) == 0)
            return true;
    }
    return false;
}

bool IsFencesVersionInfo(const std::wstring &companyName, const std::wstring &fileDescription)
{
    return ContainsWord(companyName, L"Stardock") && ContainsWord(fileDescription, L"Fences");
}

bool IsFencesInstallEntry(const std::wstring &displayName, const std::wstring &publisher)
{
    if (!ContainsWord(displayName, L"Fences"))
        return false;
    return ContainsWord(displayName, L"Stardock") || ContainsWord(publisher, L"Stardock");
}

ConflictInfo DetectConflictInThisProcess()
{
    std::wstring loadedVersion, installedVersion;
    bool loaded = IsFencesLoadedHere(loadedVersion);
    bool installed = IsFencesInstalled(installedVersion);
    return Combine(loaded, loadedVersion, installed, installedVersion);
}

ConflictInfo DetectConflictInShell()
{
    std::wstring loadedVersion, installedVersion;
    bool loaded = IsFencesLoadedInShell(loadedVersion);
    bool installed = IsFencesInstalled(installedVersion);
    return Combine(loaded, loadedVersion, installed, installedVersion);
}

bool IsConflictLoadedInThisProcess()
{
    std::wstring version;
    return IsFencesLoadedHere(version);
}
