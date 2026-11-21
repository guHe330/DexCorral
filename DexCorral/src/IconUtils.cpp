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
#include "IconUtils.h"
#include <string.h> // _wcsicmp
#include <Windows.h>

namespace IconUtils
{

    bool IsSpecialIconEntry(const std::string &fileName)
    {
        return fileName.size() > 6 && fileName.substr(0, 6) == "shell:";
    }

    std::wstring GetSpecialIconClsid(const std::string &fileName)
    {
        // "shell:{CLSID}" -> L"{CLSID}"
        std::string clsid = fileName.substr(6);
        return std::wstring(clsid.begin(), clsid.end());
    }

    std::wstring StripLnkExtension(const std::wstring &name)
    {
        if (name.length() > 4)
        {
            std::wstring ext = name.substr(name.length() - 4);
            if (_wcsicmp(ext.c_str(), L".lnk") == 0)
            {
                return name.substr(0, name.length() - 4);
            }
        }
        return name;
    }

    bool IsValidNewFileName(const std::wstring &name)
    {
        if (name.empty() || name == L"." || name == L"..")
            return false;
        for (wchar_t c : name)
        {
            if (c < 32 || wcschr(L"\\/:*?\"<>|", c))
                return false;
        }
        return true;
    }

    std::wstring NameWithHiddenExtension(const std::wstring &oldFileName, const std::wstring &shownName,
                                         const std::wstring &newName)
    {
        if (oldFileName.length() <= shownName.length() ||
            _wcsnicmp(oldFileName.c_str(), shownName.c_str(), shownName.length()) != 0 ||
            oldFileName[shownName.length()] != L'.')
        {
            return newName; // Nothing was hidden
        }
        const std::wstring hiddenExt = oldFileName.substr(shownName.length());
        const bool typed = newName.length() >= hiddenExt.length() &&
                           _wcsicmp(newName.c_str() + newName.length() - hiddenExt.length(), hiddenExt.c_str()) == 0;
        return typed ? newName : newName + hiddenExt;
    }

    bool ContainsPath(const std::vector<std::wstring> &paths, const std::wstring &path)
    {
        for (const auto &p : paths)
        {
            if (_wcsicmp(p.c_str(), path.c_str()) == 0)
                return true;
        }
        return false;
    }

    std::wstring Utf8ToWide(const std::string &utf8)
    {
        if (utf8.empty())
            return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
        return result;
    }

    std::string WideToUtf8(const std::wstring &wide)
    {
        if (wide.empty())
            return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
        return result;
    }

} // namespace IconUtils
