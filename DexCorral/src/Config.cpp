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
#include "Config.h"
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <filesystem>

std::string Config::GetConfigPath()
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        std::wstring wpath(path);
        wpath += L"\\DexCorral";

        // Create directory if it doesn't exist
        CreateDirectoryW(wpath.c_str(), NULL);

        wpath += L"\\config.json";

        // Convert to UTF-8
        int size = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, NULL, 0, NULL, NULL);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, &result[0], size, NULL, NULL);
        return result;
    }
    return "config.json";
}

AppConfig Config::Load()
{
    std::string path = GetConfigPath();
    std::ifstream file(path);

    if (!file.is_open())
    {
        return AppConfig();
    }

    try
    {
        nlohmann::json j;
        file >> j;
        return j.get<AppConfig>();
    }
    catch (...)
    {
        return AppConfig();
    }
}

void Config::Save(const AppConfig &config)
{
    std::string path = GetConfigPath();
    std::ofstream file(path);

    if (!file.is_open())
    {
        return;
    }

    try
    {
        nlohmann::json j = config;
        file << j.dump(4);
    }
    catch (...)
    {
        // Error saving config
    }
}
