/**
 * DexCorral - a free and open source Windows desktop icon organizer
 * Copyright (C) 2026 Gunter Heiss
 *
 * For more information see: https://dexcorral.com
 * The DexCorral project is hosted on GitHub: https://github.com/guHe330/DexCorral
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "CrashGuard.h"

CrashGuardVerdict EvaluateCrashGuard(const CrashGuardState &state, unsigned long long now,
                                     const std::wstring &currentVersion)
{
    CrashGuardVerdict verdict;

    // What an earlier build recorded says nothing about this one. `pending` is
    // kept: it describes the session that just ended, whichever build ran it,
    // but with the history gone it can only ever be a first failure.
    CrashGuardState s = state;
    if (s.version != currentVersion)
    {
        s.lastFailureTime = 0;
        s.standby = false;
    }

    verdict.next = s;
    verdict.next.pending = false;
    verdict.next.version = currentVersion;

    if (s.standby)
    {
        verdict.standby = true;
        return verdict;
    }

    if (!s.pending)
        return verdict;

    verdict.failureNoted = true;

    // A clock that went backwards makes the gap meaningless: treat it as a first failure
    const bool hasEarlier = s.lastFailureTime != 0 && now >= s.lastFailureTime;
    if (hasEarlier && now - s.lastFailureTime <= CRASH_GUARD_WINDOW_SECONDS)
    {
        verdict.standby = true;
        verdict.enteredNow = true;
        verdict.next.standby = true;
    }
    verdict.next.lastFailureTime = now;
    return verdict;
}

CrashGuardState CrashGuardAfterStableSession(const CrashGuardState &state)
{
    CrashGuardState next = state;
    next.pending = false;
    next.lastFailureTime = 0;
    return next;
}

CrashGuardState ResumeCrashGuard()
{
    return CrashGuardState();
}
