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
 * ModernMenu.h - DexCorral's own popup menus, drawn in the look of the dialogs
 *
 * More room per entry, a rounded highlight, light or dark like the dialogs
 * (see DialogSkin.h). The menu stays an ordinary popup menu: keyboard,
 * submenus and screen readers work as always, only the entries are painted
 * here. With a high-contrast theme Windows draws the menu.
 */

#pragma once
#include <Windows.h>

namespace ModernMenu
{
/// Shows `menu` at the screen point and returns the command chosen, 0 for
/// none. Takes the place of TrackPopupMenu(menu, TPM_RETURNCMD | flags, ...).
/// `owner` must be a window of the calling thread.
int Track(HMENU menu, UINT flags, int x, int y, HWND owner);
} // namespace ModernMenu
