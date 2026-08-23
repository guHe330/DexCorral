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

/**
 * DesktopIcons.cpp - Cross-process desktop icon manipulation
 *
 * Implements cross-process access to the Explorer desktop ListView to read icon positions,
 * names, and properties, and to modify icon positions (hide/restore). Uses VirtualAllocEx,
 * ReadProcessMemory, and SendMessage to access and manipulate the ListView in explorer.exe
 * without direct access to its memory space.
 */

#include "DesktopIcons.h"
#include "Constants.h"
#include "HookBridge.h"
#include <CommCtrl.h>
#include <ShlObj.h>
#include <ShObjIdl.h>
#include <Shlwapi.h>
#include <vector>
#include <algorithm>

#pragma comment(lib, "Shlwapi.lib")

HWND DesktopIcons::GetDesktopListView()
{
    HWND progman = FindWindowW(L"Progman", nullptr);
    HWND shellDll = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);

    if (shellDll == nullptr)
    {
        // Try WorkerW windows
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
                    {
            HWND shellDll = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
            if (shellDll != nullptr) {
                *(HWND*)lParam = shellDll;
                return FALSE;
            }
            return TRUE; }, (LPARAM)&shellDll);
    }

    if (shellDll == nullptr)
        return nullptr;
    return FindWindowExW(shellDll, nullptr, L"SysListView32", nullptr);
}

std::wstring DesktopIcons::GetShellDisplayName(const std::wstring &fullPath)
{
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(fullPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME) && sfi.szDisplayName[0])
    {
        return sfi.szDisplayName;
    }

    // Fallback (SHGetFileInfoW failed — typically the file no longer exists, e.g. a
    // stale/ghost entry): strip whatever extension the shell always hides regardless of
    // the "show file extensions" setting. ".lnk" (shortcuts) and ".url" (Internet
    // Shortcuts, e.g. Steam desktop launchers) are the common cases; both are 4 chars.
    size_t slash = fullPath.find_last_of(L"\\/");
    std::wstring name = (slash != std::wstring::npos) ? fullPath.substr(slash + 1) : fullPath;
    if (name.length() > 4)
    {
        const wchar_t *ext = name.c_str() + name.length() - 4;
        if (_wcsicmp(ext, L".lnk") == 0 || _wcsicmp(ext, L".url") == 0)
        {
            name = name.substr(0, name.length() - 4);
        }
    }
    return name;
}

std::wstring DesktopIcons::GetItemText(HWND hListView, HANDLE hProcess, LPVOID pRemoteItem, LPVOID pRemoteText, int index)
{
    LVITEMW lvItem = {};
    lvItem.mask = LVIF_TEXT;
    lvItem.iItem = index;
    lvItem.iSubItem = 0;
    lvItem.pszText = (LPWSTR)pRemoteText;
    lvItem.cchTextMax = MAX_PATH;

    SIZE_T written;
    WriteProcessMemory(hProcess, pRemoteItem, &lvItem, sizeof(LVITEMW), &written);
    SendMessageW(hListView, LVM_GETITEMTEXTW, index, (LPARAM)pRemoteItem);

    wchar_t buffer[MAX_PATH];
    SIZE_T bytesRead;
    ReadProcessMemory(hProcess, pRemoteText, buffer, MAX_PATH * sizeof(wchar_t), &bytesRead);
    buffer[MAX_PATH - 1] = L'\0';

    return std::wstring(buffer);
}

bool DesktopIcons::IsPointOnIcon(int screenX, int screenY)
{
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return false;

    POINT pt = {screenX, screenY};
    ScreenToClient(hListView, &pt);

    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr)
        return false;

    LPVOID pRemoteInfo = VirtualAllocEx(hProcess, nullptr, 1024, MEM_COMMIT, PAGE_READWRITE);

    LVHITTESTINFO info = {};
    info.pt = pt;

    SIZE_T written;
    WriteProcessMemory(hProcess, pRemoteInfo, &info, sizeof(LVHITTESTINFO), &written);
    int index = (int)SendMessageW(hListView, LVM_HITTEST, 0, (LPARAM)pRemoteInfo);

    VirtualFreeEx(hProcess, pRemoteInfo, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return index != -1;
}

