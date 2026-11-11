/**
 * test_icon_stamp.cpp - Unit tests for IconStamp.
 *
 * Decides whether a virtual Corral may keep a loaded icon across a reload.
 */

#include <gtest/gtest.h>
#include "IconStamp.h"

static IconStamp Stamp()
{
    IconStamp s;
    s.writeTime = {100, 200};
    s.size = 1234;
    s.attributes = FILE_ATTRIBUTE_ARCHIVE;
    s.iconSize = 48;
    return s;
}

TEST(IconStamp, UnchangedFileIsEqual)
{
    EXPECT_TRUE(Stamp() == Stamp());
}

TEST(IconStamp, NeverLoadedEqualsNothing)
{
    EXPECT_FALSE(IconStamp() == IconStamp());
}

TEST(IconStamp, AnyDifferenceForcesAReload)
{
    IconStamp a = Stamp();
    a.writeTime.dwLowDateTime++;
    EXPECT_FALSE(a == Stamp());

    a = Stamp();
    a.size++;
    EXPECT_FALSE(a == Stamp());

    a = Stamp();
    a.attributes |= FILE_ATTRIBUTE_PINNED; // cloud state changed
    EXPECT_FALSE(a == Stamp());

    a = Stamp();
    a.iconSize = 32; // view mode changed
    EXPECT_FALSE(a == Stamp());

    a = Stamp();
    a.details = true;
    EXPECT_FALSE(a == Stamp());
}
