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
 * HookBridge.cpp - In-process communication between app and Explorer hook
 *
 * Both the app (worker thread) and the Explorer hook (UI thread) run inside
 * DexCorralHook.dll in the same process. Communication uses a global vector
 * protected by a CRITICAL_SECTION, with a version counter for zero-cost
 * cache checks on the hook side.
 */

#include "HookBridge.h"
#include "Constants.h"
#include <vector>
#include <ShlObj.h>
#include <stdio.h>
#include <string.h>

static CRITICAL_SECTION s_Lock;
static std::vector<HiddenIconInfo> s_HiddenIcons;
static volatile DWORD s_Version = 0;
static bool s_Initialized = false;
static bool s_DebugLogging = false;
static bool s_DebugLoggingInitialized = false;
static volatile HWND s_AppMessageWindow = nullptr;
static volatile LONG s_AppIconMoveDepth = 0;
static HMODULE s_DllModule = nullptr;

void HookBridge::Initialize()
{
    if (!s_Initialized)
    {
        InitializeCriticalSection(&s_Lock);
        s_Initialized = true;
    }
}

void HookBridge::Cleanup()
{
    if (s_Initialized)
    {
        DeleteCriticalSection(&s_Lock);
        s_Initialized = false;
    }
}

void HookBridge::UpdateHiddenIcons(const std::vector<HiddenIconInfo> &icons)
{
    EnterCriticalSection(&s_Lock);
    // Only bump the version when the list actually changed. The hook's
    // deferred revalidation posts a repark to the app, which recomputes this
    // list — without this check that round trip would bump the version again
    // and loop forever.
    if (!(s_HiddenIcons == icons))
    {
        s_HiddenIcons = icons;
        s_Version++;
    }
    LeaveCriticalSection(&s_Lock);
}

void HookBridge::ClearHiddenIcons()
{
    EnterCriticalSection(&s_Lock);
    s_HiddenIcons.clear();
    s_Version++;
    LeaveCriticalSection(&s_Lock);
}

DWORD HookBridge::GetVersion()
{
    return s_Version;
}

DWORD HookBridge::GetHiddenIcons(std::vector<HiddenIconInfo> &out)
{
    EnterCriticalSection(&s_Lock);
    out = s_HiddenIcons;
    DWORD ver = s_Version;
    LeaveCriticalSection(&s_Lock);
    return ver;
}

static std::vector<IconPositionRequest> s_PositionRequests;
static std::vector<DesktopIconInfo> s_IconSnapshot;

void HookBridge::SetIconSnapshot(const std::vector<DesktopIconInfo> &icons)
{
    EnterCriticalSection(&s_Lock);
    s_IconSnapshot = icons;
    LeaveCriticalSection(&s_Lock);
}

void HookBridge::TakeIconSnapshot(std::vector<DesktopIconInfo> &out)
{
    EnterCriticalSection(&s_Lock);
    out.swap(s_IconSnapshot);
    s_IconSnapshot.clear();
    LeaveCriticalSection(&s_Lock);
}

void HookBridge::SetIconPositionRequests(const std::vector<IconPositionRequest> &requests)
{
    EnterCriticalSection(&s_Lock);
    s_PositionRequests = requests;
    LeaveCriticalSection(&s_Lock);
}

void HookBridge::TakeIconPositionRequests(std::vector<IconPositionRequest> &out)
{
    EnterCriticalSection(&s_Lock);
    out.swap(s_PositionRequests);
    s_PositionRequests.clear();
    LeaveCriticalSection(&s_Lock);
}

void HookBridge::BeginAppIconMove()
{
    InterlockedIncrement(&s_AppIconMoveDepth);
}

void HookBridge::EndAppIconMove()
{
    InterlockedDecrement(&s_AppIconMoveDepth);
}

bool HookBridge::IsAppIconMoveActive()
{
    return s_AppIconMoveDepth > 0;
}

