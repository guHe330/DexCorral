/**
 * test_recently_left.cpp - Unit tests for RecentlyLeft.
 *
 * The memory that lets a desktop file that was replaced (deleted and written
 * again, or renamed away and back) return to the corral slot it came from.
 */

#include <gtest/gtest.h>
#include "RecentlyLeft.h"

TEST(RecentlyLeft, GivesBackTheSlotOnce)
{
    RecentlyLeft memo;
    int corral = 0;
    memo.Remember("a.lnk", &corral, 2, 5, 1000);

    LeftSlot slot;
    ASSERT_TRUE(memo.Take("a.lnk", 1500, slot));
    EXPECT_EQ(slot.corral, &corral);
    EXPECT_EQ(slot.tabIndex, 2);
    EXPECT_EQ(slot.index, 5u);

    EXPECT_FALSE(memo.Take("a.lnk", 1500, slot)); // taken, so gone
}

TEST(RecentlyLeft, UnknownNameIsNotFound)
{
    RecentlyLeft memo;
    int corral = 0;
    memo.Remember("a.lnk", &corral, 0, 0, 1000);

    LeftSlot slot;
    EXPECT_FALSE(memo.Take("b.lnk", 1000, slot));
    EXPECT_FALSE(memo.Take("A.LNK", 1000, slot)); // exact names, like tab.Files
}

TEST(RecentlyLeft, ExpiresAfterTheWindow)
{
    RecentlyLeft memo;
    int corral = 0;
    memo.Remember("a.lnk", &corral, 0, 0, 1000);

    LeftSlot slot;
    EXPECT_FALSE(memo.Take("a.lnk", 1000 + RecentlyLeft::kWindowMs + 1, slot));
    EXPECT_EQ(memo.Size(), 0u);
}

TEST(RecentlyLeft, StillValidAtTheEdgeOfTheWindow)
{
    RecentlyLeft memo;
    int corral = 0;
    memo.Remember("a.lnk", &corral, 0, 0, 1000);

    LeftSlot slot;
    EXPECT_TRUE(memo.Take("a.lnk", 1000 + RecentlyLeft::kWindowMs, slot));
}

TEST(RecentlyLeft, SurvivesTickWraparound)
{
    RecentlyLeft memo;
    int corral = 0;
    unsigned long nearWrap = 0xFFFFFF00ul;
    memo.Remember("a.lnk", &corral, 1, 3, nearWrap);

    LeftSlot slot;
    EXPECT_TRUE(memo.Take("a.lnk", nearWrap + 600, slot)); // counter wrapped past zero
    EXPECT_EQ(slot.index, 3u);
}

TEST(RecentlyLeft, LaterRememberReplacesEarlier)
{
    RecentlyLeft memo;
    int first = 0, second = 0;
    memo.Remember("a.lnk", &first, 0, 1, 1000);
    memo.Remember("a.lnk", &second, 4, 9, 1200);

    LeftSlot slot;
    ASSERT_TRUE(memo.Take("a.lnk", 1300, slot));
    EXPECT_EQ(slot.corral, &second);
    EXPECT_EQ(slot.tabIndex, 4);
}

TEST(RecentlyLeft, OldEntriesAreDroppedWhenNewOnesArrive)
{
    RecentlyLeft memo;
    int corral = 0;
    memo.Remember("old.lnk", &corral, 0, 0, 1000);
    memo.Remember("new.lnk", &corral, 0, 0, 1000 + RecentlyLeft::kWindowMs + 500);
    EXPECT_EQ(memo.Size(), 1u);
}