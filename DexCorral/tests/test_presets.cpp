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

TEST(Presets, CleanNameTrimsAndCuts)
{
    EXPECT_EQ(Presets::CleanName("  Night shift 	"), "Night shift");
    EXPECT_EQ(Presets::CleanName("   "), "");
    EXPECT_EQ(Presets::CleanName(std::string(60, 'a')).size(), Presets::MAX_NAME);
    // Counts characters, not bytes: 39 letters and a two-byte umlaut are 40
    const std::string umlaut = "\xC3\xA4";
    EXPECT_EQ(Presets::CleanName(std::string(39, 'a') + umlaut + "b"), std::string(39, 'a') + umlaut);
}

TEST(Presets, SanitizeRepairsWhatACorralCannotShow)
{
    AppearanceSettings bad;
    bad.ColorHex = "blue";
    bad.HeaderFontColor = "#zzzzzz";
    bad.IconTintColor = "";
    bad.TitleBarHeight = 500;
    bad.HeaderOpacity = -1;
    bad.IconOpacity = -1;
    bad.IconTintStrength = 999;
    bad.IconSpacingXPercent = 1;
    bad.IconSpacingYPercent = 999;
    bad.HeaderFontSize = 0;
    bad.HeaderFontName = "";
    const AppearanceSettings l = Presets::Sanitize(bad);
    const AppearanceSettings stock = Presets::Classic();

    EXPECT_EQ(l.ColorHex, stock.ColorHex);
    EXPECT_EQ(l.HeaderFontColor, stock.HeaderFontColor);
    EXPECT_EQ(l.IconTintColor, stock.IconTintColor);
    EXPECT_EQ(l.TitleBarHeight, 64);
    EXPECT_EQ(l.HeaderOpacity, ChromeAlpha::ClampHeaderOpacity(-1));
    EXPECT_EQ(l.IconOpacity, 5);
    EXPECT_EQ(l.IconTintStrength, 255);
    EXPECT_EQ(l.IconSpacingXPercent, 50);
    EXPECT_EQ(l.IconSpacingYPercent, 200);
    EXPECT_EQ(l.HeaderFontSize, 6);
    EXPECT_EQ(l.HeaderFontName, stock.HeaderFontName);
}

TEST(Presets, SanitizeLeavesAGoodLookAlone)
{
    for (const auto &p : Presets::Shipped())
    {
        const AppearanceSettings l = Presets::Sanitize(p.look);
        EXPECT_EQ(l.ColorHex, p.look.ColorHex);
        EXPECT_EQ(l.TitleBarHeight, p.look.TitleBarHeight);
        EXPECT_EQ(l.HeaderOpacity, p.look.HeaderOpacity);
        EXPECT_EQ(l.IconOpacity, p.look.IconOpacity);
        EXPECT_EQ(l.HeaderFontName, p.look.HeaderFontName);
        EXPECT_EQ(l.UseAccentColor, p.look.UseAccentColor);
    }
}

TEST(Presets, UpsertAddsReplacesAndRefuses)
{
    std::vector<LookPreset> own;
    AppearanceSettings a = Presets::Classic();
    EXPECT_EQ(Presets::Upsert(own, " Mine ", a), Presets::UpsertResult::Added);
    ASSERT_EQ(own.size(), 1u);
    EXPECT_EQ(own[0].Name, "Mine");

    a.TitleBarHeight = 40;
    EXPECT_EQ(Presets::Upsert(own, "Mine", a), Presets::UpsertResult::Replaced);
    ASSERT_EQ(own.size(), 1u);
    EXPECT_EQ(own[0].Look.TitleBarHeight, 40);

    EXPECT_EQ(Presets::Upsert(own, "  ", a), Presets::UpsertResult::BadName);
    while (own.size() < Presets::MAX_OWN)
        own.push_back({"n" + std::to_string(own.size()), a});
    EXPECT_EQ(Presets::Upsert(own, "one too many", a), Presets::UpsertResult::Full);
    EXPECT_EQ(Presets::Upsert(own, "Mine", a), Presets::UpsertResult::Replaced); // Replacing needs no room
    EXPECT_EQ(own.size(), Presets::MAX_OWN);
}

