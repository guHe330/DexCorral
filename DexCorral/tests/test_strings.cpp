/**
 * test_strings.cpp - Unit tests for the UI string catalog (Strings.h/.cpp).
 *
 * Verifies that every Str enum value resolves to a non-null, non-empty string in
 * every shipped language, that placeholders stay in parity across languages, and
 * that TrFmt performs literal {0}/{1} token replacement (no printf).
 */

#include <gtest/gtest.h>
#include "Strings.h"
#include <cwchar>
#include <cwctype>
#include <vector>

TEST(StringCatalog, EveryIdResolvesToNonEmptyEnglish) {
    for (size_t i = 0; i < (size_t)Str::_Count; i++) {
        const wchar_t *s = Tr((Str)i);
        ASSERT_NE(s, nullptr) << "Str id " << i << " returned null";
        EXPECT_NE(s[0], L'\0') << "Str id " << i << " is empty";
    }
}

TEST(StringCatalog, OutOfRangeIdFallsBackToEmptyNotNull) {
    const wchar_t *s = Tr(Str::_Count);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s[0], L'\0');
}

TEST(StringCatalog, TrFmtReplacesToken) {
    // Title_Appearance is "Appearance: {0}"
    std::wstring result = TrFmt(Str::Title_Appearance, L"My Corral");
    EXPECT_EQ(result, L"Appearance: My Corral");
}

TEST(StringCatalog, TrFmtLeavesStringsWithoutTokenUntouched) {
    std::wstring result = TrFmt(Str::Btn_OK, L"ignored");
    EXPECT_EQ(result, Tr(Str::Btn_OK));
}

// ---------------------------------------------------------------------------
// Language switching (German)
// ---------------------------------------------------------------------------

// Restores English after each test so test order doesn't matter.
class GermanCatalog : public ::testing::Test {
protected:
    void TearDown() override { SetLanguage(L"en"); }
};

TEST_F(GermanCatalog, EveryIdResolvesToNonEmptyGerman) {
    SetLanguage(L"de");
    for (size_t i = 0; i < (size_t)Str::_Count; i++) {
        const wchar_t *s = Tr((Str)i);
        ASSERT_NE(s, nullptr) << "Str id " << i << " returned null";
        EXPECT_NE(s[0], L'\0') << "Str id " << i << " is empty in German";
    }
}

TEST_F(GermanCatalog, PlaceholderParityWithEnglish) {
    // Every {0} and every {1} in an English string must appear the same number
    // of times in the German one (and vice versa) so TrFmt substitutions never
    // get lost. Counted apart: a sum would let "{0}..{0}" pass for "{0}..{1}".
    auto countToken = [](const std::wstring &s, const wchar_t *tok) {
        size_t n = 0, pos = 0;
        while ((pos = s.find(tok, pos)) != std::wstring::npos) { n++; pos += 3; }
        return n;
    };
    for (size_t i = 0; i < (size_t)Str::_Count; i++) {
        for (const wchar_t *tok : {L"{0}", L"{1}"}) {
            SetLanguage(L"en");
            size_t en = countToken(Tr((Str)i), tok);
            SetLanguage(L"de");
            size_t de = countToken(Tr((Str)i), tok);
            EXPECT_EQ(en, de) << "placeholder count mismatch for " << tok << " in Str id " << i;
        }
    }
}

TEST_F(GermanCatalog, SwitchAndFallback) {
    SetLanguage(L"de");
    EXPECT_STREQ(Tr(Str::Btn_Cancel), L"Abbrechen");
    SetLanguage(L"en");
    EXPECT_STREQ(Tr(Str::Btn_Cancel), L"Cancel");
    // Unknown / empty codes select English
    SetLanguage(L"fr");
    EXPECT_STREQ(Tr(Str::Btn_Cancel), L"Cancel");
    SetLanguage(L"");
    EXPECT_STREQ(Tr(Str::Btn_Cancel), L"Cancel");
}

TEST_F(GermanCatalog, TrFmtUsesActiveLanguage) {
    SetLanguage(L"de");
    EXPECT_EQ(TrFmt(Str::Title_Appearance, L"Mein Corral"), L"Darstellung: Mein Corral");
}

