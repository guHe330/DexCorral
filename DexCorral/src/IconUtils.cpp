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

} // namespace IconUtils
