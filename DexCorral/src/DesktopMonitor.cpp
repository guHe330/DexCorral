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

    // Start monitoring threads
    std::wstring userPath = GetDesktopPath();
    std::wstring publicPath = GetPublicDesktopPath();

    if (!userPath.empty())
    {
        userDesktopThread = std::thread([this, userPath]()
                                        { MonitorThread(userPath); });
    }

    if (!publicPath.empty() && publicPath != userPath)
    {
        publicDesktopThread = std::thread([this, publicPath]()
                                          { MonitorThread(publicPath); });
    }
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

void DesktopMonitor::MonitorThread(const std::wstring &path)
{
    HANDLE hDir = CreateFileW(
        path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (hDir == INVALID_HANDLE_VALUE)
    {
        return;
    }

    BYTE buffer[4096];
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    while (running)
    {
        DWORD bytesReturned = 0;

        BOOL success = ReadDirectoryChangesW(
            hDir,
            buffer,
            sizeof(buffer),
            FALSE, // Don't watch subtree
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
            &bytesReturned,
            &overlapped,
            nullptr);

        if (!success)
        {
            break;
        }

        // Wait for either the directory change or stop signal
        HANDLE handles[2] = {overlapped.hEvent, userStopEvent ? userStopEvent : publicStopEvent};
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0)
        {
            // Directory change occurred
            if (GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE))
            {
                ProcessNotification(buffer, bytesReturned);
            }
            ResetEvent(overlapped.hEvent);
        }
        else
        {
            // Stop signal or error
            CancelIo(hDir);
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
}

void DesktopMonitor::ProcessNotification(BYTE *buffer, DWORD bytesReturned)
{
    if (bytesReturned == 0)
        return;

    FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)buffer;

    while (true)
    {
        // Extract filename (it's not null-terminated)
        std::wstring fileName(info->FileName, info->FileNameLength / sizeof(WCHAR));

        switch (info->Action)
        {
        case FILE_ACTION_ADDED:
            if (fileAddedCallback)
            {
                fileAddedCallback(fileName);
            }
            break;

        case FILE_ACTION_REMOVED:
            if (fileDeletedCallback)
            {
                fileDeletedCallback(fileName);
            }
            break;

        case FILE_ACTION_RENAMED_OLD_NAME:
            pendingOldName = fileName;
            break;

        case FILE_ACTION_RENAMED_NEW_NAME:
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
