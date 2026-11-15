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

#include "DesktopMonitor.h"

// Debug log (dllmain.cpp); a no-op unless DebugLogging is on
void DllLog(const wchar_t *format, ...);
#include <ShlObj.h>

DesktopMonitor::DesktopMonitor() : running(false)
{
}

DesktopMonitor::~DesktopMonitor()
{
    Stop();
}

std::wstring DesktopMonitor::GetDesktopPath()
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path)))
    {
        return std::wstring(path);
    }
    return L"";
}

std::wstring DesktopMonitor::GetPublicDesktopPath()
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, path)))
    {
        return std::wstring(path);
    }
    return L"";
}

void DesktopMonitor::Start()
{
    if (running)
        return;
    running = true;

    // Create stop events
    userStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    publicStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // One thread per desktop folder. Each resolves its own path, so a desktop
    // that is relocated during the session is picked up again.
    userDesktopThread = std::thread([this]()
                                    { MonitorThread(false); });
    publicDesktopThread = std::thread([this]()
                                      { MonitorThread(true); });
}

void DesktopMonitor::Stop()
{
    if (!running)
        return;
    running = false;

    // Signal threads to stop
    if (userStopEvent)
        SetEvent(userStopEvent);
    if (publicStopEvent)
        SetEvent(publicStopEvent);

    // Wait for threads to finish
    if (userDesktopThread.joinable())
    {
        userDesktopThread.join();
    }
    if (publicDesktopThread.joinable())
    {
        publicDesktopThread.join();
    }

    // Clean up events
    if (userStopEvent)
    {
        CloseHandle(userStopEvent);
        userStopEvent = nullptr;
    }
    if (publicStopEvent)
    {
        CloseHandle(publicStopEvent);
        publicStopEvent = nullptr;
    }
}

void DesktopMonitor::MonitorThread(bool publicDesktop)
{
    HANDLE stopEvent = publicDesktop ? publicStopEvent : userStopEvent;

    // The names in the folder as far as the events told us. Kept across a
    // reopened watch so that what was missed in between can be worked out.
    NameSet known;
    bool haveKnown = false;

    const DWORD firstRetryMs = 2000, maxRetryMs = 60000;
    DWORD retryMs = firstRetryMs;

    while (running)
    {
        std::wstring path = publicDesktop ? GetPublicDesktopPath() : GetDesktopPath();
        // Both desktops in one folder: the user thread covers it
        if (publicDesktop && _wcsicmp(path.c_str(), GetDesktopPath().c_str()) == 0)
            path.clear();

        HANDLE hDir = path.empty() ? INVALID_HANDLE_VALUE
                                   : CreateFileW(path.c_str(), FILE_LIST_DIRECTORY,
                                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                                 OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                                 nullptr);
        if (hDir != INVALID_HANDLE_VALUE)
        {
            retryMs = firstRetryMs;
            WatchFolder(hDir, path, publicDesktop, stopEvent, known, haveKnown);
            CloseHandle(hDir);
        }
        else if (!path.empty() && retryMs == firstRetryMs)
        {
            DllLog(L"Desktop watch: cannot open '%s' (error %lu), retrying", path.c_str(), GetLastError());
        }

        // Try again after a pause that grows while the folder stays away
        if (WaitForSingleObject(stopEvent, retryMs) != WAIT_TIMEOUT)
            break;
        retryMs = retryMs * 2 < maxRetryMs ? retryMs * 2 : maxRetryMs;
    }
}

