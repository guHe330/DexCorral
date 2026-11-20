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
 * UndoMove.h - Which desktop icons an undone Corral move puts back
 *
 * Pure logic, kept apart from App so it can be unit tested.
 */

#pragma once
#include "DesktopIcons.h"
#include <string>
#include <vector>

namespace UndoMove
{
/// Same identity rule as App::CacheDesktopIconPositions: parsing names when
/// both sides have one, otherwise display names; both case-insensitive.
inline bool SameIcon(const DesktopIconInfo &a, const DesktopIconInfo &b)
{
    if (!a.parsingName.empty() && !b.parsingName.empty())
        return _wcsicmp(a.parsingName.c_str(), b.parsingName.c_str()) == 0;
    return !a.displayName.empty() && _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) == 0;
}

/// Icons that the move displaced (position differs between `before` and
/// `after`) and that still lie at their `after` position in `now`: an icon
/// the user has moved since, or that is gone, is left alone.
/// Each result carries the icon's `before` position.
inline std::vector<IconPositionRequest> IconsToRestore(const std::vector<DesktopIconInfo> &before,
                                                       const std::vector<DesktopIconInfo> &after,
                                                       const std::vector<DesktopIconInfo> &now)
{
    auto find = [](const std::vector<DesktopIconInfo> &list, const DesktopIconInfo &icon) -> const DesktopIconInfo *
    {
        for (const auto &other : list)
        {
            if (SameIcon(other, icon))
                return &other;
        }
        return nullptr;
    };
    auto samePlace = [](const DesktopIconInfo &a, const DesktopIconInfo &b)
    {
        return a.pt.x == b.pt.x && a.pt.y == b.pt.y;
    };

    std::vector<IconPositionRequest> result;
    for (const auto &was : before)
    {
        const DesktopIconInfo *pushed = find(after, was);
        if (!pushed || samePlace(*pushed, was))
            continue;
        const DesktopIconInfo *current = find(now, was);
        if (current && samePlace(*current, *pushed))
            result.push_back({was.displayName, was.parsingName, was.pt});
    }
    return result;
}
} // namespace UndoMove
