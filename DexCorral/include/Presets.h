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
 * Presets.h - The looks shipped with DexCorral, and how one is put on a Corral
 *
 * Pure logic, kept apart from App so it can be unit tested.
 */

#pragma once
#include "ChromeAlpha.h"
#include "Config.h"
#include "Strings.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace Presets
{
struct Preset
{
    Str name;
    AppearanceSettings look;
};

/// The stock look: what a fresh install gives a new Corral
inline AppearanceSettings Classic()
{
    const AppConfig stock;
    AppearanceSettings l;
    l.ColorHex = stock.DefaultColorHex;
    l.UseAccentColor = stock.DefaultUseAccentColor;
    l.TitleBarHeight = stock.DefaultTitleBarHeight;
    l.HeaderOpacity = stock.DefaultHeaderOpacity;
    l.BorderOpacity = stock.DefaultBorderOpacity;
    l.HeaderFontName = stock.DefaultHeaderFontName;
    l.HeaderFontSize = stock.DefaultHeaderFontSize;
    l.HeaderFontColor = stock.DefaultHeaderFontColor;
    l.HeaderFontOpacity = stock.DefaultHeaderFontOpacity;
    l.IconOpacity = stock.DefaultIconOpacity;
    l.IconLabelOpacity = stock.DefaultIconLabelOpacity;
    l.IconTintColor = stock.DefaultIconTintColor;
    l.IconTintStrength = stock.DefaultIconTintStrength;
    l.IconSpacingXPercent = stock.DefaultIconSpacingXPercent;
    l.IconSpacingYPercent = stock.DefaultIconSpacingYPercent;
    return l;
}

/// Every shipped preset is the stock look with a few values changed
inline std::vector<Preset> Shipped()
{
    AppearanceSettings automatic = Classic();
    automatic.UseAccentColor = true;
    automatic.IconTintStrength = 0; // The stock blue tint would fight any other accent

    AppearanceSettings lightGlass = Classic();
    lightGlass.ColorHex = "#40FFFFFF";
    lightGlass.HeaderOpacity = 140;
    lightGlass.BorderOpacity = 120;
    lightGlass.HeaderFontColor = "#202020";
    lightGlass.IconOpacity = 255;
    lightGlass.IconTintStrength = 0;

    AppearanceSettings darkGlass = Classic();
    darkGlass.ColorHex = "#59000000";
    darkGlass.HeaderOpacity = 160;
    darkGlass.BorderOpacity = 90;
    darkGlass.IconOpacity = 240;
    darkGlass.IconTintStrength = 0;

    AppearanceSettings graphite = Classic();
    graphite.ColorHex = "#E62B2B2B";
    graphite.HeaderOpacity = 255;
    graphite.HeaderFontColor = "#F0F0F0";
    graphite.IconOpacity = 255;
    graphite.IconTintStrength = 0;

    AppearanceSettings compact = Classic();
    compact.TitleBarHeight = 22;
    compact.HeaderFontSize = 9;
    compact.IconSpacingXPercent = 85;
    compact.IconSpacingYPercent = 85;

    return {
        {Str::Preset_Automatic, automatic},
        {Str::Preset_ClassicBlue, Classic()},
        {Str::Preset_LightGlass, lightGlass},
        {Str::Preset_DarkGlass, darkGlass},
        {Str::Preset_Graphite, graphite},
        {Str::Preset_Compact, compact},
    };
}

/// Puts the whole look on a Corral and on every one of its tabs. What the
/// Corral holds (tabs, files, place, size) stays as it is.
inline void ApplyLook(CorralWindowConfig &cfg, const AppearanceSettings &look)
{
    cfg.TitleBarHeight = look.TitleBarHeight;
    cfg.HeaderOpacity = ChromeAlpha::ClampHeaderOpacity(look.HeaderOpacity);
    cfg.BorderOpacity = ChromeAlpha::ClampBorderOpacity(look.BorderOpacity);
    cfg.IconOpacity = look.IconOpacity;
    cfg.IconLabelOpacity = ChromeAlpha::ClampTextOpacity(look.IconLabelOpacity);
    cfg.IconTintColor = look.IconTintColor;
    cfg.IconTintStrength = look.IconTintStrength;
    cfg.IconSpacingXPercent = look.IconSpacingXPercent;
    cfg.IconSpacingYPercent = look.IconSpacingYPercent;
    for (auto &tab : cfg.Tabs)
    {
        tab.ColorHex = look.ColorHex;
        tab.UseAccentColor = look.UseAccentColor;
        tab.HeaderFontName = look.HeaderFontName;
        tab.HeaderFontSize = look.HeaderFontSize;
        tab.HeaderFontColor = look.HeaderFontColor;
        tab.HeaderFontOpacity = ChromeAlpha::ClampTextOpacity(look.HeaderFontOpacity);
    }
}

// ---- Own presets: looks the user saved under a name ----

constexpr size_t MAX_NAME = 40; // Characters
constexpr size_t MAX_OWN = 50;

/// How many characters a UTF-8 text has (bytes that do not continue a character)
inline size_t Characters(const std::string &utf8)
{
    return (size_t)std::count_if(utf8.begin(), utf8.end(), [](unsigned char c) { return (c & 0xC0) != 0x80; });
}

/// A name as it is stored: no blanks around it and at most MAX_NAME
/// characters. Empty means "not a name".
inline std::string CleanName(std::string name)
{
    auto blank = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!name.empty() && blank((unsigned char)name.back()))
        name.pop_back();
    size_t first = 0;
    while (first < name.size() && blank((unsigned char)name[first]))
        first++;
    name.erase(0, first);
    size_t characters = 0;
    for (size_t i = 0; i < name.size(); i++)
    {
        if (((unsigned char)name[i] & 0xC0) != 0x80 && ++characters > MAX_NAME)
        {
            name.resize(i); // The first byte of the character that is one too many
            break;
        }
    }
    return name;
}

