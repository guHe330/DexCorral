/**
 * test_config.cpp - Unit tests for Config JSON serialization / deserialization.
 *
 * These tests verify:
 *  - Default field values for all three config structs.
 *  - Full round-trip fidelity (struct -> JSON -> struct).
 *  - Missing JSON fields produce the correct defaults (backward-compat guarantee).
 *  - Unknown JSON fields are silently ignored (forward-compat guarantee).
 *  - Nested collections (tabs, corrals, monitor-positions map) survive round-trips.
 */

#include <gtest/gtest.h>
#include "Config.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static nlohmann::json toJson(const AppConfig& c) {
    return nlohmann::json(c);
}
static nlohmann::json toJson(const CorralWindowConfig& c) {
    return nlohmann::json(c);
}
static nlohmann::json toJson(const CorralTabConfig& c) {
    return nlohmann::json(c);
}

// ---------------------------------------------------------------------------
// Default value tests
// ---------------------------------------------------------------------------

TEST(AppConfig, Defaults) {
    AppConfig c;
    EXPECT_TRUE(c.DesktopIconsVisible);
    EXPECT_EQ(c.DefaultColorHex, "#7F0000FF");
    EXPECT_FALSE(c.HideShortcutArrows);
    EXPECT_EQ(c.DefaultTitleBarHeight, 26);
    EXPECT_EQ(c.DefaultHeaderFontName, "Segoe UI Semibold");
    EXPECT_EQ(c.DefaultHeaderFontSize, 10);
    EXPECT_EQ(c.DefaultHeaderFontColor, "#FFFFFF");
    EXPECT_EQ(c.DefaultHeaderOpacity, 240);
    EXPECT_EQ(c.DefaultBorderOpacity, 255);
    EXPECT_EQ(c.DefaultIconOpacity, 210);
    EXPECT_EQ(c.DefaultIconTintColor, "#0000FF");
    EXPECT_EQ(c.DefaultIconTintStrength, 28);
    EXPECT_EQ(c.DefaultIconSpacingXPercent, 100);
    EXPECT_EQ(c.DefaultIconSpacingYPercent, 100);
    EXPECT_TRUE(c.Corrals.empty());
}

TEST(CorralWindowConfig, Defaults) {
    CorralWindowConfig c;
    EXPECT_DOUBLE_EQ(c.Left,   0.0);
    EXPECT_DOUBLE_EQ(c.Top,    0.0);
    EXPECT_DOUBLE_EQ(c.Width,  300.0);
    EXPECT_DOUBLE_EQ(c.Height, 200.0);
    EXPECT_FALSE(c.IsRolledUp);
    EXPECT_EQ(c.ActiveTabIndex, 0);
    EXPECT_EQ(c.TitleBarHeight, 32);  // Header height is per-corral
    EXPECT_EQ(c.HeaderOpacity, 240);  // Matches the previously hard-coded active tab alpha
    EXPECT_EQ(c.BorderOpacity, 255);  // Matches the previously hard-coded opaque border
    EXPECT_EQ(c.IconOpacity, 255);
    EXPECT_EQ(c.IconTintColor, "#000000");
    EXPECT_EQ(c.IconTintStrength, 0);
    EXPECT_EQ(c.IconSpacingXPercent, 100);
    EXPECT_EQ(c.IconSpacingYPercent, 100);
    EXPECT_TRUE(c.Tabs.empty());
    EXPECT_TRUE(c.TargetMonitorId.empty());
    EXPECT_TRUE(c.MonitorPositions.empty());
}

TEST(CorralTabConfig, Defaults) {
    CorralTabConfig c;
    EXPECT_EQ(c.Title, "New Tab");
    EXPECT_EQ(c.ColorHex, "#99000000");
    EXPECT_TRUE(c.Files.empty());
    EXPECT_EQ(c.ViewModeInt, 0);
    EXPECT_FALSE(c.IsCatchAll);
    EXPECT_FALSE(c.IsVirtual);
    EXPECT_TRUE(c.VirtualFolderPath.empty());
    EXPECT_TRUE(c.CurrentSubPath.empty());
    EXPECT_TRUE(c.DetailsColumnWidths.empty());
    EXPECT_EQ(c.DetailsSortColumn, 0);
    EXPECT_TRUE(c.DetailsSortAscending);
    // Header font is per-tab
    EXPECT_EQ(c.HeaderFontName, "Segoe UI");
    EXPECT_EQ(c.HeaderFontSize, 10);
    EXPECT_EQ(c.HeaderFontColor, "#FFFFFF");
}

// ---------------------------------------------------------------------------
// Round-trip tests
// ---------------------------------------------------------------------------

