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
 * UpdateChecker.h - Opt-in GitHub release update check
 *
 * Queries the GitHub Releases API on a background thread and posts the result
 * back to the app message window. No auto-download — the caller decides whether
 * to notify the user and open the release page. Disabled by default
 * (AppConfig::CheckForUpdates).
 */

#pragma once
#include <Windows.h>
#include <string>

/// Outcome of an update check, heap-allocated and delivered via PostMessage.
/// The receiver takes ownership (delete after handling).
struct UpdateCheckResult
{
    bool Success = false;         /// Network request + JSON parse both succeeded
    bool UpdateAvailable = false; /// LatestVersion is newer than the running build
    bool UserInitiated = false;   /// Started from "Check now" (show an up-to-date / failed notice)
    std::wstring LatestVersion;   /// e.g. "1.0.18" (without leading 'v')
    std::wstring ReleaseUrl;      /// html_url of the latest release page
};

namespace UpdateChecker
{
    /**
     * Starts an asynchronous update check. Spawns a detached worker thread that
     * performs the HTTPS request; on completion it posts `resultMsg` to `hwnd`
     * with lParam = new UpdateCheckResult* (receiver owns and deletes it). If the
     * window is gone the result is freed instead. Never blocks the caller.
     */
    void StartAsyncCheck(HWND hwnd, UINT resultMsg, bool userInitiated);
}
