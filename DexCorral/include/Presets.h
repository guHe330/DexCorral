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
} // namespace Presets
