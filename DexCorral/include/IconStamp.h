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
 * IconStamp.h - What a loaded icon of a virtual Corral was loaded from
 *
 * When a folder changes, usually one file did. A file whose stamp is the same
 * as at the last load keeps its icon handles and details instead of being
 * asked from the shell again. The stamp holds what the folder listing gives
 * for free, plus how the icon was loaded (size, with or without details).
 */

#pragma once
#include <Windows.h>

struct IconStamp
{
    FILETIME writeTime = {};
    ULONGLONG size = 0;
    DWORD attributes = 0; // includes the cloud states (pinned, recall on access)
    int iconSize = 0;     // 0: never loaded, equal to nothing
    bool details = false;

    bool operator==(const IconStamp &o) const
    {
        return iconSize != 0 && iconSize == o.iconSize && details == o.details && size == o.size &&
               attributes == o.attributes && CompareFileTime(&writeTime, &o.writeTime) == 0;
    }
};
