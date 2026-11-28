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
#include "Presets.h"
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include "ChromeAlpha.h"
#include "LayoutMath.h"
#include "IconUtils.h"

using IconUtils::Utf8ToWide;

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

namespace
{
    /// Replaces a colour that is not "#" followed by exactly `digits` hex digits
    /// (8 for ARGB, 6 for RGB), the same rule as presets. Every reader parses
    /// these with sscanf("%x") into a local it does not initialise, so a
    /// hand-edited "#zzzzzz" would paint from stack garbage, and an ARGB reader
    /// skips a 6-digit colour and paints its default instead.
    void FixHexColor(std::string &hex, size_t digits, const std::string &fallback)
    {
        if (!Presets::IsHexColor(hex, digits))
            hex = fallback;
    }
} // namespace

void Config::StoreLayout(CorralWindowConfig &corral, const std::string &topologyKey,
                         const MonitorPosition &layout)
{
    auto current = corral.Layouts.find(topologyKey);
    if (current != corral.Layouts.end() && current->second.MonitorId != layout.MonitorId)
    {
        const std::vector<std::string> ids = LayoutMath::TopologyMonitorIds(topologyKey);
        for (auto it = corral.Layouts.begin(); it != corral.Layouts.end();)
        {
            const std::vector<std::string> sub = LayoutMath::TopologyMonitorIds(it->first);
            if (sub.size() < ids.size() && std::includes(ids.begin(), ids.end(), sub.begin(), sub.end()))
                it = corral.Layouts.erase(it);
            else
                ++it;
        }
    }
    corral.Layouts[topologyKey] = layout;
}

void Config::MigrateLayouts(CorralWindowConfig &corral, const std::string &topologyKey)
{
    if (corral.Layouts.empty())
    {
        const std::vector<std::string> connected = LayoutMath::TopologyMonitorIds(topologyKey);
        for (const auto &entry : corral.MonitorPositions)
        {
            MonitorPosition layout = entry.second;
            layout.MonitorId = entry.first;
            bool home = entry.first == corral.TargetMonitorId &&
                        std::binary_search(connected.begin(), connected.end(), entry.first);
            corral.Layouts[home ? topologyKey
                                : LayoutMath::MonitorKey(entry.first, layout.RefWidth, layout.RefHeight)] = layout;
        }
    }
    corral.TargetMonitorId.clear();
    corral.MonitorPositions.clear();
}

void Config::Normalize(AppConfig &config)
{
    if (!config.Corrals.empty())
        config.FirstCorralSeeded = true; // A layout exists: the first run is over

    static const AppConfig defaultApp;
    static const CorralWindowConfig defaultCorral;
    static const CorralTabConfig defaultTab;

    FixHexColor(config.DefaultColorHex, 8, defaultApp.DefaultColorHex);
    FixHexColor(config.DefaultHeaderFontColor, 6, defaultApp.DefaultHeaderFontColor);
    FixHexColor(config.DefaultIconTintColor, 6, defaultApp.DefaultIconTintColor);
    for (auto &corral : config.Corrals)
    {
        FixHexColor(corral.IconTintColor, 6, defaultCorral.IconTintColor);
        for (auto &tab : corral.Tabs)
        {
            FixHexColor(tab.ColorHex, 8, defaultTab.ColorHex);
            FixHexColor(tab.HeaderFontColor, 6, defaultTab.HeaderFontColor);
        }
    }

    config.DefaultHeaderOpacity = ChromeAlpha::ClampHeaderOpacity(config.DefaultHeaderOpacity);
    config.DefaultBorderOpacity = ChromeAlpha::ClampBorderOpacity(config.DefaultBorderOpacity);
    config.DefaultHeaderFontOpacity = ChromeAlpha::ClampTextOpacity(config.DefaultHeaderFontOpacity);
    config.DefaultIconLabelOpacity = ChromeAlpha::ClampTextOpacity(config.DefaultIconLabelOpacity);

    // The rest to the ranges of the Appearance dialog's sliders, and the font
    // size to what the font picker accepts, so nothing the UI can set changes
    config.DefaultTitleBarHeight = std::clamp(config.DefaultTitleBarHeight, 20, 64);
    config.DefaultIconOpacity = std::clamp(config.DefaultIconOpacity, 5, 255);
    config.DefaultIconTintStrength = std::clamp(config.DefaultIconTintStrength, 0, 255);
    config.DefaultIconSpacingXPercent = std::clamp(config.DefaultIconSpacingXPercent, 50, 200);
    config.DefaultIconSpacingYPercent = std::clamp(config.DefaultIconSpacingYPercent, 50, 200);
    config.DefaultHeaderFontSize = std::clamp(config.DefaultHeaderFontSize, 1, 1638);

    for (auto &corral : config.Corrals)
    {
        corral.HeaderOpacity = ChromeAlpha::ClampHeaderOpacity(corral.HeaderOpacity);
        corral.BorderOpacity = ChromeAlpha::ClampBorderOpacity(corral.BorderOpacity);
        corral.IconLabelOpacity = ChromeAlpha::ClampTextOpacity(corral.IconLabelOpacity);
        corral.TitleBarHeight = std::clamp(corral.TitleBarHeight, 20, 64);
        corral.IconOpacity = std::clamp(corral.IconOpacity, 5, 255);
        corral.IconTintStrength = std::clamp(corral.IconTintStrength, 0, 255);
        corral.IconSpacingXPercent = std::clamp(corral.IconSpacingXPercent, 50, 200);
        corral.IconSpacingYPercent = std::clamp(corral.IconSpacingYPercent, 50, 200);

        for (auto &tab : corral.Tabs)
        {
            tab.HeaderFontOpacity = ChromeAlpha::ClampTextOpacity(tab.HeaderFontOpacity);
            tab.HeaderFontSize = std::clamp(tab.HeaderFontSize, 1, 1638);
        }
    }

    // Own presets get the same cleaning as an imported file
    std::vector<LookPreset> presets;
    presets.swap(config.Presets);
    Presets::Merge(config.Presets, presets);
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

        // The file is there. If it cannot be opened, another program (a virus
        // scanner, a backup tool) is holding it for a moment: wait for it rather
        // than fall back to the backup, which the next save would write over
        // the newer file. This runs once at start, off the UI thread.
        std::ifstream file;
        for (int attempt = 0; attempt < 20; attempt++)
        {
            file.open(path.c_str(), std::ios::binary);
            if (file.is_open())
                break;
            file.clear();
            Sleep(100);
        }
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

bool Config::Save(const AppConfig &config)
{
    return SaveTo(Utf8ToWide(GetConfigPath()), config);
}

bool Config::SaveTo(const std::wstring &wPath, const AppConfig &config)
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
                return false;
            }
            file << data;
            file.flush();
            if (!file.good())
            {
                return false; // Don't replace good config with a bad write
            }
        } // close the stream before renaming

        if (!MoveFileExW(wTmp.c_str(), wPath.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(wTmp.c_str()); // leave the existing config intact
            return false;
        }
        return true;
    }
    catch (...)
    {
        DeleteFileW(wTmp.c_str());
        return false;
    }
}