TEST(CorralTabConfig, RoundTrip) {
    CorralTabConfig orig;
    orig.Title = "Work";
    orig.ColorHex = "#FF112233";
    orig.Files = { "notepad.lnk", "shell:{645FF040-5081-101B-9F08-00AA002F954E}" };
    orig.SetViewMode(ViewMode::Details);
    orig.IsCatchAll = true;
    orig.IsVirtual = true;
    orig.VirtualFolderPath = "C:/Users/test/Documents";
    orig.CurrentSubPath = "Projects\\2026";
    orig.DetailsColumnWidths = { 180, 90, 70, 110 };
    orig.DetailsSortColumn = 2;
    orig.DetailsSortAscending = false;

    auto j = toJson(orig);
    auto back = j.get<CorralTabConfig>();

    EXPECT_EQ(back.Title, orig.Title);
    EXPECT_EQ(back.ColorHex, orig.ColorHex);
    EXPECT_EQ(back.Files, orig.Files);
    EXPECT_EQ(back.GetViewMode(), ViewMode::Details);
    EXPECT_TRUE(back.IsCatchAll);
    EXPECT_TRUE(back.IsVirtual);
    EXPECT_EQ(back.VirtualFolderPath, orig.VirtualFolderPath);
    EXPECT_EQ(back.CurrentSubPath, orig.CurrentSubPath);
    EXPECT_EQ(back.DetailsColumnWidths, orig.DetailsColumnWidths);
    EXPECT_EQ(back.DetailsSortColumn, 2);
    EXPECT_FALSE(back.DetailsSortAscending);
}

TEST(MissingField, TabSortFields_GetDefaults) {
    // Old config predating the details sort/column feature
    nlohmann::json j = R"({"Title":"T","IsVirtual":true})"_json;
    auto tab = j.get<CorralTabConfig>();
    EXPECT_TRUE(tab.CurrentSubPath.empty());
    EXPECT_TRUE(tab.DetailsColumnWidths.empty());
    EXPECT_EQ(tab.DetailsSortColumn, 0);
    EXPECT_TRUE(tab.DetailsSortAscending);
}

TEST(CorralWindowConfig, RoundTrip) {
    CorralWindowConfig orig;
    orig.Left = 100.5; orig.Top = 200.5; orig.Width = 400.0; orig.Height = 300.0;
    orig.IsRolledUp = true;
    orig.ActiveTabIndex = 2;
    orig.TitleBarHeight = 48;
    orig.IconOpacity = 180;
    orig.IconTintColor = "#FF0000";
    orig.IconTintStrength = 64;
    orig.IconSpacingXPercent = 150;
    orig.IconSpacingYPercent = 120;
    orig.TargetMonitorId = "DISPLAY1";

    CorralTabConfig tab;
    tab.Title = "Alpha";
    tab.HeaderFontName = "Arial";
    tab.HeaderFontSize = 12;
    tab.HeaderFontColor = "#AABBCC";
    orig.Tabs.push_back(tab);

    MonitorPosition mp;
    mp.Left = 10; mp.Top = 20; mp.Width = 300; mp.Height = 200;
    mp.RefWidth = 1920; mp.RefHeight = 1080;
    orig.MonitorPositions["DISPLAY1"] = mp;

    auto back = toJson(orig).get<CorralWindowConfig>();

    EXPECT_DOUBLE_EQ(back.Left,   orig.Left);
    EXPECT_DOUBLE_EQ(back.Top,    orig.Top);
    EXPECT_DOUBLE_EQ(back.Width,  orig.Width);
    EXPECT_DOUBLE_EQ(back.Height, orig.Height);
    EXPECT_TRUE(back.IsRolledUp);
    EXPECT_EQ(back.ActiveTabIndex, 2);
    EXPECT_EQ(back.TitleBarHeight, 48);
    EXPECT_EQ(back.IconOpacity, 180);
    EXPECT_EQ(back.IconTintColor, "#FF0000");
    EXPECT_EQ(back.IconTintStrength, 64);
    EXPECT_EQ(back.IconSpacingXPercent, 150);
    EXPECT_EQ(back.IconSpacingYPercent, 120);
    EXPECT_EQ(back.TargetMonitorId, "DISPLAY1");
    ASSERT_EQ(back.Tabs.size(), 1u);
    EXPECT_EQ(back.Tabs[0].Title, "Alpha");
    EXPECT_EQ(back.Tabs[0].HeaderFontName, "Arial");
    EXPECT_EQ(back.Tabs[0].HeaderFontSize, 12);
    EXPECT_EQ(back.Tabs[0].HeaderFontColor, "#AABBCC");
    ASSERT_EQ(back.MonitorPositions.count("DISPLAY1"), 1u);
    EXPECT_EQ(back.MonitorPositions.at("DISPLAY1").RefWidth, 1920);
}