void DesktopMonitor::WatchFolder(HANDLE hDir, const std::wstring &path, bool publicDesktop, HANDLE stopEvent,
                                 NameSet &known, bool &haveKnown)
{
    BYTE buffer[4096];
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // Old name of a rename until its new name arrives. Per thread: the user and
    // the public desktop are watched by two threads, and a shared member let
    // one pair up with the other's half.
    std::wstring pendingOldName;

    bool requestPending = false;
    bool reconcile = true; // once the first request is queued, so nothing falls in between

    while (running)
    {
        DWORD bytesReturned = 0;

        if (!requestPending)
        {
            if (!ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE, // Don't watch subtree
                                       FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME, &bytesReturned,
                                       &overlapped, nullptr))
            {
                // The folder was removed, renamed or unmounted
                DllLog(L"Desktop watch: '%s' ended (error %lu), reopening", path.c_str(), GetLastError());
                break;
            }
            requestPending = true;
        }

        if (reconcile)
        {
            Reconcile(path, known, haveKnown);
            reconcile = false;
        }

        // Wait for a change or the stop signal; wake now and then to see
        // whether the desktop folder was moved (OneDrive backup on or off)
        HANDLE handles[2] = {overlapped.hEvent, stopEvent};
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, 30000);

        if (waitResult == WAIT_OBJECT_0)
        {
            requestPending = false;
            if (GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE) && bytesReturned != 0)
            {
                ProcessNotification(buffer, bytesReturned, pendingOldName, known);
            }
            else if (bytesReturned == 0 || GetLastError() == ERROR_NOTIFY_ENUM_DIR)
            {
                // The change buffer overflowed: Windows reports nothing rather
                // than a partial list. Work out what happened from a listing.
                DllLog(L"Desktop watch: change buffer overflowed, comparing '%s' with what was known", path.c_str());
                reconcile = true;
            }
            else
            {
                DllLog(L"Desktop watch: '%s' ended (error %lu), reopening", path.c_str(), GetLastError());
                break;
            }
            ResetEvent(overlapped.hEvent);
        }
        else if (waitResult == WAIT_TIMEOUT)
        {
            std::wstring now = publicDesktop ? GetPublicDesktopPath() : GetDesktopPath();
            if (!now.empty() && _wcsicmp(now.c_str(), path.c_str()) != 0)
            {
                DllLog(L"Desktop watch: desktop moved from '%s' to '%s'", path.c_str(), now.c_str());
                break;
            }
        }
        else
        {
            break; // Stop signal or error
        }
    }

    if (requestPending)
    {
        // The request still points at this stack frame: wait until it is really gone
        CancelIo(hDir);
        DWORD ignored = 0;
        GetOverlappedResult(hDir, &overlapped, &ignored, TRUE);
    }
    CloseHandle(overlapped.hEvent);
}

void DesktopMonitor::Reconcile(const std::wstring &path, NameSet &known, bool &haveKnown)
{
    NameSet listed;
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((path + L"\\*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return; // unreadable right now: keep what is known, the next round compares again
    do
    {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0)
            listed.insert(fd.cFileName);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    // The first listing is only the starting point: files that were on the
    // desktop before DexCorral started are not "new"
    if (haveKnown)
    {
        std::vector<std::wstring> added, removed;
        DiffNames(known, listed, added, removed);
        for (const auto &name : removed)
            if (fileDeletedCallback)
                fileDeletedCallback(name);
        for (const auto &name : added)
            if (fileAddedCallback)
                fileAddedCallback(name);
        if (!added.empty() || !removed.empty())
            DllLog(L"Desktop watch: recovered %u added and %u removed in '%s'", (unsigned)added.size(),
                   (unsigned)removed.size(), path.c_str());
    }
    known.swap(listed);
    haveKnown = true;
}

void DesktopMonitor::ProcessNotification(BYTE *buffer, DWORD bytesReturned, std::wstring &pendingOldName,
                                         NameSet &known)
{
    UNREFERENCED_PARAMETER(bytesReturned);
    FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)buffer;

    while (true)
    {
        // Extract filename (it's not null-terminated)
        std::wstring fileName(info->FileName, info->FileNameLength / sizeof(WCHAR));

        switch (info->Action)
        {
        case FILE_ACTION_ADDED:
            known.insert(fileName);
            if (fileAddedCallback)
            {
                fileAddedCallback(fileName);
            }
            break;

        case FILE_ACTION_REMOVED:
            known.erase(fileName);
            if (fileDeletedCallback)
            {
                fileDeletedCallback(fileName);
            }
            break;

        case FILE_ACTION_RENAMED_OLD_NAME:
            known.erase(fileName);
            pendingOldName = fileName;
            break;

        case FILE_ACTION_RENAMED_NEW_NAME:
            known.insert(fileName);
            if (!pendingOldName.empty() && fileRenamedCallback)
            {
                fileRenamedCallback(pendingOldName, fileName);
                pendingOldName.clear();
            }
            break;
        }

        // Move to next entry
        if (info->NextEntryOffset == 0)
        {
            break;
        }
        info = (FILE_NOTIFY_INFORMATION *)((BYTE *)info + info->NextEntryOffset);
    }
}
