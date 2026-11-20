/**
 * test_undo_move.cpp - Unit tests for UndoMove.
 *
 * Which desktop icons an undone Corral move puts back.
 */

#include <gtest/gtest.h>
#include "UndoMove.h"

static DesktopIconInfo icon(const wchar_t *name, const wchar_t *path, int x, int y)
{
    return DesktopIconInfo{name, path, POINT2D{x, y}};
}

TEST(UndoMove, RestoresIconThatWasPushedAndStillLiesThere)
{
    auto r = UndoMove::IconsToRestore({icon(L"a", L"C:\\d\\a.txt", 10, 10)},
                                      {icon(L"a", L"C:\\d\\a.txt", 160, 10)},
                                      {icon(L"a", L"C:\\d\\a.txt", 160, 10)});
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].parsingName, L"C:\\d\\a.txt");
    EXPECT_EQ(r[0].displayName, L"a");
    EXPECT_EQ(r[0].pt.x, 10);
    EXPECT_EQ(r[0].pt.y, 10);
}

TEST(UndoMove, LeavesIconThatTheMoveDidNotTouch)
{
    std::vector<DesktopIconInfo> same{icon(L"a", L"C:\\d\\a.txt", 10, 10)};
    EXPECT_TRUE(UndoMove::IconsToRestore(same, same, same).empty());
}

TEST(UndoMove, SkipsIconMovedByUserSince)
{
    auto r = UndoMove::IconsToRestore({icon(L"a", L"C:\\d\\a.txt", 10, 10)},
                                      {icon(L"a", L"C:\\d\\a.txt", 160, 10)},
                                      {icon(L"a", L"C:\\d\\a.txt", 300, 300)});
    EXPECT_TRUE(r.empty());
}

TEST(UndoMove, SkipsIconThatIsGone)
{
    auto r = UndoMove::IconsToRestore({icon(L"a", L"C:\\d\\a.txt", 10, 10)},
                                      {icon(L"a", L"C:\\d\\a.txt", 160, 10)},
                                      {});
    EXPECT_TRUE(r.empty());
}

TEST(UndoMove, SkipsIconThatAppearedDuringTheMove)
{
    auto r = UndoMove::IconsToRestore({},
                                      {icon(L"new", L"C:\\d\\new.txt", 160, 10)},
                                      {icon(L"new", L"C:\\d\\new.txt", 160, 10)});
    EXPECT_TRUE(r.empty());
}

TEST(UndoMove, NameTwinsAreToldApartByParsingName)
{
    std::vector<DesktopIconInfo> before{icon(L"x", L"C:\\u\\x.lnk", 10, 10), icon(L"x", L"C:\\p\\x.lnk", 85, 10)};
    std::vector<DesktopIconInfo> after{icon(L"x", L"C:\\u\\x.lnk", 10, 10), icon(L"x", L"C:\\p\\x.lnk", 235, 10)};
    auto r = UndoMove::IconsToRestore(before, after, after);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].parsingName, L"C:\\p\\x.lnk");
    EXPECT_EQ(r[0].pt.x, 85);
    EXPECT_EQ(r[0].pt.y, 10);
}

TEST(UndoMove, FallsBackToDisplayNameWithoutParsingName)
{
    auto r = UndoMove::IconsToRestore({icon(L"Bin", L"", 10, 10)},
                                      {icon(L"bin", L"", 160, 10)},
                                      {icon(L"BIN", L"", 160, 10)});
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].pt.x, 10);
    EXPECT_EQ(r[0].pt.y, 10);
}

TEST(UndoMove, PushCappedKeepsTheNewestRecords)
{
    std::vector<int> stack;
    for (int i = 1; i <= 12; i++)
        UndoMove::PushCapped(stack, i, 10);
    ASSERT_EQ(stack.size(), 10u);
    EXPECT_EQ(stack.front(), 3);
    EXPECT_EQ(stack.back(), 12);
}
