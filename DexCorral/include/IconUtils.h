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
 * IconUtils.h - Pure string utilities for icon entry classification and name normalization.
 *
 * These functions are deliberately free of Win32 API calls so they can be compiled
 * into the unit test executable without dragging in window/shell dependencies.
 */

#pragma once
#include <string>

namespace IconUtils
{

    /// Returns true if the entry is a special shell icon ("shell:{CLSID}").
    bool IsSpecialIconEntry(const std::string &fileName);

    /// Extracts the CLSID from a "shell:{CLSID}" entry, returning L"{CLSID}".
    std::wstring GetSpecialIconClsid(const std::string &fileName);

    /// Strips a ".lnk" extension (case-insensitive) from a wide filename.
    /// If the name doesn't end in ".lnk" or is too short, it is returned unchanged.
    std::wstring StripLnkExtension(const std::wstring &name);

} // namespace IconUtils