int DesktopIcons::GetSelectedCount()
{
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return 0;
    return (int)SendMessageW(hListView, LVM_GETSELECTEDCOUNT, 0, 0);
}

void DesktopIcons::RefreshDesktop()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void DesktopIcons::PositionIcon(const std::wstring &fileName, int x, int y)
{
    AppIconMoveScope moveScope;

    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr)
        return;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, DESKTOP_ICON_BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);

    for (int i = 0; i < count; i++)
    {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);

        if (_wcsicmp(text.c_str(), fileName.c_str()) == 0)
        {
            LPARAM pos = MAKELPARAM(x & 0xFFFF, y);
            SendMessageW(hListView, LVM_SETITEMPOSITION, i, pos);
            break;
        }
    }

    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
}

void DesktopIcons::PositionIcons(const std::map<std::wstring, POINT2D> &iconPositions)
{
    if (iconPositions.empty())
        return;

    AppIconMoveScope moveScope;

    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr)
        return;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, DESKTOP_ICON_BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);

    for (int i = 0; i < count; i++)
    {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);

        // Case-insensitive lookup (display names may differ in case from filenames)
        for (const auto &[name, pos2d] : iconPositions)
        {
            if (_wcsicmp(text.c_str(), name.c_str()) == 0)
            {
                LPARAM pos = MAKELPARAM(pos2d.x & 0xFFFF, pos2d.y);
                SendMessageW(hListView, LVM_SETITEMPOSITION, i, pos);
                break;
            }
        }
    }

    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);
}

void DesktopIcons::PositionIconsByPath(const std::vector<IconPositionRequest> &requests)
{
    if (requests.empty())
        return;

    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return;

    // Hand the requests to the Explorer hook, which matches items by parsing
    // name on the UI thread (returns 1 when handled)
    static UINT positionMsg = RegisterWindowMessageW(L"DexCorral_PositionIconsByPath");
    HookBridge::SetIconPositionRequests(requests);
    if (SendMessageW(hListView, positionMsg, 0, 0) == 1)
        return;

    // Hook not active: drain the request store and fall back to legacy
    // display-name positioning
    std::vector<IconPositionRequest> drained;
    HookBridge::TakeIconPositionRequests(drained);
    std::map<std::wstring, POINT2D> byName;
    for (const auto &req : requests)
    {
        if (!req.displayName.empty())
            byName[req.displayName] = req.pt;
    }
    PositionIcons(byName);
}

std::vector<DesktopIconInfo> DesktopIcons::GetAllIconsWithIdentity()
{
    std::vector<DesktopIconInfo> result;

    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return result;

    // Ask the Explorer hook for an identity snapshot (returns 1 when handled)
    static UINT snapshotMsg = RegisterWindowMessageW(L"DexCorral_GetIconSnapshot");
    if (SendMessageW(hListView, snapshotMsg, 0, 0) == 1)
    {
        HookBridge::TakeIconSnapshot(result);
        return result;
    }

    // Hook not active: display-name-only entries (empty parsing names)
    for (const auto &[name, pos] : GetAllIconPositions())
    {
        result.push_back({name, L"", pos});
    }
    return result;
}

