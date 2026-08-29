/**
 * test_strings.cpp - Unit tests for the UI string catalog (Strings.h/.cpp).
 *
 * Verifies that every Str enum value resolves to a non-null, non-empty English
 * string and that TrFmt performs literal {0} token replacement (no printf).
 * See docs/TRANSLATION_PLAN.md §7.
 */

#include <gtest/gtest.h>
#include "Strings.h"

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
    // Every {0} in an English string must appear the same number of times in
    // the German one (and vice versa) so TrFmt substitutions never get lost.
    auto countToken = [](const std::wstring &s, const wchar_t *tok) {
        size_t n = 0, pos = 0;
        while ((pos = s.find(tok, pos)) != std::wstring::npos) { n++; pos += 3; }
        return n;
    };
    for (size_t i = 0; i < (size_t)Str::_Count; i++) {
        SetLanguage(L"en");
        size_t en = countToken(Tr((Str)i), L"{0}") + countToken(Tr((Str)i), L"{1}");
        SetLanguage(L"de");
        size_t de = countToken(Tr((Str)i), L"{0}") + countToken(Tr((Str)i), L"{1}");
        EXPECT_EQ(en, de) << "placeholder count mismatch for Str id " << i;
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
