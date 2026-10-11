/**
 * test_crash_guard.cpp - Unit tests for the crash guard's decision rules.
 *
 * Each test walks a sequence of Explorer sessions the way the hook does:
 * evaluate at start, arm `pending`, then either confirm stability or "crash"
 * (leave `pending` set) before the next start.
 */

#include <gtest/gtest.h>
#include "CrashGuard.h"

namespace {

const std::wstring V1 = L"1.0.30";
const std::wstring V2 = L"1.0.31";

// One session start: evaluate, then arm the sentinel as the hook does when it installs.
CrashGuardVerdict Start(CrashGuardState &state, unsigned long long now, const std::wstring &version = V1) {
    CrashGuardVerdict v = EvaluateCrashGuard(state, now, version);
    state = v.next;
    if (!v.standby)
        state.pending = true; // hook installed
    return v;
}

void Survive(CrashGuardState &state) {
    state = CrashGuardAfterStableSession(state);
}

} // namespace

TEST(CrashGuard, FirstEverStart_IsNormal) {
    CrashGuardState s;
    CrashGuardVerdict v = Start(s, 1000);
    EXPECT_FALSE(v.standby);
    EXPECT_FALSE(v.failureNoted);
    EXPECT_TRUE(s.pending);
    EXPECT_EQ(s.version, V1);
}

TEST(CrashGuard, StableSessions_NeverTrip) {
    CrashGuardState s;
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(Start(s, 1000 + i * 10).standby);
        Survive(s);
    }
    EXPECT_FALSE(s.standby);
    EXPECT_EQ(s.lastFailureTime, 0u);
}

TEST(CrashGuard, OneCrash_IsNotedButDoesNotTrip) {
    CrashGuardState s;
    Start(s, 1000);                       // crashes
    CrashGuardVerdict v = Start(s, 1010); // restart after the crash
    EXPECT_TRUE(v.failureNoted);
    EXPECT_FALSE(v.standby);
    EXPECT_EQ(s.lastFailureTime, 1010u);
}

TEST(CrashGuard, TwoCrashesInsideTheWindow_EnterStandby) {
    CrashGuardState s;
    Start(s, 1000);                       // crash 1
    Start(s, 1010);                       // noted; crash 2
    CrashGuardVerdict v = Start(s, 1025); // 15 s after the first note
    EXPECT_TRUE(v.standby);
    EXPECT_TRUE(v.enteredNow);
    EXPECT_TRUE(s.standby);
    EXPECT_FALSE(s.pending) << "the hook is not installed in standby, so nothing is armed";
}

TEST(CrashGuard, WindowBoundary_IsInclusive) {
    CrashGuardState s;
    Start(s, 1000);
    Start(s, 2000);
    EXPECT_TRUE(Start(s, 2000 + CRASH_GUARD_WINDOW_SECONDS).standby);

    CrashGuardState t;
    Start(t, 1000);
    Start(t, 2000);
    EXPECT_FALSE(Start(t, 2000 + CRASH_GUARD_WINDOW_SECONDS + 1).standby);
}

TEST(CrashGuard, TwoCrashesFarApart_DoNotTrip) {
    CrashGuardState s;
    Start(s, 1000);                        // crash
    Start(s, 1010);                        // noted; crash again, but Explorer stays down for an hour
    CrashGuardVerdict v = Start(s, 4610);
    EXPECT_TRUE(v.failureNoted);
    EXPECT_FALSE(v.standby);
    EXPECT_EQ(s.lastFailureTime, 4610u) << "the newer failure replaces the expired one";
}

TEST(CrashGuard, StableSessionInBetween_ForgetsTheFirstCrash) {
    CrashGuardState s;
    Start(s, 1000); // crash
    Start(s, 1010); // noted
    Survive(s);     // this session was fine
    Start(s, 1020); // crash
    EXPECT_FALSE(Start(s, 1030).standby) << "that is a first failure again, not a second";
}

TEST(CrashGuard, Standby_IsStickyAcrossStarts) {
    CrashGuardState s;
    Start(s, 1000);
    Start(s, 1010);
    ASSERT_TRUE(Start(s, 1020).standby);

    for (unsigned long long t = 2000; t < 2000000; t += 400000) {
        CrashGuardVerdict v = Start(s, t);
        EXPECT_TRUE(v.standby);
        EXPECT_FALSE(v.enteredNow) << "carried over, not entered again";
    }
}

TEST(CrashGuard, StableCallback_DoesNotEndStandby) {
    CrashGuardState s;
    s.standby = true;
    s.version = V1;
    EXPECT_TRUE(CrashGuardAfterStableSession(s).standby);
}

TEST(CrashGuard, Resume_GivesACleanSlate) {
    CrashGuardState s;
    Start(s, 1000);
    Start(s, 1010);
    ASSERT_TRUE(Start(s, 1020).standby);

    s = ResumeCrashGuard();
    CrashGuardVerdict v = Start(s, 1030);
    EXPECT_FALSE(v.standby);
    EXPECT_FALSE(v.failureNoted);
}

TEST(CrashGuard, AfterResume_ItTakesTwoCrashesAgain) {
    CrashGuardState s = ResumeCrashGuard();
    Start(s, 1030);                        // crash
    EXPECT_FALSE(Start(s, 1040).standby);  // first failure
    EXPECT_TRUE(Start(s, 1050).standby);   // second, in the window
}

TEST(CrashGuard, NewVersion_EndsStandby) {
    CrashGuardState s;
    Start(s, 1000);
    Start(s, 1010);
    ASSERT_TRUE(Start(s, 1020).standby);

    CrashGuardVerdict v = Start(s, 1030, V2);
    EXPECT_FALSE(v.standby);
    EXPECT_EQ(s.version, V2);
    EXPECT_FALSE(s.standby);
}

TEST(CrashGuard, NewVersion_DoesNotInheritAFailure) {
    CrashGuardState s;
    Start(s, 1000);     // crash on V1
    Start(s, 1010);     // noted on V1; then the upgrade restarts Explorer uncleanly
    CrashGuardVerdict v = Start(s, 1020, V2);
    EXPECT_FALSE(v.standby) << "the V1 failure must not count against V2";
    EXPECT_TRUE(v.failureNoted);
}

TEST(CrashGuard, ClockGoingBackwards_IsTreatedAsAFirstFailure) {
    CrashGuardState s;
    Start(s, 5000);
    Start(s, 5010);
    EXPECT_FALSE(Start(s, 4000).standby);
    EXPECT_EQ(s.lastFailureTime, 4000u);
}

TEST(CrashGuard, VerdictNeverLeavesPendingSet) {
    CrashGuardState s;
    s.pending = true;
    s.version = V1;
    EXPECT_FALSE(EvaluateCrashGuard(s, 1000, V1).next.pending);
}