std::map<std::wstring, POINT2D> DesktopIcons::GetAllIconPositions()
{
    std::map<std::wstring, POINT2D> result;

    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return result;

    int count = (int)SendMessageW(hListView, LVM_GETITEMCOUNT, 0, 0);
    DWORD processId;
    GetWindowThreadProcessId(hListView, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (hProcess == nullptr)
        return result;

    LPVOID pRemoteItem = VirtualAllocEx(hProcess, nullptr, DESKTOP_ICON_BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
    LPVOID pRemoteText = (LPBYTE)pRemoteItem + sizeof(LVITEMW);
    LPVOID pRemotePoint = VirtualAllocEx(hProcess, nullptr, 8, MEM_COMMIT, PAGE_READWRITE);

    for (int i = 0; i < count; i++)
    {
        std::wstring text = GetItemText(hListView, hProcess, pRemoteItem, pRemoteText, i);
        if (text.empty())
            continue;

        SendMessageW(hListView, LVM_GETITEMPOSITION, i, (LPARAM)pRemotePoint);

        POINT pt;
        SIZE_T bytesRead;
        ReadProcessMemory(hProcess, pRemotePoint, &pt, 8, &bytesRead);

        result[text] = {pt.x, pt.y};
    }

    VirtualFreeEx(hProcess, pRemotePoint, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return result;
}

void DesktopIcons::SetIconsVisible(bool visible)
{
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return;

    ShowWindow(hListView, visible ? SW_SHOW : SW_HIDE);
}

bool DesktopIcons::AreIconsVisible()
{
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return true;

    return IsWindowVisible(hListView) != FALSE;
}

// Registry key for shell icon overlays
static const wchar_t *SHELL_ICONS_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Icons";
static const wchar_t *SHORTCUT_ARROW_VALUE = L"29";

bool DesktopIcons::SetShortcutArrowsHidden(bool hidden)
{
    HKEY hKey;
    LONG result;

    if (hidden)
    {
        // Create or open the Shell Icons key
        result = RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            SHELL_ICONS_KEY,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &hKey,
            nullptr);

        if (result != ERROR_SUCCESS)
        {
            // Try HKEY_CURRENT_USER as fallback (doesn't require admin)
            result = RegCreateKeyExW(
                HKEY_CURRENT_USER,
                SHELL_ICONS_KEY,
                0,
                nullptr,
                REG_OPTION_NON_VOLATILE,
                KEY_SET_VALUE,
                nullptr,
                &hKey,
                nullptr);
        }

        if (result != ERROR_SUCCESS)
        {
            return false;
        }

        // Set value "29" to an empty string to hide the arrow
        // Using a blank icon reference removes the overlay
        const wchar_t *blankValue = L"%systemroot%\\System32\\shell32.dll,-50";
        result = RegSetValueExW(
            hKey,
            SHORTCUT_ARROW_VALUE,
            0,
            REG_EXPAND_SZ,
            (const BYTE *)blankValue,
            (DWORD)((wcslen(blankValue) + 1) * sizeof(wchar_t)));

        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }
    else
    {
        // Delete the value to restore default arrow
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, SHELL_ICONS_KEY, 0, KEY_SET_VALUE, &hKey);
        if (result == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, SHORTCUT_ARROW_VALUE);
            RegCloseKey(hKey);
        }

        result = RegOpenKeyExW(HKEY_CURRENT_USER, SHELL_ICONS_KEY, 0, KEY_SET_VALUE, &hKey);
        if (result == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, SHORTCUT_ARROW_VALUE);
            RegCloseKey(hKey);
        }

        return true;
    }
}

bool DesktopIcons::AreShortcutArrowsHidden()
{
    HKEY hKey;
    LONG result;

    // Check HKEY_LOCAL_MACHINE first
    result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, SHELL_ICONS_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (result == ERROR_SUCCESS)
    {
        DWORD type;
        DWORD size = 0;
        result = RegQueryValueExW(hKey, SHORTCUT_ARROW_VALUE, nullptr, &type, nullptr, &size);
        RegCloseKey(hKey);
        if (result == ERROR_SUCCESS && size > 0)
        {
            return true;
        }
    }

    // Check HKEY_CURRENT_USER
    result = RegOpenKeyExW(HKEY_CURRENT_USER, SHELL_ICONS_KEY, 0, KEY_QUERY_VALUE, &hKey);
    if (result == ERROR_SUCCESS)
    {
        DWORD type;
        DWORD size = 0;
        result = RegQueryValueExW(hKey, SHORTCUT_ARROW_VALUE, nullptr, &type, nullptr, &size);
        RegCloseKey(hKey);
        if (result == ERROR_SUCCESS && size > 0)
        {
            return true;
        }
    }

    return false;
}