TEST(AppConfig, RoundTrip) {
    AppConfig orig;
    orig.DesktopIconsVisible = false;
    orig.DefaultColorHex = "#AAFFFFFF";
    orig.HideShortcutArrows = true;
    orig.DefaultTitleBarHeight = 40;
    orig.DefaultIconOpacity = 200;
    orig.DefaultIconSpacingXPercent = 130;

    CorralWindowConfig corral;
    corral.Width = 500;
    orig.Corrals.push_back(corral);
    orig.Corrals.push_back(corral);

    auto back = toJson(orig).get<AppConfig>();

    EXPECT_FALSE(back.DesktopIconsVisible);
    EXPECT_EQ(back.DefaultColorHex, "#AAFFFFFF");
    EXPECT_TRUE(back.HideShortcutArrows);
    EXPECT_EQ(back.DefaultTitleBarHeight, 40);
    EXPECT_EQ(back.DefaultIconOpacity, 200);
    EXPECT_EQ(back.DefaultIconSpacingXPercent, 130);
    ASSERT_EQ(back.Corrals.size(), 2u);
    EXPECT_DOUBLE_EQ(back.Corrals[0].Width, 500.0);
}

// ---------------------------------------------------------------------------
// Missing-field → default tests  (backward-compat guarantee)
// ---------------------------------------------------------------------------

TEST(MissingField, TabColorHex_GetsDefault) {
    // JSON without "ColorHex" -> default "#99000000"
    nlohmann::json j = R"({"Title":"X","Files":[]})"_json;
    auto tab = j.get<CorralTabConfig>();
    EXPECT_EQ(tab.ColorHex, "#99000000");
}

TEST(MissingField, WindowTitleBarHeight_GetsDefault) {
    nlohmann::json j = R"({"Left":0,"Top":0,"Width":300,"Height":200})"_json;
    auto cfg = j.get<CorralWindowConfig>();
    EXPECT_EQ(cfg.TitleBarHeight, 32);
}

TEST(MissingField, AppIconOpacity_GetsDefault) {
    nlohmann::json j = R"({"Corrals":[]})"_json;
    auto cfg = j.get<AppConfig>();
    EXPECT_EQ(cfg.DefaultIconOpacity, 210);
}

TEST(MissingField, IconTintStrength_GetsDefault) {
    // Simulates loading an old config that predates the tint feature
    nlohmann::json j = R"({"Left":0,"Top":0,"Width":300,"Height":200})"_json;
    auto cfg = j.get<CorralWindowConfig>();
    EXPECT_EQ(cfg.IconTintStrength, 0);
}

TEST(MissingField, WindowIsLocked_GetsDefault) {
    // Old config predating Lock Position -> loads unlocked
    nlohmann::json j = R"({"Left":0,"Top":0,"Width":300,"Height":200})"_json;
    auto cfg = j.get<CorralWindowConfig>();
    EXPECT_FALSE(cfg.IsLocked);
}

TEST(CorralWindowConfig, IsLockedRoundTrip) {
    CorralWindowConfig orig;
    orig.IsLocked = true;
    nlohmann::json j = orig;
    auto back = j.get<CorralWindowConfig>();
    EXPECT_TRUE(back.IsLocked);
}

TEST(MissingField, SpacingPercents_DefaultTo100) {
    nlohmann::json j = R"({})"_json;
    auto cfg = j.get<CorralWindowConfig>();
    EXPECT_EQ(cfg.IconSpacingXPercent, 100);
    EXPECT_EQ(cfg.IconSpacingYPercent, 100);
}

TEST(MissingField, TabIsVirtual_DefaultsFalse) {
    nlohmann::json j = R"({"Title":"T"})"_json;
    auto tab = j.get<CorralTabConfig>();
    EXPECT_FALSE(tab.IsVirtual);
}

// ---------------------------------------------------------------------------
// Unknown-field → no throw tests  (forward-compat guarantee)
// ---------------------------------------------------------------------------

TEST(UnknownField, ExtraKeyDoesNotThrow) {
    // A config written by a future version might have fields this version doesn't know.
    nlohmann::json j = R"({
        "Title": "Tab",
        "FutureFeatureX": 99,
        "AnotherNewField": "hello"
    })"_json;
    EXPECT_NO_THROW(j.get<CorralTabConfig>());
}

TEST(UnknownField, WindowConfigExtraKeyDoesNotThrow) {
    nlohmann::json j = R"({"Left":0,"Top":0,"Width":300,"Height":200,"NewUnknown":true})"_json;
    EXPECT_NO_THROW(j.get<CorralWindowConfig>());
}

// ---------------------------------------------------------------------------
// ViewMode enum round-trip
// ---------------------------------------------------------------------------

