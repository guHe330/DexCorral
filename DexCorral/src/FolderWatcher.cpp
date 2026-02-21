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

#include "FolderWatcher.h"

FolderWatcher::FolderWatcher() : running(false), lastChangeTime(0)
{
}

FolderWatcher::~FolderWatcher()
{
    Stop();
}

void FolderWatcher::SetPath(const std::wstring &path)
{
    bool wasRunning = running;
    if (wasRunning)
    {
        Stop();
    }
    watchPath = path;
    if (wasRunning && !watchPath.empty())
    {
        Start();
    }
}

void FolderWatcher::Start()
{
    if (running || watchPath.empty())
        return;
    running = true;

    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    monitorThread = std::thread([this]()
                                { MonitorThread(); });
}

void FolderWatcher::Stop()
{
    if (!running)
        return;
    running = false;

    if (stopEvent)
    {
        SetEvent(stopEvent);
    }

    if (monitorThread.joinable())
    {
        monitorThread.join();
    }

    if (stopEvent)
    {
        CloseHandle(stopEvent);
        stopEvent = nullptr;
    }
}

void FolderWatcher::MonitorThread()
{
    HANDLE hDir = CreateFileW(
        watchPath.c_str(),
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
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            &overlapped,
            nullptr);

        if (!success)
        {
            break;
        }

        HANDLE handles[2] = {overlapped.hEvent, stopEvent};
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0)
        {
            if (GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE))
            {
                // Debounce: only fire callback if enough time has passed
                DWORD now = GetTickCount();
                DWORD lastTime = lastChangeTime.load();

                if (now - lastTime >= DEBOUNCE_MS)
                {
                    lastChangeTime.store(now);
                    if (changeCallback)
                    {
                        changeCallback();
                    }
                }
            }
            ResetEvent(overlapped.hEvent);
        }
        else
        {
            CancelIo(hDir);
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
    CloseHandle(hDir);
}
