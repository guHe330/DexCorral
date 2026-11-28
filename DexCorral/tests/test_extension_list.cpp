/**
 * test_extension_list.cpp - Unit tests for ExtensionList.
 *
 * How a tab's list of extensions is read, shown, and matched against new files.
 */

#include <gtest/gtest.h>
#include "ExtensionList.h"

using Exts = std::vector<std::wstring>;

TEST(ExtensionList, AcceptsEveryTypedForm)
{
    EXPECT_EQ(ExtensionList::Parse(L"*.PDF, .docx xlsx;txt"), (Exts{L"pdf", L"docx", L"xlsx", L"txt"}));
}

TEST(ExtensionList, IgnoresEmptyEntriesAndBlanks)
{
    EXPECT_EQ(ExtensionList::Parse(L"  ;; pdf ,\t, ; "), (Exts{L"pdf"}));
    EXPECT_TRUE(ExtensionList::Parse(L"").empty());
    EXPECT_TRUE(ExtensionList::Parse(L" ; , ").empty());
}

TEST(ExtensionList, DropsRepeatsInAnyCase)
{
    EXPECT_EQ(ExtensionList::Parse(L"pdf; PDF; *.Pdf; docx"), (Exts{L"pdf", L"docx"}));
}

TEST(ExtensionList, DropsWhatCannotBeAnExtension)
{
    // Patterns, a dot inside, characters a file name cannot have, shortcuts
    EXPECT_EQ(ExtensionList::Parse(L"Invoice*.pdf; tar.gz; a?b; c|d; *; .; lnk; *.LNK; zip"), (Exts{L"zip"}));
}

TEST(ExtensionList, JoinsAsStoredAndShown)
{
    EXPECT_EQ(ExtensionList::Join({L"pdf", L"docx", L"xlsx"}), L"pdf; docx; xlsx");
    EXPECT_EQ(ExtensionList::Join({}), L"");
    EXPECT_EQ(ExtensionList::Join(ExtensionList::Parse(L"*.PDF,.docx")), L"pdf; docx");
}

TEST(ExtensionList, RemovesTheGivenEntries)
{
    Exts list{L"pdf", L"docx", L"txt"};
    ExtensionList::Remove(list, {L"txt", L"pdf", L"zip"});
    EXPECT_EQ(list, (Exts{L"docx"}));
}

TEST(ExtensionList, ExtensionOfAFile)
{
    EXPECT_EQ(ExtensionList::OfFile(L"Invoice 2026.PDF"), L"pdf");
    EXPECT_EQ(ExtensionList::OfFile(L"archive.tar.gz"), L"gz");
    EXPECT_EQ(ExtensionList::OfFile(L"README"), L"");
    EXPECT_EQ(ExtensionList::OfFile(L"name."), L"");
    EXPECT_EQ(ExtensionList::OfFile(L"folder.d\\file"), L"");
}

TEST(ExtensionList, ShortcutsHaveNoExtensionToRouteBy)
{
    EXPECT_EQ(ExtensionList::OfFile(L"Report.pdf.lnk"), L"");
    EXPECT_EQ(ExtensionList::OfFile(L"Editor.LNK"), L"");
}