void DesktopIcons::RestartExplorer()
{
    // Find and terminate explorer.exe
    HWND shellWindow = GetShellWindow();
    if (shellWindow)
    {
        DWORD pid;
        GetWindowThreadProcessId(shellWindow, &pid);

        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess)
        {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }

    // Wait briefly for explorer to terminate
    Sleep(1000);

    // Restart explorer
    ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
}

std::vector<SpecialDesktopIcon> DesktopIcons::GetSpecialDesktopIcons()
{
    std::vector<SpecialDesktopIcon> result;

    // Get the desktop IShellFolder
    IShellFolder *pDesktop = nullptr;
    if (FAILED(SHGetDesktopFolder(&pDesktop)))
        return result;

    // Enumerate desktop items
    IEnumIDList *pEnum = nullptr;
    if (FAILED(pDesktop->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnum)))
    {
        pDesktop->Release();
        return result;
    }

    LPITEMIDLIST pidlChild = nullptr;
    while (pEnum->Next(1, &pidlChild, nullptr) == S_OK)
    {
        // Get the parsing name - special items return "::{GUID}"
        STRRET strret;
        if (SUCCEEDED(pDesktop->GetDisplayNameOf(pidlChild, SHGDN_FORPARSING, &strret)))
        {
            wchar_t parseName[MAX_PATH] = {};
            StrRetToBufW(&strret, pidlChild, parseName, MAX_PATH);

            // Special shell items have parsing names starting with "::"
            if (parseName[0] == L':' && parseName[1] == L':')
            {
                // Get the localized display name
                STRRET strretDisplay;
                if (SUCCEEDED(pDesktop->GetDisplayNameOf(pidlChild, SHGDN_NORMAL, &strretDisplay)))
                {
                    wchar_t displayName[MAX_PATH] = {};
                    StrRetToBufW(&strretDisplay, pidlChild, displayName, MAX_PATH);

                    // Windows exposes several namespace items under the same localized
                    // name (e.g. Control Panel appears twice: the browsable folder
                    // {26EE0668-...} and the Start menu/desktop command object
                    // {5399E694-...}, both resolving to shell32.dll,-4161). They are
                    // indistinguishable in the menu, so keep only the first occurrence.
                    bool duplicateName = std::any_of(result.begin(), result.end(),
                                                     [&](const SpecialDesktopIcon &existing)
                                                     { return existing.displayName == displayName; });
                    if (!duplicateName)
                    {
                        SpecialDesktopIcon sdi;
                        // Strip leading "::" to store just the CLSID
                        sdi.clsid = parseName + 2;
                        sdi.displayName = displayName;
                        result.push_back(std::move(sdi));
                    }
                }
            }
        }
        CoTaskMemFree(pidlChild);
    }

    pEnum->Release();
    pDesktop->Release();
    return result;
}

std::wstring DesktopIcons::GetSpecialIconDisplayName(const std::wstring &clsid)
{
    // Build the parsing name "::{CLSID}" and resolve to display name
    std::wstring parseName = L"::" + clsid;

    LPITEMIDLIST pidl = nullptr;
    if (FAILED(SHParseDisplayName(parseName.c_str(), nullptr, &pidl, 0, nullptr)) || !pidl)
    {
        return L"";
    }

    IShellFolder *pDesktop = nullptr;
    if (FAILED(SHGetDesktopFolder(&pDesktop)))
    {
        CoTaskMemFree(pidl);
        return L"";
    }

    STRRET strret;
    std::wstring displayName;
    if (SUCCEEDED(pDesktop->GetDisplayNameOf(pidl, SHGDN_NORMAL, &strret)))
    {
        wchar_t buf[MAX_PATH] = {};
        StrRetToBufW(&strret, pidl, buf, MAX_PATH);
        displayName = buf;
    }

    pDesktop->Release();
    CoTaskMemFree(pidl);
    return displayName;
}
