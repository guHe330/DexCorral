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
 * Conflicts.h - Detection of software DexCorral cannot run next to
 *
 * One program is known: Stardock Fences. It takes over the same desktop icon
 * view DexCorral does. Observed with Fences 6.5.4.1: the icons DexCorral hides
 * become visible again and are drawn twice.
 *
 * DexCorral's answer is to step aside: Setup and --register refuse, and a
 * DexCorral that is already installed goes into standby. It never touches the
 * other program: nothing here unloads, stops, disables or modifies anything.
 *
 * Detection uses only what Windows itself exposes about an installed program:
 * the names of its modules, their version resources, and its entry in the
 * list of installed programs.
 *
 * "Stardock" and "Fences" are trademarks of Stardock. DexCorral is not
 * affiliated with Stardock.
 */

#pragma once
#include <string>

// HKCU\Software\DexCorral value: set once the conflict dialog has been shown, so
// later starts only remind with a balloon. Removed by the hook at the first
// start that finds no conflict, so a later conflict is explained in full again.
constexpr const wchar_t *CONFLICT_REGKEY = L"Software\\DexCorral";
constexpr const wchar_t *CONFLICT_REGVAL_NOTICED = L"ConflictNoticeShown"; // REG_DWORD

/// What was found. `found` is `installed || loaded`.
struct ConflictInfo
{
    bool found = false;
    bool installed = false; /// Listed among the installed programs
    bool loaded = false;    /// One of its modules is loaded in the Explorer process examined
    std::wstring product;   /// Name for messages, with the version when known: "Stardock Fences 6.5.4.1"
};

// ---------------------------------------------------------------------------
// Identity checks: pure string logic, unit-tested
// ---------------------------------------------------------------------------

/// True for the file names of the Fences module that Explorer loads:
/// DesktopDock.dll, DesktopDock64.dll, DesktopDockARM64.dll (any case, no path).
bool IsFencesModuleName(const std::wstring &fileName);

/// True if a module's version resource identifies it as Stardock Fences. Guards
/// against an unrelated DLL that merely shares the file name.
bool IsFencesVersionInfo(const std::wstring &companyName, const std::wstring &fileDescription);

/// True if an "installed programs" entry is Stardock Fences. Both words are
/// required, across the display name and the publisher, so that another
/// vendor's product with "Fences" in its name is not matched.
bool IsFencesInstallEntry(const std::wstring &displayName, const std::wstring &publisher);

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

/// For code running inside explorer.exe (the hook DLL): modules of this process,
/// plus the installed-programs list.
ConflictInfo DetectConflictInThisProcess();

/// For DexCorral.exe: modules of the shell's Explorer process, plus the
/// installed-programs list.
ConflictInfo DetectConflictInShell();

/// Only the "loaded in this process" half. Cheap enough to poll.
bool IsConflictLoadedInThisProcess();
