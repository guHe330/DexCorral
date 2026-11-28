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
 * ExtensionList.h - The file extensions a tab receives new desktop files by
 *
 * A tab can list extensions ("pdf; docx"): a new desktop file with one of them
 * goes to that tab instead of the Catch-All. The list is kept in config.json as
 * it is shown, in lower case without dots, separated by "; ".
 */

#pragma once
#include <Windows.h>
#include <algorithm>
#include <string>
#include <vector>

namespace ExtensionList
{
    /// Lower case, as Windows compares file names
    inline std::wstring Lower(std::wstring text)
    {
        if (!text.empty())
            CharLowerBuffW(&text[0], (DWORD)text.size());
        return text;
    }

    /// What the user typed (or config.json holds) as a list: "*.PDF, .docx txt"
    /// gives pdf, docx, txt. Separators are ; , and blanks. Entries that cannot
    /// be an extension are dropped: a dot or "*" inside, a character no file name
    /// can have, and "lnk" (shortcuts are never sent by extension). So are repeats.
    inline std::vector<std::wstring> Parse(const std::wstring &text)
    {
        std::vector<std::wstring> list;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find_first_of(L";, \t", pos);
            if (end == std::wstring::npos)
                end = text.size();
            std::wstring entry = Lower(text.substr(pos, end - pos));
            pos = end + 1;

            if (!entry.empty() && entry[0] == L'*')
                entry.erase(0, 1);
            if (!entry.empty() && entry[0] == L'.')
                entry.erase(0, 1);
            bool valid = !entry.empty() && entry != L"lnk";
            for (wchar_t c : entry)
                if (c < 32 || wcschr(L"\\/:*?\"<>|.", c))
                    valid = false;
            if (valid && std::find(list.begin(), list.end(), entry) == list.end())
                list.push_back(entry);
        }
        return list;
    }

    /// The list as it is stored and shown: "pdf; docx"
    inline std::wstring Join(const std::vector<std::wstring> &list)
    {
        std::wstring text;
        for (const auto &entry : list)
            text += (text.empty() ? L"" : L"; ") + entry;
        return text;
    }

    /// Takes every entry of `these` out of `list`
    inline void Remove(std::vector<std::wstring> &list, const std::vector<std::wstring> &these)
    {
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [&](const std::wstring &entry)
                                  { return std::find(these.begin(), these.end(), entry) != these.end(); }),
                   list.end());
    }

    /// The extension a new desktop file is sent by, in lower case without the
    /// dot. Empty when it has none, and for a shortcut (.lnk): its own extension
    /// says nothing about what it points to, and its target's is not looked at.
    /// Folders have no extension either; the caller checks the attributes.
    inline std::wstring OfFile(const std::wstring &fileName)
    {
        const size_t dot = fileName.find_last_of(L'.');
        if (dot == std::wstring::npos || fileName.find_first_of(L"\\/", dot) != std::wstring::npos)
            return L"";
        std::wstring ext = Lower(fileName.substr(dot + 1));
        return ext == L"lnk" ? L"" : ext;
    }
} // namespace ExtensionList
