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
#include "ChromeAlpha.h"

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

void Config::Normalize(AppConfig &config)
{
    config.DefaultHeaderOpacity = ChromeAlpha::ClampHeaderOpacity(config.DefaultHeaderOpacity);
    config.DefaultBorderOpacity = ChromeAlpha::ClampBorderOpacity(config.DefaultBorderOpacity);
    config.DefaultHeaderFontOpacity = ChromeAlpha::ClampTextOpacity(config.DefaultHeaderFontOpacity);
    config.DefaultIconLabelOpacity = ChromeAlpha::ClampTextOpacity(config.DefaultIconLabelOpacity);

    for (auto &corral : config.Corrals)
    {
        corral.HeaderOpacity = ChromeAlpha::ClampHeaderOpacity(corral.HeaderOpacity);
        corral.BorderOpacity = ChromeAlpha::ClampBorderOpacity(corral.BorderOpacity);
        corral.IconLabelOpacity = ChromeAlpha::ClampTextOpacity(corral.IconLabelOpacity);

        for (auto &tab : corral.Tabs)
        {
            tab.HeaderFontOpacity = ChromeAlpha::ClampTextOpacity(tab.HeaderFontOpacity);
        }
    }
}

namespace
{
    enum class ReadResult
    {
        NotFound,
        Unreadable,
        Good
    };

    /// Reads and normalizes one config file. `out` is only written on Good.
    ReadResult ReadConfigFile(const std::wstring &path, AppConfig &out)
    {
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            return ReadResult::NotFound;

        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file.is_open())
            return ReadResult::Unreadable;

        try
        {
            nlohmann::json j;
            file >> j;
            AppConfig loaded = j.get<AppConfig>();
            Config::Normalize(loaded);
            out = std::move(loaded);
            return ReadResult::Good;
        }
        catch (...)
        {
            return ReadResult::Unreadable;
        }
    }

    /// "…\config.json" -> "…\config" + suffix
    std::wstring SiblingPath(const std::wstring &path, const wchar_t *suffix)
    {
        size_t dot = path.find_last_of(L'.');
        size_t slash = path.find_last_of(L"\\/");
        std::wstring base = (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash))
                                ? path.substr(0, dot)
                                : path;
        return base + suffix;
    }
} // namespace

std::wstring Config::GetBackupPath(const std::wstring &configPath)
{
    return SiblingPath(configPath, L".bak");
}

std::wstring Config::GetCorruptPath(const std::wstring &configPath)
{
    return SiblingPath(configPath, L".corrupt.json");
}

AppConfig Config::Load(ConfigLoadStatus *status)
{
    return LoadFrom(Utf8ToWide(GetConfigPath()), status);
}

AppConfig Config::LoadFrom(const std::wstring &path, ConfigLoadStatus *status)
{
    ConfigLoadStatus localStatus;
    if (!status)
        status = &localStatus;

    AppConfig loaded;
    switch (ReadConfigFile(path, loaded))
    {
    case ReadResult::Good:
        // Keep a copy of the last config that loaded cleanly. Taken here rather
        // than on every save so the backup is always a file that is known to parse.
        CopyFileW(path.c_str(), GetBackupPath(path).c_str(), FALSE);
        *status = ConfigLoadStatus::Ok;
        return loaded;

    case ReadResult::NotFound:
        // First run, or the user deleted the file to start over: the backup is
        // deliberately not restored here.
        *status = ConfigLoadStatus::Missing;
        return AppConfig();

    case ReadResult::Unreadable:
        break;
    }

    // The next save replaces config.json, so set the unreadable file aside first:
    // a hand edit with one stray comma should be fixable, not gone.
    CopyFileW(path.c_str(), GetCorruptPath(path).c_str(), FALSE);

    if (ReadConfigFile(GetBackupPath(path), loaded) == ReadResult::Good)
    {
        *status = ConfigLoadStatus::RestoredFromBackup;
        return loaded;
    }

    *status = ConfigLoadStatus::Reset;
    return AppConfig();
}

void Config::Save(const AppConfig &config)
{
    SaveTo(Utf8ToWide(GetConfigPath()), config);
}

void Config::SaveTo(const std::wstring &wPath, const AppConfig &config)
{
    std::wstring wTmp = wPath + L".tmp";

    try
    {
        // Serialize first so a serialization failure never touches the file.
        std::string data = nlohmann::json(config).dump(4);

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
        DeleteFileW(wTmp.c_str());
    }
}