#include <gtest/gtest.h>

#include "PendingAction.h"

// What the user asked for on a network tab waits for the share to answer.

TEST(PendingAction, RunsWhenTheAnswerComesInTime) {
    PendingAction pending;
    int runs = 0;
    pending.Set([&runs] { runs++; }, 1000);
    auto action = pending.Take(1500, 5000);
    ASSERT_TRUE(action);
    action();
    EXPECT_EQ(runs, 1);
}

TEST(PendingAction, IsHandedOutOnlyOnce) {
    PendingAction pending;
    pending.Set([] {}, 1000);
    EXPECT_TRUE(pending.Take(1100, 5000));
    EXPECT_FALSE(pending.Take(1200, 5000));
}

TEST(PendingAction, IsDroppedAfterWaitingTooLong) {
    PendingAction pending;
    pending.Set([] {}, 1000);
    EXPECT_FALSE(pending.Take(6001, 5000)); // The share answered 5.001 s later
    EXPECT_FALSE(pending.Take(1100, 5000)); // And it is gone, not kept for the next answer
}

TEST(PendingAction, TheLatestRequestReplacesTheOlder) {
    PendingAction pending;
    int which = 0;
    pending.Set([&which] { which = 1; }, 1000);
    pending.Set([&which] { which = 2; }, 1200);
    pending.Take(1300, 5000)();
    EXPECT_EQ(which, 2);
}

TEST(PendingAction, NothingAskedGivesNothing) {
    PendingAction pending;
    EXPECT_FALSE(pending.Take(1000, 5000));
}
