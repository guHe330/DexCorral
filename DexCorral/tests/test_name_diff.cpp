/**
 * test_name_diff.cpp - Unit tests for DiffNames.
 *
 * The comparison a folder watch uses to recover changes it did not receive.
 */

#include <gtest/gtest.h>
#include "NameDiff.h"

TEST(NameDiff, ReportsAddedAndRemoved)
{
    NameSet before{L"a.txt", L"b.lnk", L"c"};
    NameSet after{L"b.lnk", L"c", L"d.url"};
    std::vector<std::wstring> added, removed;
    DiffNames(before, after, added, removed);
    ASSERT_EQ(added.size(), 1u);
    EXPECT_EQ(added[0], L"d.url");
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], L"a.txt");
}

TEST(NameDiff, SameListingIsNoChange)
{
    NameSet names{L"a", L"b"};
    std::vector<std::wstring> added, removed;
    DiffNames(names, names, added, removed);
    EXPECT_TRUE(added.empty());
    EXPECT_TRUE(removed.empty());
}

TEST(NameDiff, CaseAloneIsNoChange)
{
    NameSet before{L"Report.DOCX"};
    NameSet after{L"report.docx"};
    std::vector<std::wstring> added, removed;
    DiffNames(before, after, added, removed);
    EXPECT_TRUE(added.empty());
    EXPECT_TRUE(removed.empty());
}

TEST(NameDiff, EmptyBeforeReportsEverythingAsAdded)
{
    NameSet after{L"x", L"y"};
    std::vector<std::wstring> added, removed;
    DiffNames(NameSet(), after, added, removed);
    EXPECT_EQ(added.size(), 2u);
    EXPECT_TRUE(removed.empty());
}
