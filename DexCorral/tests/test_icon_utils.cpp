/**
 * test_icon_utils.cpp - Unit tests for IconUtils pure string helpers.
 *
 * Tests IsSpecialIconEntry, GetSpecialIconClsid, and StripLnkExtension
 * without any Win32 window / shell API calls.
 */

#include <gtest/gtest.h>
#include "IconUtils.h"

using namespace IconUtils;

// ---------------------------------------------------------------------------
// IsSpecialIconEntry
// ---------------------------------------------------------------------------

TEST(IsSpecialIconEntry, ShellPrefix_ReturnsTrue) {
    EXPECT_TRUE(IsSpecialIconEntry("shell:{645FF040-5081-101B-9F08-00AA002F954E}"));
}

TEST(IsSpecialIconEntry, ShellPrefixMinimal_ReturnsTrue) {
    EXPECT_TRUE(IsSpecialIconEntry("shell:X"));  // length 7, > 6
}

TEST(IsSpecialIconEntry, RegularFile_ReturnsFalse) {
    EXPECT_FALSE(IsSpecialIconEntry("notepad.lnk"));
    EXPECT_FALSE(IsSpecialIconEntry("Recycle Bin"));
}

TEST(IsSpecialIconEntry, EmptyString_ReturnsFalse) {
    EXPECT_FALSE(IsSpecialIconEntry(""));
}

TEST(IsSpecialIconEntry, ExactlyShellColon_ReturnsFalse) {
    // length == 6, not > 6
    EXPECT_FALSE(IsSpecialIconEntry("shell:"));
}

TEST(IsSpecialIconEntry, ShellInMiddle_ReturnsFalse) {
    EXPECT_FALSE(IsSpecialIconEntry("my_shell:{GUID}"));
}

// ---------------------------------------------------------------------------
// GetSpecialIconClsid
// ---------------------------------------------------------------------------

TEST(GetSpecialIconClsid, BasicCase) {
    auto clsid = GetSpecialIconClsid("shell:{1234}");
    EXPECT_EQ(clsid, L"{1234}");
}

TEST(GetSpecialIconClsid, RecycleBinGuid) {
    const std::string entry = "shell:{645FF040-5081-101B-9F08-00AA002F954E}";
    auto clsid = GetSpecialIconClsid(entry);
    EXPECT_EQ(clsid, L"{645FF040-5081-101B-9F08-00AA002F954E}");
}

TEST(GetSpecialIconClsid, EmptySuffix) {
    // "shell:" -> suffix is empty (caller ensures IsSpecialIconEntry was true first)
    auto clsid = GetSpecialIconClsid("shell:");
    EXPECT_EQ(clsid, L"");
}

// ---------------------------------------------------------------------------
// StripLnkExtension
// ---------------------------------------------------------------------------

TEST(StripLnkExtension, WithLnkLowercase) {
    EXPECT_EQ(StripLnkExtension(L"Notepad.lnk"), L"Notepad");
}

TEST(StripLnkExtension, WithLnkUppercase) {
    EXPECT_EQ(StripLnkExtension(L"Notepad.LNK"), L"Notepad");
}

TEST(StripLnkExtension, WithLnkMixedCase) {
    EXPECT_EQ(StripLnkExtension(L"Notepad.Lnk"), L"Notepad");
}

TEST(StripLnkExtension, NotLnkExtension_Unchanged) {
    EXPECT_EQ(StripLnkExtension(L"readme.txt"), L"readme.txt");
    EXPECT_EQ(StripLnkExtension(L"archive.zip"), L"archive.zip");
}

TEST(StripLnkExtension, TooShortToStrip) {
    // ".lnk" itself is exactly 4 chars, length not > 4
    EXPECT_EQ(StripLnkExtension(L".lnk"), L".lnk");
    EXPECT_EQ(StripLnkExtension(L""), L"");
}

TEST(StripLnkExtension, NoExtension_Unchanged) {
    EXPECT_EQ(StripLnkExtension(L"Recycle Bin"), L"Recycle Bin");
}
