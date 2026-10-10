/**
 * test_hotkey.cpp - Unit tests for ParseHotkey.
 *
 * Covers the accepted spellings, and the inputs that must be refused so a bad
 * config.json value can never register a hotkey that swallows a plain key.
 */

#include <gtest/gtest.h>
#include "Hotkey.h"
#include <Windows.h>

TEST(ParseHotkey, DefaultCombination) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey(DEFAULT_QUICK_HIDE_HOTKEY, spec));
    EXPECT_EQ(spec.modifiers, (unsigned)(MOD_CONTROL | MOD_ALT));
    EXPECT_EQ(spec.vk, (unsigned)'H');
}

TEST(ParseHotkey, CaseAndSpacesAreIgnored) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey("  ctrl + SHIFT+ f9 ", spec));
    EXPECT_EQ(spec.modifiers, (unsigned)(MOD_CONTROL | MOD_SHIFT));
    EXPECT_EQ(spec.vk, (unsigned)VK_F9);
}

TEST(ParseHotkey, AllModifiersAndAliases) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey("Control+Alt+Shift+Windows+D", spec));
    EXPECT_EQ(spec.modifiers, (unsigned)(MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN));
    EXPECT_EQ(spec.vk, (unsigned)'D');

    ASSERT_TRUE(ParseHotkey("Strg+Alt+H", spec)); // German keyboard label
    EXPECT_EQ(spec.modifiers, (unsigned)(MOD_CONTROL | MOD_ALT));
}

TEST(ParseHotkey, DigitsAndFunctionKeyRange) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey("Win+Alt+0", spec));
    EXPECT_EQ(spec.vk, (unsigned)'0');
    ASSERT_TRUE(ParseHotkey("Ctrl+F1", spec));
    EXPECT_EQ(spec.vk, (unsigned)VK_F1);
    ASSERT_TRUE(ParseHotkey("Ctrl+F24", spec));
    EXPECT_EQ(spec.vk, (unsigned)VK_F24);
    EXPECT_FALSE(ParseHotkey("Ctrl+F0", spec));
    EXPECT_FALSE(ParseHotkey("Ctrl+F25", spec));
}

TEST(ParseHotkey, KeyOrderDoesNotMatter) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey("H+Alt+Ctrl", spec));
    EXPECT_EQ(spec.modifiers, (unsigned)(MOD_CONTROL | MOD_ALT));
    EXPECT_EQ(spec.vk, (unsigned)'H');
}

TEST(ParseHotkey, RefusesBareKeyAndBareModifiers) {
    HotkeySpec spec;
    EXPECT_FALSE(ParseHotkey("H", spec));
    EXPECT_FALSE(ParseHotkey("F9", spec));
    EXPECT_FALSE(ParseHotkey("Ctrl+Alt", spec));
}

TEST(ParseHotkey, RefusesMalformedInput) {
    HotkeySpec spec;
    EXPECT_FALSE(ParseHotkey("", spec));
    EXPECT_FALSE(ParseHotkey("   ", spec));
    EXPECT_FALSE(ParseHotkey("Ctrl+", spec));
    EXPECT_FALSE(ParseHotkey("Ctrl++H", spec));
    EXPECT_FALSE(ParseHotkey("Ctrl+H+J", spec));
    EXPECT_FALSE(ParseHotkey("Ctrl+Escape", spec));
    EXPECT_FALSE(ParseHotkey("Ctrl+Fx", spec));
}

TEST(ParseHotkey, FailureLeavesOutputUntouched) {
    HotkeySpec spec;
    spec.modifiers = 99;
    spec.vk = 77;
    EXPECT_FALSE(ParseHotkey("nonsense", spec));
    EXPECT_EQ(spec.modifiers, 99u);
    EXPECT_EQ(spec.vk, 77u);
}
