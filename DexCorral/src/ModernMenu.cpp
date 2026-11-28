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
 * ModernMenu.cpp - DexCorral's own popup menus (see ModernMenu.h)
 */

#include "ModernMenu.h"
#include "DialogSkin.h"
#include <CommCtrl.h>
#include <deque>
#include <algorithm>
#include <string>
#include <vector>

namespace ModernMenu
{
namespace
{
const UINT_PTR SUBCLASS_ID = 0x444D;

struct Entry
{
    std::wstring text;
    std::wstring shortcut; // What followed a tab in the menu text; drawn at the right edge
    bool separator = false;
    bool submenu = false;
};

/// What one Track call needs while its menu is open
struct Session
{
    std::deque<Entry> entries; // A deque: the menu items point at its elements
    DialogSkin::Palette colors;
    COLORREF back = 0, hover = 0;
    HFONT font = nullptr, glyphs = nullptr;
    UINT dpi = 96;
    mutable std::vector<HWND> framed; // Popup windows whose frame is painted over already

    int Px(int at96) const { return MulDiv(at96, (int)dpi, 96); }
    bool Owns(const Entry *e) const
    {
        for (const auto &mine : entries)
            if (&mine == e)
                return true;
        return false;
    }
};

/// Makes every entry of `menu` and its submenus owner-drawn
void Convert(HMENU menu, Session &session)
{
    const int count = GetMenuItemCount(menu);
    for (int i = 0; i < count; i++)
    {
        wchar_t text[256] = {};
        MENUITEMINFOW info = {sizeof(info)};
        info.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU;
        info.dwTypeData = text;
        info.cch = 256;
        if (!GetMenuItemInfoW(menu, i, TRUE, &info))
            continue;

        session.entries.emplace_back();
        Entry &entry = session.entries.back();
        entry.text = text;
        const size_t tab = entry.text.find(L'\t');
        if (tab != std::wstring::npos)
        {
            entry.shortcut = entry.text.substr(tab + 1);
            entry.text.resize(tab);
        }
        entry.separator = (info.fType & MFT_SEPARATOR) != 0;
        entry.submenu = info.hSubMenu != nullptr;

        MENUITEMINFOW set = {sizeof(set)};
        set.fMask = MIIM_FTYPE | MIIM_DATA;
        set.fType = info.fType | MFT_OWNERDRAW;
        set.dwItemData = (ULONG_PTR)&entry;
        SetMenuItemInfoW(menu, i, TRUE, &set);
        if (info.hSubMenu)
            Convert(info.hSubMenu, session);
    }
}

void Measure(MEASUREITEMSTRUCT *item, const Session &s, HWND owner)
{
    const Entry *entry = (const Entry *)item->itemData;
    if (entry->separator)
    {
        item->itemWidth = 0;
        item->itemHeight = s.Px(9);
        return;
    }
    HDC dc = GetDC(owner);
    HGDIOBJ old = SelectObject(dc, s.font);
    SIZE size = {}, keys = {};
    GetTextExtentPoint32W(dc, entry->text.c_str(), (int)entry->text.size(), &size);
    if (!entry->shortcut.empty())
    {
        GetTextExtentPoint32W(dc, entry->shortcut.c_str(), (int)entry->shortcut.size(), &keys);
        keys.cx += s.Px(24); // Gap between the label and its shortcut
    }
    SelectObject(dc, old);
    ReleaseDC(owner, dc);
    // Check column, text, shortcut, room for a submenu arrow
    item->itemWidth = s.Px(36) + size.cx + keys.cx + s.Px(40);
    item->itemHeight = s.Px(30);
}

/// Windows frames a popup in its own menu colour, which need not be ours:
/// paint over that frame, once per popup window
void PaintFrame(HDC itemDc, const Session &s)
{
    HWND popup = WindowFromDC(itemDc);
    if (!popup || std::find(s.framed.begin(), s.framed.end(), popup) != s.framed.end())
        return;
    s.framed.push_back(popup);

    RECT window, client;
    GetWindowRect(popup, &window);
    GetClientRect(popup, &client);
    MapWindowPoints(popup, nullptr, (POINT *)&client, 2);
    OffsetRect(&client, -window.left, -window.top);
    OffsetRect(&window, -window.left, -window.top);
    if (HDC frame = GetWindowDC(popup))
    {
        ExcludeClipRect(frame, client.left, client.top, client.right, client.bottom);
        HBRUSH brush = CreateSolidBrush(s.back);
        FillRect(frame, &window, brush);
        DeleteObject(brush);
        ReleaseDC(popup, frame);
    }
}

void Draw(const DRAWITEMSTRUCT *item, const Session &s)
{
    const Entry *entry = (const Entry *)item->itemData;
    HDC dc = item->hDC;
    const RECT r = item->rcItem;
    PaintFrame(dc, s);

    HBRUSH back = CreateSolidBrush(s.back);
    FillRect(dc, &r, back);
    DeleteObject(back);

    if (entry->separator)
    {
        RECT line = {r.left + s.Px(10), (r.top + r.bottom) / 2, r.right - s.Px(10), (r.top + r.bottom) / 2 + 1};
        HBRUSH brush = CreateSolidBrush(s.colors.buttonBorder);
        FillRect(dc, &line, brush);
        DeleteObject(brush);
        return;
    }

    const bool disabled = (item->itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;
    if ((item->itemState & ODS_SELECTED) && !disabled)
    {
        RECT highlight = {r.left + s.Px(4), r.top + s.Px(1), r.right - s.Px(4), r.bottom - s.Px(1)};
        DialogSkin::FillRoundRect(dc, highlight, s.Px(4), s.hover, s.hover, s.back);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? s.colors.textDisabled : s.colors.text);
    HGDIOBJ old = SelectObject(dc, s.font);
    RECT text = {r.left + s.Px(36), r.top, r.right - s.Px(28), r.bottom};
    DrawTextW(dc, entry->text.c_str(), -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (!entry->shortcut.empty())
    {
        SetTextColor(dc, disabled ? s.colors.textDisabled : s.colors.textMuted);
        DrawTextW(dc, entry->shortcut.c_str(), -1, &text, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(dc, disabled ? s.colors.textDisabled : s.colors.text);
    }

    // Check mark and submenu arrow from the Windows icon font
    if (s.glyphs)
    {
        SelectObject(dc, s.glyphs);
        if (item->itemState & ODS_CHECKED)
        {
            RECT check = {r.left + s.Px(6), r.top, r.left + s.Px(34), r.bottom};
            DrawTextW(dc, L"\xE73E", 1, &check, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        if (entry->submenu)
        {
            RECT arrow = {r.right - s.Px(28), r.top, r.right - s.Px(6), r.bottom};
            DrawTextW(dc, L"\xE76C", 1, &arrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
    SelectObject(dc, old);

    // Windows draws its own submenu arrow after this: keep it out
    ExcludeClipRect(dc, r.left, r.top, r.right, r.bottom);
}

LRESULT CALLBACK OwnerProc(HWND w, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR ref)
{
    const Session *s = (const Session *)ref;
    if (msg == WM_MEASUREITEM)
    {
        MEASUREITEMSTRUCT *item = (MEASUREITEMSTRUCT *)lParam;
        if (item && item->CtlType == ODT_MENU && s->Owns((const Entry *)item->itemData))
        {
            Measure(item, *s, w);
            return TRUE;
        }
    }
    else if (msg == WM_DRAWITEM)
    {
        const DRAWITEMSTRUCT *item = (const DRAWITEMSTRUCT *)lParam;
        if (item && item->CtlType == ODT_MENU && s->Owns((const Entry *)item->itemData))
        {
            Draw(item, *s);
            return TRUE;
        }
    }
    else if (msg == WM_MENUCHAR)
    {
        // Windows matches typed letters only against entries it draws itself:
        // for ours, pick the entries whose text starts with the letter, as a plain menu does
        HMENU menu = (HMENU)lParam;
        const WCHAR typed = (WCHAR)(UINT_PTR)CharUpperW((LPWSTR)(UINT_PTR)LOWORD(wParam));
        std::vector<int> matches;
        int current = -1;
        bool ours = false;
        const int count = GetMenuItemCount(menu);
        for (int i = 0; i < count; i++)
        {
            MENUITEMINFOW info = {sizeof(info)};
            info.fMask = MIIM_DATA | MIIM_STATE;
            if (!GetMenuItemInfoW(menu, i, TRUE, &info) || !s->Owns((const Entry *)info.dwItemData))
                continue;
            ours = true;
            const Entry *entry = (const Entry *)info.dwItemData;
            if (info.fState & MFS_HILITE)
                current = i;
            if (!entry->separator && !(info.fState & MFS_DISABLED) && !entry->text.empty() &&
                (WCHAR)(UINT_PTR)CharUpperW((LPWSTR)(UINT_PTR)entry->text[0]) == typed)
                matches.push_back(i);
        }
        if (ours && matches.size() == 1)
            return MAKELRESULT(matches[0], MNC_EXECUTE);
        if (ours && !matches.empty())
        {
            // Several entries share the letter: each press moves to the next one
            auto next = std::upper_bound(matches.begin(), matches.end(), current);
            return MAKELRESULT(next != matches.end() ? *next : matches[0], MNC_SELECT);
        }
    }
    return DefSubclassProc(w, msg, wParam, lParam);
}
} // namespace

int Track(HMENU menu, UINT flags, int x, int y, HWND owner)
{
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
                              (contrast.dwFlags & HCF_HIGHCONTRASTON);
    Session session;
    if (highContrast || !owner || !SetWindowSubclass(owner, OwnerProc, SUBCLASS_ID, (DWORD_PTR)&session))
        return TrackPopupMenu(menu, TPM_RETURNCMD | flags, x, y, 0, owner, nullptr);

    session.colors = DialogSkin::CurrentPalette();
    const bool dark = DialogSkin::IsDark();
    session.back = dark ? RGB(0x2C, 0x2C, 0x2C) : RGB(0xF9, 0xF9, 0xF9);
    session.hover = dark ? RGB(0x3D, 0x3D, 0x3D) : RGB(0xEA, 0xEA, 0xEA);

    // The menu opens on the monitor under the point, which need not be the owner's
    session.dpi = GetDpiForWindow(owner);
    const POINT at = {x, y};
    if (HMONITOR monitor = MonitorFromPoint(at, MONITOR_DEFAULTTONEAREST))
    {
        typedef HRESULT(WINAPI * GetDpi)(HMONITOR, int, UINT *, UINT *);
        HMODULE shcore = GetModuleHandleW(L"Shcore.dll");
        GetDpi fn = shcore ? (GetDpi)GetProcAddress(shcore, "GetDpiForMonitor") : nullptr;
        UINT dpiX = 0, dpiY = 0;
        if (fn && SUCCEEDED(fn(monitor, 0, &dpiX, &dpiY)) && dpiX)
            session.dpi = dpiX;
    }

    NONCLIENTMETRICSW metrics = {sizeof(metrics)};
    HFONT ownFont = nullptr;
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, session.dpi))
        ownFont = CreateFontIndirectW(&metrics.lfMenuFont);
    session.font = ownFont ? ownFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    session.glyphs = CreateFontW(-session.Px(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                                 CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");

    Convert(menu, session);
    HBRUSH back = CreateSolidBrush(session.back);
    MENUINFO info = {sizeof(info)};
    info.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    info.hbrBack = back;
    SetMenuInfo(menu, &info);

    const int command = TrackPopupMenu(menu, TPM_RETURNCMD | flags, x, y, 0, owner, nullptr);

    RemoveWindowSubclass(owner, OwnerProc, SUBCLASS_ID);
    // The menu must not keep the brush: the caller destroys the menu next
    info.hbrBack = nullptr;
    SetMenuInfo(menu, &info);
    DeleteObject(back);
    if (ownFont)
        DeleteObject(ownFont);
    if (session.glyphs)
        DeleteObject(session.glyphs);
    return command;
}
} // namespace ModernMenu
