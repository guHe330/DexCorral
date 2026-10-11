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

/**
 * CrashGuard.h - The rules that decide when DexCorral stops trying
 *
 * DexCorral's hook runs inside explorer.exe, so a fault in it takes the desktop
 * down, Windows restarts Explorer, and the hook loads again. Left alone that is
 * a crash loop. The guard watches for it and puts DexCorral into standby.
 *
 * This file holds only the decision, as pure functions over a small state
 * struct, so the unit tests can walk through every sequence of starts and
 * crashes. Reading and writing the state (the registry) and acting on the
 * verdict are the hook's job, in CorralHook.cpp.
 *
 * How a crash is recognised: `pending` is set just before the hook is installed
 * and cleared once Explorer has survived a stability window, or shuts down
 * cleanly. A start that finds `pending` still set therefore follows a session
 * in which Explorer died with the hook freshly installed.
 */

#pragma once
#include <string>

// Where the state lives: HKCU, because it is written from inside Explorer
// before config.json is available. Technical names, not UI strings.
constexpr const wchar_t *CRASH_GUARD_REGKEY = L"Software\\DexCorral";
constexpr const wchar_t *CRASH_GUARD_REGVAL_PENDING = L"HookStartPending";       // REG_DWORD
constexpr const wchar_t *CRASH_GUARD_REGVAL_LAST_FAILURE = L"GuardLastFailure"; // REG_QWORD, seconds (FILETIME / 10^7)
constexpr const wchar_t *CRASH_GUARD_REGVAL_STANDBY = L"GuardStandby";           // REG_DWORD
constexpr const wchar_t *CRASH_GUARD_REGVAL_VERSION = L"GuardVersion";           // REG_SZ
constexpr const wchar_t *CRASH_GUARD_REGVAL_LEGACY_COUNT = L"HookFailureCount";  // pre-guard counter, removed on sight

/// Two failures no further apart than this put DexCorral into standby
constexpr unsigned long long CRASH_GUARD_WINDOW_SECONDS = 120;

/// What the guard remembers between Explorer sessions
struct CrashGuardState
{
    bool pending = false;                   /// The hook was installed and not yet confirmed stable
    unsigned long long lastFailureTime = 0; /// When the last failure was noticed (seconds, any fixed epoch); 0 = none
    bool standby = false;                   /// Standby is in force until the user resumes
    std::wstring version;                   /// DexCorral version that recorded the failure / standby
};

/// The guard's verdict for the session that is starting
struct CrashGuardVerdict
{
    CrashGuardState next;      /// State to store; `pending` is always false here (the caller arms it)
    bool standby = false;      /// Do not install the hook this session
    bool enteredNow = false;   /// Standby began with this very start (as opposed to carried over)
    bool failureNoted = false; /// The previous session counted as a failure
};

/**
 * Decides, at the start of a session, whether the hook may be installed.
 *
 * - Anything recorded by a different DexCorral version is forgotten: a new
 *   build gets a fresh attempt.
 * - Standby already in force stays in force. Only ResumeCrashGuard ends it.
 * - A previous session that ended with `pending` set is a failure. Two failures
 *   within CRASH_GUARD_WINDOW_SECONDS enter standby; a lone one is remembered
 *   and, unless a second follows in time, simply expires.
 */
CrashGuardVerdict EvaluateCrashGuard(const CrashGuardState &state, unsigned long long now,
                                     const std::wstring &currentVersion);

/// State after Explorer has survived the stability window or shut down cleanly:
/// the session was fine, so an earlier lone failure is forgotten. Standby is untouched.
CrashGuardState CrashGuardAfterStableSession(const CrashGuardState &state);

/// State after the user chose to resume: a clean slate
CrashGuardState ResumeCrashGuard();
