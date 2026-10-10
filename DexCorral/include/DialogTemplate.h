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
 * DialogTemplate.h - Helpers for building a DLGTEMPLATE in memory
 *
 * DexCorral's dialogs are built at run time rather than from .rc resources, so
 * their texts can come from the string catalog. These helpers write the pieces
 * of the template; the caller owns the buffer and its size.
 */

#pragma once
#include <Windows.h>
#include <cstring>

/// Predefined control classes for AddDlgItem's classLo (with classHi = 0xFFFF)
constexpr WORD DLG_CLASS_BUTTON = 0x0080;
constexpr WORD DLG_CLASS_EDIT = 0x0081;
constexpr WORD DLG_CLASS_STATIC = 0x0082;

/**
 * Writes the DLGTEMPLATE header (no menu, default class, DS_SETFONT font).
 * `style` must include DS_SETFONT. Returns the position for the first item.
 */
inline WORD *BeginDlgTemplate(WORD *p, DWORD style, WORD itemCount, short cx, short cy,
                              const wchar_t *title, WORD fontPointSize = 9, const wchar_t *fontName = L"Segoe UI")
{
    DLGTEMPLATE *dlg = (DLGTEMPLATE *)p;
    dlg->style = style;
    dlg->cdit = itemCount;
    dlg->cx = cx;
    dlg->cy = cy;
    p += sizeof(DLGTEMPLATE) / sizeof(WORD);

    *p++ = 0; // no menu
    *p++ = 0; // default dialog class

    size_t len = wcslen(title) + 1;
    memcpy(p, title, len * sizeof(wchar_t));
    p += len;

    *p++ = fontPointSize;
    len = wcslen(fontName) + 1;
    memcpy(p, fontName, len * sizeof(wchar_t));
    p += len;
    return p;
}

/// Appends one control of a predefined class. `text` may be null. Returns the next position.
inline WORD *AddDlgItem(WORD *p, DWORD style, short x, short y, short cx, short cy, WORD id,
                        WORD classHi, WORD classLo, const wchar_t *text)
{
    (void)classHi; // always 0xFFFF: "the next WORD is a predefined class ordinal"

    // Align to DWORD boundary
    if ((ULONG_PTR)p % 4)
        p++;

    DLGITEMTEMPLATE *item = (DLGITEMTEMPLATE *)p;
    item->style = style;
    item->x = x;
    item->y = y;
    item->cx = cx;
    item->cy = cy;
    item->id = id;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF;
    *p++ = classLo;
    if (text)
    {
        size_t len = wcslen(text) + 1;
        memcpy(p, text, len * sizeof(wchar_t));
        p += len;
    }
    else
    {
        *p++ = 0;
    }
    *p++ = 0; // no creation data
    return p;
}
