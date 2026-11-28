/**
 * test_desktop_sort.cpp - Unit tests for DesktopSort.
 *
 * Maps Explorer's desktop sort column to the hook's sort modes, and reads
 * back the stored last sort mode.
 */

#include <initguid.h> // Define the PKEY_* constants in this TU
#include <gtest/gtest.h>
#include "DesktopSort.h"

TEST(DesktopSort, TheFourDesktopColumnsMapToTheirModes)
{
    EXPECT_EQ(SortModeForKey(PKEY_ItemNameDisplay), DesktopSortMode::Name);
    EXPECT_EQ(SortModeForKey(PKEY_Size), DesktopSortMode::Size);
    EXPECT_EQ(SortModeForKey(PKEY_ItemTypeText), DesktopSortMode::Type);
    EXPECT_EQ(SortModeForKey(PKEY_DateModified), DesktopSortMode::Date);
}

TEST(DesktopSort, OtherColumnsAreNotADesktopSort)
{
    EXPECT_EQ(SortModeForKey(PKEY_Null), DesktopSortMode::PositionOrder);
    EXPECT_EQ(SortModeForKey(PKEY_DateCreated), DesktopSortMode::PositionOrder);
    // Same property set as ItemNameDisplay, other property id
    PROPERTYKEY nearName = PKEY_ItemNameDisplay;
    nearName.pid++;
    EXPECT_EQ(SortModeForKey(nearName), DesktopSortMode::PositionOrder);
}

TEST(DesktopSort, ColumnOrDirectionChangeCounts)
{
    SORTCOLUMN name = { PKEY_ItemNameDisplay, SORT_ASCENDING };
    SORTCOLUMN same = name;
    EXPECT_FALSE(SortColumnChanged(name, same));

    SORTCOLUMN size = { PKEY_Size, SORT_ASCENDING };
    EXPECT_TRUE(SortColumnChanged(name, size));

    SORTCOLUMN reversed = { PKEY_ItemNameDisplay, SORT_DESCENDING };
    EXPECT_TRUE(SortColumnChanged(name, reversed));

    SORTCOLUMN none = {}; // No sort column before the first Sort by
    EXPECT_TRUE(SortColumnChanged(none, name));
}

TEST(DesktopSort, StoredModeRoundTrips)
{
    for (DesktopSortMode m : { DesktopSortMode::PositionOrder, DesktopSortMode::Name, DesktopSortMode::Size,
                               DesktopSortMode::Type, DesktopSortMode::Date })
        EXPECT_EQ(SortModeFromStored((DWORD)m), m);
}

TEST(DesktopSort, UnknownStoredModeIsPositionOrder)
{
    EXPECT_EQ(SortModeFromStored(5), DesktopSortMode::PositionOrder);
    EXPECT_EQ(SortModeFromStored(0xFFFFFFFF), DesktopSortMode::PositionOrder);
}
