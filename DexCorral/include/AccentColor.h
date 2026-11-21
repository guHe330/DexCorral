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
 * AccentColor.h - Colours of a tab that follows the Windows accent colour
 *
 * Pure logic, kept apart from the windows so it can be unit tested.
 * Colours are 0xRRGGBB unless said otherwise.
 */

#pragma once
#include <cstdio>
#include <string>

namespace AccentColor
{
/// Windows blue, used when the accent colour cannot be read
constexpr unsigned FALLBACK_RGB = 0x0078D4;

/// HKCU\Software\Microsoft\Windows\DWM\AccentColor is stored as 0xAABBGGRR
inline unsigned RgbFromRegistry(unsigned abgr)
{
    return ((abgr & 0xFF) << 16) | (abgr & 0xFF00) | ((abgr >> 16) & 0xFF);
}

/// The tab's background as "#AARRGGBB": the accent colour with the
/// transparency of the tab's own colour (60% if that colour is malformed).
inline std::string Background(const std::string &tabColorHex, unsigned accentRgb)
{
    unsigned alpha = 0x99;
    if (tabColorHex.length() >= 9 && tabColorHex[0] == '#')
    {
        unsigned argb = 0;
        if (sscanf_s(tabColorHex.c_str() + 1, "%x", &argb) == 1)
            alpha = (argb >> 24) & 0xFF;
    }
    char buf[16];
    sprintf_s(buf, "#%02X%06X", alpha, accentRgb & 0xFFFFFF);
    return buf;
}

/// Header text on the accent colour: "#FFFFFF" or "#000000", whichever reads better
inline std::string TextOn(unsigned accentRgb)
{
    const unsigned r = (accentRgb >> 16) & 0xFF, g = (accentRgb >> 8) & 0xFF, b = accentRgb & 0xFF;
    const unsigned luma = (299 * r + 587 * g + 114 * b) / 1000; // Perceived brightness, 0-255
    return luma > 150 ? "#000000" : "#FFFFFF";
}
} // namespace AccentColor
