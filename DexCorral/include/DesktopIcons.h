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
 * DesktopIcons.h - Cross-process desktop icon manipulation
 *
 * Provides static methods to read and manipulate desktop icons in the Explorer
 * ListView. Uses cross-process techniques (VirtualAllocEx, ReadProcessMemory, SendMessage)
 * to access the desktop ListView without direct memory access to explorer.exe.
 */

#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>

/// Simple 2D integer point structure for icon positions
struct POINT2D
{
    int x; /// X coordinate in screen pixels
    int y; /// Y coordinate in screen pixels
};

/// Represents a special shell icon on the desktop (Recycle Bin, This PC, Network, etc.)
struct SpecialDesktopIcon
{
    std::wstring clsid;       /// CLSID string (e.g., "{645FF040-5081-101B-9F08-00AA002F954E}")
    std::wstring displayName; /// Localized display name (e.g., "Recycle Bin")
};

/**
 * Static utility class for desktop icon manipulation.
 * Provides cross-process access to Explorer's desktop ListView to read icon positions
 * and properties, and to move icons (hide/restore).
 */
class DesktopIcons
{
public:
    /// Hides a single desktop icon by moving it off-screen
    static void HideIcon(const std::wstring &fileName);

    /// Returns true if the specified screen point is over a desktop icon
    static bool IsPointOnIcon(int screenX, int screenY);

    /// Returns the number of selected icons on the desktop
    static int GetSelectedCount();

    /// Forces a desktop refresh (F5 equivalent)
    static void RefreshDesktop();

    /// Positions a single icon at the specified screen coordinates
    static void PositionIcon(const std::wstring &fileName, int x, int y);

    /// Positions multiple icons according to the provided map of name->position
    static void PositionIcons(const std::map<std::wstring, POINT2D> &iconPositions);

    /// Returns the current screen position of an icon, or nullptr if not found
    static POINT2D *GetIconPosition(const std::wstring &fileName);

    /// Returns a map of all desktop icon names to their current positions
    static std::map<std::wstring, POINT2D> GetAllIconPositions();

    /// Hides or shows all desktop icons (entire ListView)
    static void SetIconsVisible(bool visible);

    /// Returns true if desktop icons are currently visible
    static bool AreIconsVisible();

    /**
     * Sets whether shortcut arrow overlay is hidden (requires Explorer restart).
     * Returns true if setting was changed.
     */
    static bool SetShortcutArrowsHidden(bool hidden);

    /// Returns true if shortcut arrows are hidden
    static bool AreShortcutArrowsHidden();

    /// Restarts Explorer.exe to apply certain settings (like shortcut arrow changes)
    static void RestartExplorer();

    /// Returns the window handle of the desktop ListView
    static HWND GetDesktopListView();

    /// Returns a vector of all special shell icons (Recycle Bin, This PC, etc.) on desktop
    static std::vector<SpecialDesktopIcon> GetSpecialDesktopIcons();

    /// Returns the localized display name for a special icon CLSID
    static std::wstring GetSpecialIconDisplayName(const std::wstring &clsid);

private:
    /// Helper to extract text from remote ListView item using cross-process access
    static std::wstring GetItemText(HWND hListView, HANDLE hProcess, LPVOID pRemoteItem, LPVOID pRemoteText, int index);
};
