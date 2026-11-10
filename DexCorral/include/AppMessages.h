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
 * AppMessages.h - Private window messages that cross file boundaries
 *
 * Messages posted to the App's message window from the hook, the shell
 * extension or the shell itself. They used to be repeated as bare
 * "WM_APP + n" in each file; renumbering one silently broke the other side.
 * Messages only App.cpp posts to itself (WM_APP + 102..105, 108, 109) stay
 * there. Keep the numbers unique across both places.
 */

#pragma once
#include <Windows.h>

/// Hook installed, or its hidden-icon state changed: the app re-syncs positions
constexpr UINT WM_HOOK_READY = WM_APP + 100;

/// SHChangeNotify delivery for SHCNE_UPDATEIMAGE (e.g. Recycle Bin full/empty)
constexpr UINT WM_SHELL_IMAGE_CHANGED = WM_APP + 101;

/// Desktop context menu: create a corral at (wParam, lParam) screen coordinates.
/// Posted from Explorer's UI thread; see the handler in App.cpp for why.
constexpr UINT WM_CREATE_CORRAL_AT = WM_APP + 106;
constexpr UINT WM_CREATE_VIRTUAL_CORRAL_AT = WM_APP + 107;
