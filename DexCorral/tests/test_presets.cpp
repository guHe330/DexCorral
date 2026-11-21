/**
 * test_presets.cpp - Unit tests for Presets.
 *
 * The looks shipped with DexCorral and how one is put on a Corral.
 */

#include <gtest/gtest.h>
#include "ChromeAlpha.h"
#include "Presets.h"
#include <cctype>
#include <set>

static bool IsHex(const std::string &s, size_t digits)
{
    if (s.size() != digits + 1 || s[0] != '#')
        return false;
    for (size_t i = 1; i < s.size(); i++)
        if (!isxdigit((unsigned char)s[i]))
            return false;
    return true;
}

TEST(Presets, SixShippedWithDistinctNames)
{
    const auto presets = Presets::Shipped();
    std::set<Str> names;
    for (const auto &p : presets)
        names.insert(p.name);
    EXPECT_EQ(presets.size(), 6u);
    EXPECT_EQ(names.size(), presets.size());
}

TEST(Presets, EveryShippedLookIsWellFormed)
{
    for (const auto &p : Presets::Shipped())
    {
        const AppearanceSettings &l = p.look;
        SCOPED_TRACE(l.ColorHex);
        EXPECT_TRUE(IsHex(l.ColorHex, 8));
        EXPECT_TRUE(IsHex(l.HeaderFontColor, 6));
        EXPECT_TRUE(IsHex(l.IconTintColor, 6));
        EXPECT_FALSE(l.HeaderFontName.empty());
        EXPECT_GE(l.HeaderFontSize, 8);
        EXPECT_LE(l.HeaderFontSize, 14);
        EXPECT_GE(l.TitleBarHeight, 20);
        EXPECT_LE(l.TitleBarHeight, 64);
        EXPECT_GE(l.IconSpacingXPercent, 50);
        EXPECT_LE(l.IconSpacingXPercent, 200);
        EXPECT_GE(l.IconSpacingYPercent, 50);
        EXPECT_LE(l.IconSpacingYPercent, 200);
        // A value the clamps would change is a value the Corral would not show
        EXPECT_EQ(l.HeaderOpacity, ChromeAlpha::ClampHeaderOpacity(l.HeaderOpacity));
        EXPECT_EQ(l.BorderOpacity, ChromeAlpha::ClampBorderOpacity(l.BorderOpacity));
        EXPECT_EQ(l.HeaderFontOpacity, ChromeAlpha::ClampTextOpacity(l.HeaderFontOpacity));
        EXPECT_EQ(l.IconLabelOpacity, ChromeAlpha::ClampTextOpacity(l.IconLabelOpacity));
        EXPECT_GE(l.IconOpacity, 128); // Icons stay clearly visible
        EXPECT_LE(l.IconOpacity, 255);
        EXPECT_GE(l.IconTintStrength, 0);
        EXPECT_LE(l.IconTintStrength, 255);
    }
}

TEST(Presets, ClassicBlueIsTheStockLook)
{
    const AppConfig stock;
    const AppearanceSettings l = Presets::Shipped()[1].look;
    EXPECT_EQ(Presets::Shipped()[1].name, Str::Preset_ClassicBlue);
    EXPECT_EQ(l.ColorHex, stock.DefaultColorHex);
    EXPECT_EQ(l.UseAccentColor, stock.DefaultUseAccentColor);
    EXPECT_EQ(l.TitleBarHeight, stock.DefaultTitleBarHeight);
    EXPECT_EQ(l.HeaderOpacity, stock.DefaultHeaderOpacity);
    EXPECT_EQ(l.BorderOpacity, stock.DefaultBorderOpacity);
    EXPECT_EQ(l.HeaderFontName, stock.DefaultHeaderFontName);
    EXPECT_EQ(l.HeaderFontSize, stock.DefaultHeaderFontSize);
    EXPECT_EQ(l.HeaderFontColor, stock.DefaultHeaderFontColor);
    EXPECT_EQ(l.HeaderFontOpacity, stock.DefaultHeaderFontOpacity);
    EXPECT_EQ(l.IconOpacity, stock.DefaultIconOpacity);
    EXPECT_EQ(l.IconLabelOpacity, stock.DefaultIconLabelOpacity);
    EXPECT_EQ(l.IconTintColor, stock.DefaultIconTintColor);
    EXPECT_EQ(l.IconTintStrength, stock.DefaultIconTintStrength);
    EXPECT_EQ(l.IconSpacingXPercent, stock.DefaultIconSpacingXPercent);
    EXPECT_EQ(l.IconSpacingYPercent, stock.DefaultIconSpacingYPercent);
}