void HookBridge::SetAppMessageWindow(HWND hwnd)
{
    s_AppMessageWindow = hwnd;
}

HWND HookBridge::GetAppMessageWindow()
{
    return s_AppMessageWindow;
}

// Reads the DebugLogging flag straight from config.json. The first log calls
// happen during DLL injection, before the App has loaded the config and
// called SetDebugLogging — without this bootstrap, enabling DebugLogging
// could never capture startup logging. Naive token scan instead of a JSON
// parse: this can run under the loader lock, and the file is machine-written.
static bool ReadDebugLoggingFromConfigFile()
{
    wchar_t path[MAX_PATH];
    wchar_t *appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)))
        return false;
    swprintf_s(path, L"%s\\DexCorral\\config.json", appData);
    CoTaskMemFree(appData);

    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    bool enabled = false;
    DWORD size = GetFileSize(hFile, nullptr);
    if (size != INVALID_FILE_SIZE && size > 0 && size < 4 * 1024 * 1024)
    {
        std::vector<char> data(size + 1, 0);
        DWORD read = 0;
        if (ReadFile(hFile, data.data(), size, &read, nullptr) && read > 0)
        {
            data[read] = 0;
            const char *key = strstr(data.data(), "\"DebugLogging\"");
            if (key)
            {
                const char *p = key + strlen("\"DebugLogging\"");
                while (*p == ':' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
                    p++;
                enabled = (strncmp(p, "true", 4) == 0);
            }
        }
    }
    CloseHandle(hFile);
    return enabled;
}

void HookBridge::SetDebugLogging(bool enable)
{
    s_DebugLoggingInitialized = true;
    s_DebugLogging = enable;
}

bool HookBridge::IsDebugLogging()
{
    if (!s_DebugLoggingInitialized)
    {
        s_DebugLoggingInitialized = true; // Set first: never re-read on failure
        s_DebugLogging = ReadDebugLoggingFromConfigFile();
    }
    return s_DebugLogging;
}

void HookBridge::SetDllModule(HMODULE hModule)
{
    s_DllModule = hModule;
}

HMODULE HookBridge::GetDllModule()
{
    return s_DllModule;
}

void HookBridge::RefreshDesktop()
{
    HWND hListView = DesktopIcons::GetDesktopListView();
    if (hListView)
    {
        RedrawWindow(hListView, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
}

// ----------------------------------------------------------------------------
// Debug log
// ----------------------------------------------------------------------------

namespace
{
    const DWORD kMaxDebugLogBytes = 2 * 1024 * 1024;

    /// "%APPDATA%\DexCorral\" (created if missing), or the temp folder if that fails
    std::wstring ResolveLogFolder()
    {
        wchar_t *appData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)) && appData)
        {
            std::wstring dir = std::wstring(appData) + L"\\DexCorral";
            CoTaskMemFree(appData);
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir + L"\\";
        }
        wchar_t temp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, temp);
        return temp;
    }
} // namespace

void HookBridge::WriteDebugLog(const wchar_t *fileName, const wchar_t *format, va_list args)
{
    if (!IsDebugLogging())
        return;

    // _TRUNCATE: the secure formatters without it end the process on overflow,
    // and the process is Explorer.
    wchar_t message[1024];
    _vsnwprintf_s(message, _countof(message), _TRUNCATE, format, args);

    static const std::wstring folder = ResolveLogFolder();
    const std::wstring path = folder + fileName;

    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info) &&
        (info.nFileSizeHigh != 0 || info.nFileSizeLow > kMaxDebugLogBytes))
    {
        MoveFileExW(path.c_str(), (path + L".old").c_str(), MOVEFILE_REPLACE_EXISTING);
    }

    // Shared for writing: hook, app and watcher threads log concurrently, and
    // FILE_APPEND_DATA makes each write land at the end.
    HANDLE hFile = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t line[1100];
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"[%02d:%02d:%02d.%03d] %s\r\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
    DWORD written;
    WriteFile(hFile, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(hFile);
}