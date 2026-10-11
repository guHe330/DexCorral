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
 * Hotkey.h - Parsing of global hotkey descriptions ("Ctrl+Alt+H")
 *
 * Pure string logic with no window or shell calls, so the unit tests can
 * cover it. The result is in the form RegisterHotKey takes.
 */

#pragma once
#include <string>

/// A global hotkey as RegisterHotKey wants it
struct HotkeySpec
{
    unsigned modifiers = 0; /// MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN
    unsigned vk = 0;        /// Virtual-key code
};


/**
 * Parses a hotkey such as "Ctrl+Alt+H", "ctrl + shift + F9" or "Win+Alt+D".
 *
 * Case and spaces are ignored. A valid hotkey has at least one modifier
 * (Ctrl, Alt, Shift, Win) and exactly one key (A-Z, 0-9, F1-F24). A bare key
 * is refused: registered globally it would swallow that key in every program.
 * Returns false, leaving `out` untouched, for anything else.
 */
bool ParseHotkey(const std::string &text, HotkeySpec &out);

/**
 * The hotkey as a menu shows it: "Ctrl+Alt+H".
 *
 * Modifiers come in the fixed order Ctrl, Alt, Shift, Win, whatever order the
 * config named them in, and the key is written as on the keycap (a capital
 * letter names the key, not the character it types). Ctrl and Shift are
 * labelled differently on German keyboards ("Strg", "Umschalt"), so the caller
 * passes the names for the interface language. Empty for a spec without a key.
 */
std::wstring FormatHotkey(const HotkeySpec &spec, const std::wstring &ctrlName, const std::wstring &shiftName);