TEST(Presets, OnlyAutomaticFollowsTheAccentColour)
{
    const auto presets = Presets::Shipped();
    EXPECT_EQ(presets[0].name, Str::Preset_Automatic);
    for (size_t i = 0; i < presets.size(); i++)
        EXPECT_EQ(presets[i].look.UseAccentColor, i == 0);
}

TEST(Presets, ApplyLookReachesTheCorralAndEveryTab)
{
    CorralWindowConfig cfg;
    cfg.Tabs.resize(2);
    cfg.Tabs[0].Title = "Work";
    cfg.Tabs[1].Files = {"a.txt"};

    AppearanceSettings look;
    look.ColorHex = "#11223344";
    look.UseAccentColor = true;
    look.TitleBarHeight = 22;
    look.HeaderOpacity = 100;
    look.BorderOpacity = 90;
    look.HeaderFontName = "Arial";
    look.HeaderFontSize = 9;
    look.HeaderFontColor = "#ABCDEF";
    look.HeaderFontOpacity = 200;
    look.IconOpacity = 180;
    look.IconLabelOpacity = 190;
    look.IconTintColor = "#010203";
    look.IconTintStrength = 7;
    look.IconSpacingXPercent = 85;
    look.IconSpacingYPercent = 90;
    Presets::ApplyLook(cfg, look);

    EXPECT_EQ(cfg.TitleBarHeight, 22);
    EXPECT_EQ(cfg.HeaderOpacity, 100);
    EXPECT_EQ(cfg.BorderOpacity, 90);
    EXPECT_EQ(cfg.IconOpacity, 180);
    EXPECT_EQ(cfg.IconLabelOpacity, 190);
    EXPECT_EQ(cfg.IconTintColor, "#010203");
    EXPECT_EQ(cfg.IconTintStrength, 7);
    EXPECT_EQ(cfg.IconSpacingXPercent, 85);
    EXPECT_EQ(cfg.IconSpacingYPercent, 90);
    for (const auto &tab : cfg.Tabs)
    {
        EXPECT_EQ(tab.ColorHex, "#11223344");
        EXPECT_TRUE(tab.UseAccentColor);
        EXPECT_EQ(tab.HeaderFontName, "Arial");
        EXPECT_EQ(tab.HeaderFontSize, 9);
        EXPECT_EQ(tab.HeaderFontColor, "#ABCDEF");
        EXPECT_EQ(tab.HeaderFontOpacity, 200);
    }
    // What a tab holds is not part of its look
    EXPECT_EQ(cfg.Tabs[0].Title, "Work");
    EXPECT_EQ(cfg.Tabs[1].Files.size(), 1u);
}

TEST(Presets, ApplyLookClampsWhatACorralCannotShow)
{
    CorralWindowConfig cfg;
    cfg.Tabs.resize(1);
    AppearanceSettings look;
    look.HeaderOpacity = -5;
    look.BorderOpacity = 999;
    look.HeaderFontOpacity = 999;
    look.IconLabelOpacity = -5;
    Presets::ApplyLook(cfg, look);

    EXPECT_EQ(cfg.HeaderOpacity, ChromeAlpha::ClampHeaderOpacity(-5));
    EXPECT_EQ(cfg.BorderOpacity, ChromeAlpha::ClampBorderOpacity(999));
    EXPECT_EQ(cfg.Tabs[0].HeaderFontOpacity, ChromeAlpha::ClampTextOpacity(999));
    EXPECT_EQ(cfg.IconLabelOpacity, ChromeAlpha::ClampTextOpacity(-5));
}
