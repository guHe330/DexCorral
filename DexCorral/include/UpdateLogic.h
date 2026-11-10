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
 * UpdateLogic.h - The update check's decisions, without the network
 *
 * Version parsing and comparison, and the check that a release URL may be
 * handed to ShellExecuteW. Header-only so the unit tests can cover them
 * without linking WinHTTP; UpdateChecker.cpp is the only production user.
 */

#pragma once
#include <string>
#include <cstring>

namespace UpdateLogic
{
// Any release URL we hand to ShellExecuteW must sit under this prefix. The
// API response is attacker-controlled if the transport or the API is ever
// compromised, and ShellExecuteW on an arbitrary string is a powerful
// primitive (file:, UNC, registered protocol handlers).
inline const char *const kReleaseUrlPrefix = "https://github.com/guHe330/DexCorral/";

struct Version
{
    int major = 0, minor = 0, patch = 0;
};

/// Parses "vX.Y.Z" / "X.Y.Z" (extra suffixes ignored) into a Version.
inline Version ParseVersion(const std::string &s)
{
    Version v;
    size_t i = 0;
    if (i < s.size() && (s[i] == 'v' || s[i] == 'V'))
        ++i;
    int *parts[3] = {&v.major, &v.minor, &v.patch};
    for (int p = 0; p < 3 && i < s.size(); ++p)
    {
        int value = 0;
        bool any = false;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9')
        {
            value = value * 10 + (s[i] - '0');
            any = true;
            ++i;
        }
        if (!any)
            break;
        *parts[p] = value;
        if (i < s.size() && s[i] == '.')
            ++i;
        else
            break;
    }
    return v;
}

/// True if `a` represents a strictly newer release than `b`.
inline bool IsNewer(const Version &a, const Version &b)
{
    if (a.major != b.major)
        return a.major > b.major;
    if (a.minor != b.minor)
        return a.minor > b.minor;
    return a.patch > b.patch;
}

/// True only for URLs under the project's own GitHub repository. The scheme
/// and host are matched case-insensitively (they are case-insensitive per
/// RFC 3986); the path is matched exactly. A userinfo trick such as
/// "https://github.com@evil/" fails because the prefix requires '/' right
/// after the host.
inline bool IsTrustedReleaseUrl(const std::string &url)
{
    const size_t prefixLen = strlen(kReleaseUrlPrefix);
    if (url.size() <= prefixLen)
        return false;
    if (_strnicmp(url.c_str(), kReleaseUrlPrefix, prefixLen) != 0)
        return false;
    if (url.find("..") != std::string::npos)
        return false;
    // Reject control characters and anything non-ASCII; a legitimate
    // release URL has neither.
    for (unsigned char c : url)
    {
        if (c < 0x21 || c > 0x7E)
            return false;
    }
    return true;
}
} // namespace UpdateLogic