/// A look from outside (an imported file, a hand-edited config) with every
/// value brought into the range the Appearance dialog offers. A colour that
/// is not a colour becomes the stock one.
inline AppearanceSettings Sanitize(AppearanceSettings look)
{
    const AppearanceSettings stock = Classic();
    auto isHex = [](const std::string &s, size_t digits)
    {
        if (s.size() != digits + 1 || s[0] != '#')
            return false;
        return std::all_of(s.begin() + 1, s.end(), [](unsigned char c) { return isxdigit(c) != 0; });
    };
    auto clamp = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };

    if (!isHex(look.ColorHex, 8))
        look.ColorHex = stock.ColorHex;
    if (!isHex(look.HeaderFontColor, 6))
        look.HeaderFontColor = stock.HeaderFontColor;
    if (!isHex(look.IconTintColor, 6))
        look.IconTintColor = stock.IconTintColor;
    if (look.HeaderFontName.empty() || Characters(look.HeaderFontName) > 31) // LF_FACESIZE - 1
        look.HeaderFontName = stock.HeaderFontName;
    look.HeaderFontSize = clamp(look.HeaderFontSize, 6, 72);
    look.TitleBarHeight = clamp(look.TitleBarHeight, 20, 64);
    look.HeaderOpacity = ChromeAlpha::ClampHeaderOpacity(look.HeaderOpacity);
    look.BorderOpacity = ChromeAlpha::ClampBorderOpacity(look.BorderOpacity);
    look.HeaderFontOpacity = ChromeAlpha::ClampTextOpacity(look.HeaderFontOpacity);
    look.IconLabelOpacity = ChromeAlpha::ClampTextOpacity(look.IconLabelOpacity);
    look.IconOpacity = clamp(look.IconOpacity, 5, 255);
    look.IconTintStrength = clamp(look.IconTintStrength, 0, 255);
    look.IconSpacingXPercent = clamp(look.IconSpacingXPercent, 50, 200);
    look.IconSpacingYPercent = clamp(look.IconSpacingYPercent, 50, 200);
    return look;
}

enum class UpsertResult
{
    Added,
    Replaced,
    Full,   // MAX_OWN presets exist and this would be one more
    BadName // Nothing left of the name after CleanName
};

/// Saves `look` under `name`: replaces the preset of that name, else adds one
inline UpsertResult Upsert(std::vector<LookPreset> &own, const std::string &name, const AppearanceSettings &look)
{
    const std::string clean = CleanName(name);
    if (clean.empty())
        return UpsertResult::BadName;
    for (auto &preset : own)
    {
        if (preset.Name == clean)
        {
            preset.Look = Sanitize(look);
            return UpsertResult::Replaced;
        }
    }
    if (own.size() >= MAX_OWN)
        return UpsertResult::Full;
    own.push_back({clean, Sanitize(look)});
    return UpsertResult::Added;
}

inline bool Remove(std::vector<LookPreset> &own, const std::string &name)
{
    auto it = std::find_if(own.begin(), own.end(), [&](const LookPreset &p) { return p.Name == name; });
    if (it == own.end())
        return false;
    own.erase(it);
    return true;
}

/// Takes every preset of `from` into `own` through Upsert. Returns how many were taken.
inline int Merge(std::vector<LookPreset> &own, const std::vector<LookPreset> &from)
{
    int taken = 0;
    for (const auto &preset : from)
    {
        const UpsertResult r = Upsert(own, preset.Name, preset.Look);
        if (r == UpsertResult::Added || r == UpsertResult::Replaced)
            taken++;
    }
    return taken;
}

/// The text of an export file
inline std::string Export(const std::vector<LookPreset> &own)
{
    nlohmann::json file;
    file["DexCorralPresets"] = 1;
    file["Presets"] = own;
    return file.dump(2);
}

/// Reads the text of an export file into `out` (cleaned, nameless entries
/// skipped). False, and `out` untouched, if the text is not such a file.
inline bool Import(const std::string &text, std::vector<LookPreset> &out)
{
    const nlohmann::json file = nlohmann::json::parse(text, nullptr, false);
    if (!file.is_object() || !file.contains("DexCorralPresets") || !file.contains("Presets") ||
        !file["Presets"].is_array())
        return false;

    std::vector<LookPreset> read;
    for (const auto &entry : file["Presets"])
    {
        if (!entry.is_object())
            continue;
        LookPreset preset;
        try
        {
            preset = entry.get<LookPreset>();
        }
        catch (const nlohmann::json::exception &)
        {
            continue; // A value of the wrong type: this entry is not a preset
        }
        preset.Name = CleanName(preset.Name);
        if (preset.Name.empty())
            continue;
        preset.Look = Sanitize(preset.Look);
        read.push_back(std::move(preset));
    }
    out = std::move(read);
    return true;
}
} // namespace Presets
