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
 * DesktopFilter.h - Fullscreen invisible window that blocks mouse/drop interaction
 *                   at hidden icon positions on the desktop.
 *
 * Sits above the desktop ListView in z-order. Returns HTTRANSPARENT for most
 * positions so normal desktop interaction is unaffected. Returns HTCLIENT at
 * positions where hidden (corral-owned) icons are parked outside their corral
 * window bounds, swallowing those events so the desktop ListView never sees them.
 *
 * This is a common technique: physical z-order blocking rather than
 * ListView message interception. It covers mouse move (hover), clicks, and
 * OLE drag-and-drop (which uses WindowFromPoint via WM_NCHITTEST internally).
 *
 * Monitor handling: the window spans the entire virtual desktop and resizes on
 * WM_DISPLAYCHANGE, which fires for plug/unplug and resolution/orientation changes.
 */

#pragma once
#include <Windows.h>
#include <vector>

class DesktopFilter
{
public:
    /// Create and show the filter window. Call once from App::Initialize().
    bool Create();

    /// Destroy the filter window. Call from App::Shutdown().
    void Destroy();

    /// Resize to cover the new virtual desktop. Call from App::OnDisplayChange().
    void OnDisplayChange();

    /// Update which screen-coordinate rects to block.
    /// Pass only rects that are OUTSIDE all corral window bounds —
    /// the caller is responsible for filtering out corral-covered positions.
    void SetBlockedRects(std::vector<RECT> rects);

    HWND GetHWND() const { return hwnd; }

private:
    HWND hwnd = nullptr;
    std::vector<RECT> blockedRects;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static const wchar_t* CLASS_NAME;
};
