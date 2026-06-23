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

#pragma once

// ---------------------------------------------------------------------------
// Single source of truth for the DexCorral version.
//
// Bump these by hand in the commit that should ship as a release. CMakeLists.txt
// parses this file for the project version, the .rc files include it for their
// FILEVERSION/ProductVersion fields, and the release CI verifies that the pushed
// git tag (vX.Y.Z) matches DEXCORRAL_VERSION before building. Keep all five
// macros below consistent — CI checks them.
// ---------------------------------------------------------------------------
#define DEXCORRAL_VERSION L"1.0.19"
#define DEXCORRAL_VERSION_MAJOR 1
#define DEXCORRAL_VERSION_MINOR 0
#define DEXCORRAL_VERSION_PATCH 19

// Resource-file (.rc) forms — keep in sync with the numbers above.
#define DEXCORRAL_VERSION_COMMA DEXCORRAL_VERSION_MAJOR, DEXCORRAL_VERSION_MINOR, DEXCORRAL_VERSION_PATCH, 0
#define DEXCORRAL_VERSION_DOTTED "1.0.19.0"
