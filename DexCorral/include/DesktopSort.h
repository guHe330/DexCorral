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
 * DesktopSort.h - The hook's desktop sort modes and how Explorer's sort
 * column maps to them
 *
 * The hook recognises a desktop "Sort by" by its effect: Explorer's primary
 * sort column before and after the menu command. This file holds the pure
 * part of that, and how the last sort mode is stored in the registry.
 * PKEY_* must be defined in one translation unit (<initguid.h> before this).
 */

#pragma once
#include <Windows.h>
#include <ShlObj.h>
#include <propkey.h>

// Desktop sort modes — the hook owns desktop sorting entirely.
// Explorer's LVS_AUTOARRANGE is permanently off; sort commands are executed
// by the hook so hidden icons never participate in layout.
// The values are stored in the registry (SortMode): append only.
enum class DesktopSortMode
{
    PositionOrder, // Keep current position order, just remove gaps (compaction)
    Name,
    Size,
    Type,
    Date
};

// The mode for one of the four columns the desktop's Sort by offers;
// PositionOrder for any other column
inline DesktopSortMode SortModeForKey(const PROPERTYKEY& key)
{
    if (IsEqualPropertyKey(key, PKEY_ItemNameDisplay)) return DesktopSortMode::Name;
    if (IsEqualPropertyKey(key, PKEY_Size)) return DesktopSortMode::Size;
    if (IsEqualPropertyKey(key, PKEY_ItemTypeText)) return DesktopSortMode::Type;
    if (IsEqualPropertyKey(key, PKEY_DateModified)) return DesktopSortMode::Date;
    return DesktopSortMode::PositionOrder;
}

// Whether a menu command changed Explorer's primary sort (column or direction)
inline bool SortColumnChanged(const SORTCOLUMN& before, const SORTCOLUMN& after)
{
    return !IsEqualPropertyKey(before.propkey, after.propkey) || before.direction != after.direction;
}

// A stored SortMode value back to a mode; anything unknown is PositionOrder
inline DesktopSortMode SortModeFromStored(DWORD value)
{
    return value <= (DWORD)DesktopSortMode::Date ? (DesktopSortMode)value : DesktopSortMode::PositionOrder;
}
