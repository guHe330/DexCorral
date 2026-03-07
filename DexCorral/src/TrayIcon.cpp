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

#include "TrayIcon.h"
#include <shellapi.h>

TrayIcon::TrayIcon(HWND hwnd, HICON icon, const std::wstring &tooltip) : hwnd(hwnd)
{
    ZeroMemory(&nid, sizeof(NOTIFYICONDATAW));
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = icon;
    wcsncpy_s(nid.szTip, tooltip.c_str(), _TRUNCATE);

    Show();
}

TrayIcon::~TrayIcon()
{
    Hide();
}

bool TrayIcon::Show()
{
    if (m_visible) {
        Shell_NotifyIconW(NIM_MODIFY, &nid);
        return true;
    }
    if (Shell_NotifyIconW(NIM_ADD, &nid)) {
        m_visible = true;
        return true;
    }
    return false;  // Shell not ready yet; caller should retry later
}

void TrayIcon::Hide()
{
    if (m_visible) {
        Shell_NotifyIconW(NIM_DELETE, &nid);
        m_visible = false;
    }
}

void TrayIcon::UpdateTooltip(const std::wstring &tooltip)
{
    wcsncpy_s(nid.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}