TEST(StringCatalog, TrFmtIsLiteralNotPrintf) {
    // A hostile-looking argument must be inserted verbatim, never interpreted.
    std::wstring result = TrFmt(Str::Update_AvailableBody, L"%s%n{0}");
    EXPECT_NE(result.find(L"%s%n"), std::wstring::npos);
    // The {0} inside the *argument* must not be re-expanded (no infinite loop,
    // no double substitution) — it survives as literal text.
    EXPECT_NE(result.find(L"{0}"), std::wstring::npos);
}

TEST_F(GermanCatalog, TrFmtTwoArgsSubstitutesBothTokens) {
    SetLanguage(L"en");
    std::wstring r = TrFmt(Str::Reg_NeedsWin11, L"22000", L"19045");
    EXPECT_NE(r.find(L"22000"), std::wstring::npos);
    EXPECT_NE(r.find(L"19045"), std::wstring::npos);
    EXPECT_EQ(r.find(L"{0}"), std::wstring::npos);
    EXPECT_EQ(r.find(L"{1}"), std::wstring::npos);
}

TEST_F(GermanCatalog, TrFmtTwoArgsDoesNotReexpandArguments) {
    SetLanguage(L"en");
    // "{1}" arriving as arg0 must survive verbatim, not be replaced by arg1.
    std::wstring r = TrFmt(Str::Reg_NeedsWin11, L"{1}", L"19045");
    EXPECT_NE(r.find(L"{1}"), std::wstring::npos);
}

TEST_F(GermanCatalog, AppearanceAccessKeysAreUniquePerPage) {
    // The Appearance dialog shows one page at a time: the dialog manager only
    // matches visible controls, so an access key must be unique within the
    // controls always shown plus those of each page, in every language
    auto key = [](Str id) -> wchar_t {
        const wchar_t *amp = wcschr(Tr(id), L'&');
        return amp ? (wchar_t)towupper(amp[1]) : 0;
    };
    const std::vector<Str> always = {Str::Btn_SavePreset, Str::Btn_DeletePreset, Str::Lbl_Background, Str::Grp_Header,
                                     Str::Grp_Icons, Str::Chk_UseAsDefault, Str::Chk_ApplyToAll,
                                     Str::Chk_CopyStyleToAll, Str::Btn_ResetToDefaults};
    const std::vector<std::vector<Str>> pages = {
        {Str::Lbl_Color, Str::Chk_AccentColor, Str::Grp_Opacity, Str::Lbl_Border},
        {Str::Lbl_Height, Str::Grp_Opacity, Str::Lbl_Font, Str::Lbl_Color, Str::Lbl_HeaderLabel},
        {Str::Grp_Opacity, Str::Lbl_IconLabel, Str::Lbl_Tint, Str::Lbl_TintStrength, Str::Lbl_Horiz, Str::Lbl_Vert}};
    for (const wchar_t *lang : {L"en", L"de"}) {
        SetLanguage(lang);
        for (size_t page = 0; page < pages.size(); page++) {
            std::vector<Str> shown = always;
            shown.insert(shown.end(), pages[page].begin(), pages[page].end());
            std::wstring seen;
            for (Str id : shown) {
                const wchar_t k = key(id);
                ASSERT_NE(k, 0) << "no access key in Str id " << (int)id;
                EXPECT_EQ(seen.find(k), std::wstring::npos)
                    << "access key used twice on page " << page << " for Str id " << (int)id;
                seen += k;
            }
        }
    }
}

TEST_F(GermanCatalog, SettingsAccessKeysAreUnique) {
    const std::vector<Str> shown = {Str::Menu_Language, Str::Settings_Theme, Str::Settings_Look,
                                    Str::Btn_ExportPresets, Str::Btn_ImportPresets, Str::Menu_CheckUpdatesAuto,
                                    Str::Settings_Hotkey, Str::Settings_DebugLogging, Str::Menu_OpenConfigFolder};
    for (const wchar_t *lang : {L"en", L"de"}) {
        SetLanguage(lang);
        std::wstring seen;
        for (Str id : shown) {
            const wchar_t *amp = wcschr(Tr(id), L'&');
            ASSERT_NE(amp, nullptr) << "no access key in Str id " << (int)id;
            const wchar_t k = (wchar_t)towupper(amp[1]);
            EXPECT_EQ(seen.find(k), std::wstring::npos) << "access key used twice for Str id " << (int)id;
            seen += k;
        }
    }
}
