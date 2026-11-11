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
 * NameDiff.h - What changed between two listings of a folder
 *
 * A folder watch can lose changes: the change buffer overflows, or the watch
 * had to be opened again. The watcher then lists the folder and compares it
 * with the names it knew, and reports the difference as if the events had
 * arrived. File names compare without case, as the file system does.
 */

#pragma once
#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <vector>
#include <wchar.h>

struct NameLessNoCase
{
    bool operator()(const std::wstring &a, const std::wstring &b) const
    {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    }
};

using NameSet = std::set<std::wstring, NameLessNoCase>;

/// Names only in `after` go to `added`, names only in `before` to `removed`
inline void DiffNames(const NameSet &before, const NameSet &after,
                      std::vector<std::wstring> &added, std::vector<std::wstring> &removed)
{
    std::set_difference(after.begin(), after.end(), before.begin(), before.end(),
                        std::back_inserter(added), NameLessNoCase());
    std::set_difference(before.begin(), before.end(), after.begin(), after.end(),
                        std::back_inserter(removed), NameLessNoCase());
}
