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

#pragma once
#include <Windows.h>
#include <string>

class TrayIcon
{
public:
    static constexpr UINT WM_TRAYICON = WM_USER + 1;

    TrayIcon(HWND hwnd, HICON icon, const std::wstring &tooltip);
    ~TrayIcon();

    bool Show();   // Returns true if the icon was successfully added/updated.
    void Hide();
    void UpdateTooltip(const std::wstring &tooltip);
    bool IsVisible() const { return m_visible; }

private:
    HWND hwnd;
    NOTIFYICONDATAW nid;
    bool m_visible = false;
};
