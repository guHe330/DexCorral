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
#include "Hotkey.h"
#include <Windows.h>
#include <cctype>
#include <vector>

bool ParseHotkey(const std::string &text, HotkeySpec &out)
{
    // Split on '+', dropping spaces and lower-casing as we go
    std::vector<std::string> parts(1);
    for (char c : text)
    {
        if (c == '+')
            parts.emplace_back();
        else if (!isspace((unsigned char)c))
            parts.back() += (char)tolower((unsigned char)c);
    }

    HotkeySpec spec;
    for (const std::string &part : parts)
    {
        if (part.empty())
            return false; // "Ctrl++H", a trailing '+', or an empty string

        unsigned modifier = 0;
        if (part == "ctrl" || part == "control" || part == "strg")
            modifier = MOD_CONTROL;
        else if (part == "alt")
            modifier = MOD_ALT;
        else if (part == "shift")
            modifier = MOD_SHIFT;
        else if (part == "win" || part == "windows")
            modifier = MOD_WIN;

        if (modifier != 0)
        {
            spec.modifiers |= modifier;
            continue;
        }

        unsigned vk = 0;
        if (part.size() == 1 && part[0] >= 'a' && part[0] <= 'z')
        {
            vk = (unsigned)('A' + (part[0] - 'a'));
        }
        else if (part.size() == 1 && part[0] >= '0' && part[0] <= '9')
        {
            vk = (unsigned)part[0];
        }
        else if (part.size() >= 2 && part.size() <= 3 && part[0] == 'f')
        {
            int n = 0;
            for (size_t i = 1; i < part.size(); i++)
            {
                if (part[i] < '0' || part[i] > '9')
                    return false;
                n = n * 10 + (part[i] - '0');
            }
            if (n < 1 || n > 24)
                return false;
            vk = (unsigned)(VK_F1 + n - 1);
        }
        else
        {
            return false; // Not a key this parser knows
        }

        if (spec.vk != 0)
            return false; // Two keys
        spec.vk = vk;
    }

    if (spec.vk == 0 || spec.modifiers == 0)
        return false;

    out = spec;
    return true;
}

std::wstring FormatHotkey(const HotkeySpec &spec, const std::wstring &ctrlName, const std::wstring &shiftName)
{
    std::wstring key;
    if ((spec.vk >= 'A' && spec.vk <= 'Z') || (spec.vk >= '0' && spec.vk <= '9'))
        key = std::wstring(1, (wchar_t)spec.vk);
    else if (spec.vk >= VK_F1 && spec.vk <= VK_F24)
        key = L"F" + std::to_wstring(spec.vk - VK_F1 + 1);
    else
        return L"";

    std::wstring text;
    if (spec.modifiers & MOD_CONTROL)
        text += ctrlName + L"+";
    if (spec.modifiers & MOD_ALT)
        text += L"Alt+";
    if (spec.modifiers & MOD_SHIFT)
        text += shiftName + L"+";
    if (spec.modifiers & MOD_WIN)
        text += L"Win+";
    return text + key;
}