TEST(ViewMode, RoundTrip) {
    CorralTabConfig tab;
    tab.SetViewMode(ViewMode::Details);
    EXPECT_EQ(tab.GetViewMode(), ViewMode::Details);
    EXPECT_EQ(tab.ViewModeInt, 3);

    auto j = toJson(tab);
    auto back = j.get<CorralTabConfig>();
    EXPECT_EQ(back.GetViewMode(), ViewMode::Details);
}

TEST(ViewMode, AllValuesPreserved) {
    for (int v = 0; v <= 3; v++) {
        CorralTabConfig tab;
        tab.ViewModeInt = v;
        auto back = toJson(tab).get<CorralTabConfig>();
        EXPECT_EQ(back.ViewModeInt, v);
    }
}

// ---------------------------------------------------------------------------
// MonitorPositions map round-trip
// ---------------------------------------------------------------------------

TEST(MonitorPositions, TwoMonitorsRoundTrip) {
    CorralWindowConfig cfg;
    cfg.MonitorPositions["MON1"] = { 0, 0, 400, 300, 1920, 1080 };
    cfg.MonitorPositions["MON2"] = { 50, 50, 350, 250, 2560, 1440 };

    auto back = toJson(cfg).get<CorralWindowConfig>();
    ASSERT_EQ(back.MonitorPositions.size(), 2u);
    EXPECT_EQ(back.MonitorPositions.at("MON1").Width, 400);
    EXPECT_EQ(back.MonitorPositions.at("MON2").RefWidth, 2560);
}

// ---------------------------------------------------------------------------
// Files vector round-trip
// ---------------------------------------------------------------------------

TEST(TabFiles, VectorRoundTrip) {
    CorralTabConfig tab;
    tab.Files = {
        "notepad.lnk",
        "shell:{645FF040-5081-101B-9F08-00AA002F954E}",
        "calc.exe"
    };
    auto back = toJson(tab).get<CorralTabConfig>();
    ASSERT_EQ(back.Files.size(), 3u);
    EXPECT_EQ(back.Files[1], "shell:{645FF040-5081-101B-9F08-00AA002F954E}");
}

// ---------------------------------------------------------------------------
// Multiple corrals round-trip
// ---------------------------------------------------------------------------

TEST(AppConfig, MultipleCoralsRoundTrip) {
    AppConfig cfg;
    for (int i = 0; i < 3; i++) {
        CorralWindowConfig c;
        c.Left = i * 100.0;
        cfg.Corrals.push_back(c);
    }
    auto back = toJson(cfg).get<AppConfig>();
    ASSERT_EQ(back.Corrals.size(), 3u);
    EXPECT_DOUBLE_EQ(back.Corrals[2].Left, 200.0);
}

// ---------------------------------------------------------------------------
// Chrome opacity: backward compatibility and load-time clamping
// ---------------------------------------------------------------------------

TEST(MissingField, ChromeOpacity_GetsDefaults) {
    // Config written before header/border opacity existed: the corral must look
    // exactly as it did, i.e. the previously hard-coded values.
    nlohmann::json j = R"({"Left":10,"Top":20,"TitleBarHeight":26})"_json;
    auto cfg = j.get<CorralWindowConfig>();
    EXPECT_EQ(cfg.HeaderOpacity, 240);
    EXPECT_EQ(cfg.BorderOpacity, 255);
}

TEST(ChromeOpacity, RoundTrip) {
    CorralWindowConfig orig;
    orig.HeaderOpacity = 96;
    orig.BorderOpacity = 0;
    auto back = toJson(orig).get<CorralWindowConfig>();
    EXPECT_EQ(back.HeaderOpacity, 96);
    EXPECT_EQ(back.BorderOpacity, 0);
}

TEST(Normalize, ClampsHandEditedHeaderOpacity) {
    // A hand-edited config must not be able to produce a corral that is both
    // invisible and undraggable.
    AppConfig cfg;
    CorralWindowConfig corral;
    corral.HeaderOpacity = 0;
    corral.BorderOpacity = 900;
    cfg.Corrals.push_back(corral);
    cfg.DefaultHeaderOpacity = -5;
    cfg.DefaultBorderOpacity = -5;

    Config::Normalize(cfg);

    EXPECT_EQ(cfg.Corrals[0].HeaderOpacity, 20);
    EXPECT_EQ(cfg.Corrals[0].BorderOpacity, 255);
    EXPECT_EQ(cfg.DefaultHeaderOpacity, 20);
    EXPECT_EQ(cfg.DefaultBorderOpacity, 0);
}

TEST(Normalize, LeavesValidValuesAlone) {
    AppConfig cfg;
    CorralWindowConfig corral;
    corral.HeaderOpacity = 128;
    corral.BorderOpacity = 0;
    cfg.Corrals.push_back(corral);

    Config::Normalize(cfg);

    EXPECT_EQ(cfg.Corrals[0].HeaderOpacity, 128);
    EXPECT_EQ(cfg.Corrals[0].BorderOpacity, 0);
}
