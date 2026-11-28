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
 * DesktopIcons.cpp - Desktop icon manipulation
 *
 * Reads icon positions and names from the Explorer desktop ListView and moves
 * icons. This code runs inside explorer.exe, so the ListView messages take
 * plain local buffers.
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

std::wstring DesktopIcons::GetItemText(HWND hListView, int index)
{
    wchar_t buffer[MAX_PATH] = {};
    LVITEMW lvItem = {};
    lvItem.iSubItem = 0;
    lvItem.pszText = buffer;
    lvItem.cchTextMax = MAX_PATH;
    SendMessageW(hListView, LVM_GETITEMTEXTW, index, (LPARAM)&lvItem);
    buffer[MAX_PATH - 1] = L'\0';
    return buffer;
}

bool DesktopIcons::IsPointOnIcon(int screenX, int screenY)
{
    HWND hListView = GetDesktopListView();
    if (hListView == nullptr)
        return false;

    LVHITTESTINFO info = {};
    info.pt = {screenX, screenY};
    ScreenToClient(hListView, &info.pt);
    return (int)SendMessageW(hListView, LVM_HITTEST, 0, (LPARAM)&info) != -1;
}

std::wstring DesktopIcons::GetVisibleIconParsingNameAt(int screenX, int screenY)
{
    HWND hListView = GetDesktopListView();
    POINT pt = {screenX, screenY};
    // Not under a corral or another window, and not while the icons are hidden
    if (hListView == nullptr || WindowFromPoint(pt) != hListView)
        return L"";

    // The hook leaves corral-owned icons out of the hit test
    LVHITTESTINFO info = {};
    info.pt = pt;
    ScreenToClient(hListView, &info.pt);
    int hit = (int)SendMessageW(hListView, LVM_HITTEST, 0, (LPARAM)&info);
    if (hit < 0)
        return L"";

    // Only the hook can name an item (returns 1 when handled)
    static UINT parsingNameMsg = RegisterWindowMessageW(L"DexCorral_GetItemParsingName");
    HookBridge::TakeItemParsingName(); // Nothing left over from an earlier answer
    if (SendMessageW(hListView, parsingNameMsg, hit, 0) != 1)
        return L"";
    return HookBridge::TakeItemParsingName();
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
    for (int i = 0; i < count; i++)
    {
        std::wstring text = GetItemText(hListView, i);

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
    for (int i = 0; i < count; i++)
    {
        std::wstring text = GetItemText(hListView, i);
        if (text.empty())
            continue;

        POINT pt = {};
        SendMessageW(hListView, LVM_GETITEMPOSITION, i, (LPARAM)&pt);
        result[text] = {pt.x, pt.y};
    }

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
