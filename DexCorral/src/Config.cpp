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

static std::wstring Utf8ToWide(const std::string &s)
{
    if (s.empty())
        return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &result[0], size);
    return result;
}

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
    std::string tmpPath = path + ".tmp";

    try
    {
        // Serialize first so a serialization failure never touches the file.
        std::string data = nlohmann::json(config).dump(4);

        std::wstring wTmp = Utf8ToWide(tmpPath);
        std::wstring wPath = Utf8ToWide(path);

        // Write to a temp file, flush, and only then atomically replace the
        // real config. This prevents a crash/kill mid-write from leaving a
        // truncated or empty config.json.
        {
            std::ofstream file(wTmp.c_str(), std::ios::binary | std::ios::trunc);
            if (!file.is_open())
            {
                return;
            }
            file << data;
            file.flush();
            if (!file.good())
            {
                return; // Don't replace good config with a bad write
            }
        } // close the stream before renaming

        if (!MoveFileExW(wTmp.c_str(), wPath.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(wTmp.c_str()); // leave the existing config intact
        }
    }
    catch (...)
    {
        DeleteFileW(Utf8ToWide(tmpPath).c_str());
    }
}