TEST(Presets, RemoveTakesOnlyTheNamedOne)
{
    std::vector<LookPreset> own = {{"A", Presets::Classic()}, {"B", Presets::Classic()}};
    EXPECT_TRUE(Presets::Remove(own, "A"));
    EXPECT_FALSE(Presets::Remove(own, "A"));
    ASSERT_EQ(own.size(), 1u);
    EXPECT_EQ(own[0].Name, "B");
}

TEST(Presets, ExportThenImportGivesTheSamePresets)
{
    AppearanceSettings look = Presets::Shipped()[3].look;
    look.HeaderFontName = "Arial";
    const std::vector<LookPreset> own = {{"Night", look}, {"Day", Presets::Classic()}};

    std::vector<LookPreset> back;
    ASSERT_TRUE(Presets::Import(Presets::Export(own), back));
    ASSERT_EQ(back.size(), 2u);
    EXPECT_EQ(back[0].Name, "Night");
    EXPECT_EQ(back[0].Look.ColorHex, look.ColorHex);
    EXPECT_EQ(back[0].Look.HeaderFontName, "Arial");
    EXPECT_EQ(back[0].Look.BorderOpacity, look.BorderOpacity);
    EXPECT_EQ(back[1].Name, "Day");
}

TEST(Presets, ImportRefusesWhatIsNotAPresetFile)
{
    std::vector<LookPreset> out;
    EXPECT_FALSE(Presets::Import("", out));
    EXPECT_FALSE(Presets::Import("not json at all {", out));
    EXPECT_FALSE(Presets::Import("[1,2,3]", out));
    EXPECT_FALSE(Presets::Import(R"({"Presets":[{"Name":"x"}]})", out)); // No marker: some other JSON
    EXPECT_FALSE(Presets::Import(R"({"DexCorralPresets":1,"Presets":"none"})", out));
    EXPECT_TRUE(out.empty());
}

TEST(Presets, ImportCleansEachPreset)
{
    std::vector<LookPreset> out;
    ASSERT_TRUE(Presets::Import(
        R"({"DexCorralPresets":1,"Presets":[
              {"Name":"  Odd  ","Look":{"ColorHex":"nonsense","TitleBarHeight":900}},
              {"Name":"","Look":{}},
              {"Look":{}},
              "not an object",
              {"Name":"Plain"}]})",
        out));
    ASSERT_EQ(out.size(), 2u); // The nameless ones and the non-object are skipped
    EXPECT_EQ(out[0].Name, "Odd");
    EXPECT_EQ(out[0].Look.ColorHex, Presets::Classic().ColorHex);
    EXPECT_EQ(out[0].Look.TitleBarHeight, 64);
    EXPECT_EQ(out[1].Name, "Plain");
}

TEST(Presets, MergeReplacesSameNamesAndStopsWhenFull)
{
    AppearanceSettings changed = Presets::Classic();
    changed.TitleBarHeight = 50;
    std::vector<LookPreset> own = {{"A", Presets::Classic()}};
    EXPECT_EQ(Presets::Merge(own, {{"A", changed}, {"B", changed}}), 2);
    ASSERT_EQ(own.size(), 2u);
    EXPECT_EQ(own[0].Look.TitleBarHeight, 50);

    std::vector<LookPreset> many;
    for (size_t i = 0; i < Presets::MAX_OWN + 5; i++)
        many.push_back({"m" + std::to_string(i), changed});
    int full = -1;
    EXPECT_EQ(Presets::Merge(own, many, &full), (int)(Presets::MAX_OWN - 2));
    EXPECT_EQ(own.size(), Presets::MAX_OWN);
    EXPECT_EQ(full, 7); // 55 offered, room for 48
}

TEST(Presets, SanitizeKeepsALongFontNameInAnotherScript)
{
    // 20 two-byte characters: 40 bytes, but well within the 31 characters a font name may have
    AppearanceSettings look = Presets::Classic();
    look.HeaderFontName.clear();
    for (int i = 0; i < 20; i++)
        look.HeaderFontName += "\xC3\xA4";
    EXPECT_EQ(Presets::Sanitize(look).HeaderFontName, look.HeaderFontName);
}
