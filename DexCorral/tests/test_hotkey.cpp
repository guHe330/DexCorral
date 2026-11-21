/**
 * test_hotkey.cpp - Unit tests for ParseHotkey.
 *
 * Covers the accepted spellings, and the inputs that must be refused so a bad
 * config.json value can never register a hotkey that swallows a plain key.
 */

#include <gtest/gtest.h>
#include "Hotkey.h"
#include <Windows.h>

TEST(ParseHotkey, DocumentedExample) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey("Ctrl+Alt+H", spec));
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

// ---------------------------------------------------------------------------
// FormatHotkey: the form shown next to the menu entry
// ---------------------------------------------------------------------------

namespace {
std::wstring Shown(const char* config, const wchar_t* ctrl = L"Ctrl", const wchar_t* shift = L"Shift") {
    HotkeySpec spec;
    if (!ParseHotkey(config, spec))
        return L"<parse failed>";
    return FormatHotkey(spec, ctrl, shift);
}
} // namespace

TEST(FormatHotkey, EnglishAndGermanKeyNames) {
    EXPECT_EQ(Shown("Ctrl+Alt+H"), L"Ctrl+Alt+H");
    EXPECT_EQ(Shown("Ctrl+Alt+H", L"Strg", L"Umschalt"), L"Strg+Alt+H");
    EXPECT_EQ(Shown("Ctrl+Shift+F9", L"Strg", L"Umschalt"), L"Strg+Umschalt+F9");
}

TEST(FormatHotkey, KeyIsShownAsOnTheKeycap) {
    // However the config spells it: a capital letter names the key
    EXPECT_EQ(Shown("ctrl + alt + h"), L"Ctrl+Alt+H");
    EXPECT_EQ(Shown("Win+Alt+0"), L"Alt+Win+0");
    EXPECT_EQ(Shown("Ctrl+F24"), L"Ctrl+F24");
}

TEST(FormatHotkey, ModifierOrderIsFixed) {
    EXPECT_EQ(Shown("H+Win+Shift+Alt+Ctrl"), L"Ctrl+Alt+Shift+Win+H");
    EXPECT_EQ(Shown("Shift+Ctrl+D"), L"Ctrl+Shift+D");
}

TEST(FormatHotkey, SpecWithoutAKnownKey_IsEmpty) {
    HotkeySpec spec;
    EXPECT_EQ(FormatHotkey(spec, L"Ctrl", L"Shift"), L"");
    spec.modifiers = MOD_CONTROL;
    spec.vk = VK_ESCAPE;
    EXPECT_EQ(FormatHotkey(spec, L"Ctrl", L"Shift"), L"");
}

// The settings window: spec -> hot key control -> spec -> config text -> spec
TEST(Hotkey, ControlWordRoundTrip) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey("Ctrl+Alt+Shift+F9", spec));
    const unsigned word = HotkeyToControlWord(spec);
    EXPECT_EQ(word, (0x07u << 8) | VK_F9);

    const HotkeySpec back = HotkeyFromControlWord(word);
    EXPECT_EQ(back.modifiers, spec.modifiers);
    EXPECT_EQ(back.vk, spec.vk);

    const std::wstring text = FormatHotkey(back, L"Ctrl", L"Shift");
    std::string narrow;
    for (wchar_t c : text)
        narrow += (char)c;
    HotkeySpec reparsed;
    ASSERT_TRUE(ParseHotkey(narrow, reparsed));
    EXPECT_EQ(reparsed.modifiers, spec.modifiers);
    EXPECT_EQ(reparsed.vk, spec.vk);
}

TEST(Hotkey, ControlWordDropsWin_AndEmptyStaysEmpty) {
    HotkeySpec spec;
    ASSERT_TRUE(ParseHotkey("Win+Alt+D", spec));
    EXPECT_EQ(HotkeyFromControlWord(HotkeyToControlWord(spec)).modifiers, (unsigned)MOD_ALT);
    EXPECT_EQ(HotkeyToControlWord(HotkeySpec{}), 0u);
    EXPECT_EQ(HotkeyFromControlWord(0).vk, 0u);
}

// --- Audit #184 -------------------------------------------------------------

TEST(ParseHotkey, ShiftAloneIsNotEnoughForAKeyThatTypes) {
    // RegisterHotKey would take Shift+A, and then no program gets a capital A
    HotkeySpec spec;
    EXPECT_FALSE(ParseHotkey("Shift+A", spec));
    EXPECT_FALSE(ParseHotkey("Shift+1", spec));
    EXPECT_TRUE(ParseHotkey("Shift+F9", spec)); // A function key types nothing
    EXPECT_TRUE(ParseHotkey("Ctrl+Shift+A", spec));
    EXPECT_TRUE(ParseHotkey("Alt+Shift+A", spec));
    EXPECT_TRUE(ParseHotkey("Win+Shift+A", spec));
}
