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

#include "UpdateChecker.h"
#include "Version.h"

#include <winhttp.h>
#include <nlohmann/json.hpp>

#include <string>
#include <thread>
#include <memory>

#pragma comment(lib, "winhttp.lib")

// Defined in dllmain.cpp; self-gates on HookBridge::IsDebugLogging() (no-op
// unless debug logging is enabled in config).
void DllLog(const wchar_t *format, ...);

namespace
{
    // GitHub Releases API endpoint for the latest published release.
    const wchar_t *kApiHost = L"api.github.com";
    const wchar_t *kApiPath = L"/repos/guHe330/DexCorral/releases/latest";
    const wchar_t *kUserAgent = L"DexCorral-UpdateCheck";

    struct Version
    {
        int major = 0, minor = 0, patch = 0;
    };

    /// Parses "vX.Y.Z" / "X.Y.Z" (extra suffixes ignored) into a Version.
    Version ParseVersion(const std::string &s)
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
    bool IsNewer(const Version &a, const Version &b)
    {
        if (a.major != b.major)
            return a.major > b.major;
        if (a.minor != b.minor)
            return a.minor > b.minor;
        return a.patch > b.patch;
    }

    std::wstring Utf8ToWide(const std::string &s)
    {
        if (s.empty())
            return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], len);
        return w;
    }

    /// Performs the blocking HTTPS GET and fills the result. Returns true on a
    /// successful request + parse (UpdateAvailable is set independently).
    bool FetchLatestRelease(UpdateCheckResult &result)
    {
        HINTERNET hSession = WinHttpOpen(kUserAgent,
                                         WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession)
            return false;

        // Short timeouts — an update check must never feel like a hang.
        WinHttpSetTimeouts(hSession, 8000, 8000, 8000, 8000);

        HINTERNET hConnect = WinHttpConnect(hSession, kApiHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", kApiPath, nullptr,
                                                WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                WINHTTP_FLAG_SECURE);
        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        bool ok = false;
        std::string body;

        const wchar_t *headers = L"Accept: application/vnd.github+json\r\n"
                                 L"X-GitHub-Api-Version: 2022-11-28\r\n";

        if (WinHttpSendRequest(hRequest, headers, (DWORD)-1L,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr))
        {
            DWORD statusCode = 0;
            DWORD size = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest,
                                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size,
                                WINHTTP_NO_HEADER_INDEX);

            if (statusCode == 200)
            {
                DWORD avail = 0;
                do
                {
                    avail = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0)
                        break;
                    std::string chunk(avail, '\0');
                    DWORD read = 0;
                    if (!WinHttpReadData(hRequest, &chunk[0], avail, &read))
                        break;
                    chunk.resize(read);
                    body += chunk;
                } while (avail > 0);
                ok = !body.empty();
            }
            else
            {
                DllLog(L"UpdateChecker: HTTP status %lu", statusCode);
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (!ok)
            return false;

        try
        {
            auto json = nlohmann::json::parse(body);
            std::string tag = json.value("tag_name", std::string());
            std::string url = json.value("html_url", std::string());
            if (tag.empty())
                return false;

            Version latest = ParseVersion(tag);
            Version current{DEXCORRAL_VERSION_MAJOR, DEXCORRAL_VERSION_MINOR, DEXCORRAL_VERSION_PATCH};

            // Normalize the displayed version (strip a leading 'v').
            std::string display = tag;
            if (!display.empty() && (display[0] == 'v' || display[0] == 'V'))
                display.erase(0, 1);

            result.Success = true;
            result.UpdateAvailable = IsNewer(latest, current);
            result.LatestVersion = Utf8ToWide(display);
            result.ReleaseUrl = Utf8ToWide(url);
            return true;
        }
        catch (...)
        {
            DllLog(L"UpdateChecker: JSON parse failed");
            return false;
        }
    }
}

namespace UpdateChecker
{
    void StartAsyncCheck(HWND hwnd, UINT resultMsg, bool userInitiated)
    {
        if (!hwnd)
            return;

        std::thread([hwnd, resultMsg, userInitiated]()
                    {
            auto result = std::make_unique<UpdateCheckResult>();
            result->UserInitiated = userInitiated;
            FetchLatestRelease(*result);

            // Hand ownership to the message window; free it if the post fails
            // (e.g. the window was already destroyed during shutdown).
            if (!PostMessageW(hwnd, resultMsg, 0, (LPARAM)result.get()))
            {
                // PostMessage failed — keep ownership and let unique_ptr free it.
            }
            else
            {
                result.release();
            } })
            .detach();
    }
}
