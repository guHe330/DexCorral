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

// --- Audit #183 -------------------------------------------------------------

TEST(IsValidNewFileName, PlainNamesAreFine) {
    EXPECT_TRUE(IsValidNewFileName(L"Report 2026.pdf"));
    EXPECT_TRUE(IsValidNewFileName(L"..hidden"));
    EXPECT_TRUE(IsValidNewFileName(L"a.b.c"));
}

TEST(IsValidNewFileName, ANameMustNotLeaveItsFolder) {
    EXPECT_FALSE(IsValidNewFileName(L"..\\x"));
    EXPECT_FALSE(IsValidNewFileName(L"sub\\x"));
    EXPECT_FALSE(IsValidNewFileName(L"sub/x"));
    EXPECT_FALSE(IsValidNewFileName(L"C:x"));
    EXPECT_FALSE(IsValidNewFileName(L"."));
    EXPECT_FALSE(IsValidNewFileName(L".."));
}

TEST(IsValidNewFileName, CharactersWindowsRefuses) {
    for (const wchar_t *name : {L"a*b", L"a?b", L"a\"b", L"a<b", L"a>b", L"a|b", L"a\tb", L""})
        EXPECT_FALSE(IsValidNewFileName(name)) << "name: " << (name[0] ? "(see list)" : "(empty)");
}

TEST(IsValidNewFileName, ATrailingDotOrSpaceIsRefused) {
    // Windows would save "notes." as "notes", out of step with the config
    EXPECT_FALSE(IsValidNewFileName(L"notes."));
    EXPECT_FALSE(IsValidNewFileName(L"notes "));
    EXPECT_FALSE(IsValidNewFileName(L"notes.txt..."));
    EXPECT_TRUE(IsValidNewFileName(L".notes"));
}

TEST(NameWithHiddenExtension, ReappendsTheExtensionTheUserCouldNotSee) {
    // "Chrome.lnk" is shown as "Chrome"; typing "Browser" must keep it a shortcut
    EXPECT_EQ(NameWithHiddenExtension(L"Chrome.lnk", L"Chrome", L"Browser"), L"Browser.lnk");
    // Extensions hidden for known types
    EXPECT_EQ(NameWithHiddenExtension(L"notes.txt", L"notes", L"todo"), L"todo.txt");
}

TEST(NameWithHiddenExtension, DoesNotDoubleAnExtensionTheUserTyped) {
    EXPECT_EQ(NameWithHiddenExtension(L"notes.txt", L"notes", L"todo.TXT"), L"todo.TXT");
}

TEST(NameWithHiddenExtension, LeavesTheNameAloneWhenTheExtensionWasShown) {
    EXPECT_EQ(NameWithHiddenExtension(L"notes.txt", L"notes.txt", L"todo.md"), L"todo.md");
    EXPECT_EQ(NameWithHiddenExtension(L"Folder", L"Folder", L"Other"), L"Other");
    // A display name that is not the start of the file name tells nothing about an extension
    EXPECT_EQ(NameWithHiddenExtension(L"desktop.ini", L"Settings", L"x"), L"x");
}

TEST(ContainsPath, ComparesWithoutCase) {
    const std::vector<std::wstring> paths = {L"C:\\Stuff\\tool.exe", L"C:\\Stuff\\Folder"};
    EXPECT_TRUE(ContainsPath(paths, L"c:\\stuff\\TOOL.EXE"));
    EXPECT_FALSE(ContainsPath(paths, L"C:\\Stuff\\other.exe"));
    EXPECT_FALSE(ContainsPath({}, L"C:\\Stuff\\tool.exe"));
}

// --- Audit #185: one UTF-8 conversion for the whole program -------------------

TEST(Utf8, RoundTripsTextOutsideAscii) {
    const std::wstring wide = L"Grüße ☃ \U0001F600.lnk"; // umlauts, a snowman, an emoji (surrogate pair)
    const std::string utf8 = WideToUtf8(wide);
    EXPECT_EQ(utf8, "Gr\xC3\xBC\xC3\x9F" "e \xE2\x98\x83 \xF0\x9F\x98\x80.lnk");
    EXPECT_EQ(Utf8ToWide(utf8), wide);
}

TEST(Utf8, EmptyStaysEmpty) {
    EXPECT_TRUE(Utf8ToWide("").empty());
    EXPECT_TRUE(WideToUtf8(L"").empty());
}

TEST(Utf8, TheResultHasNoTrailingNul) {
    EXPECT_EQ(Utf8ToWide("abc").size(), 3u);
    EXPECT_EQ(WideToUtf8(L"abc").size(), 3u);
}
