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
#include <process.h>

// Debug log (dllmain.cpp); a no-op unless DebugLogging is on
void DllLog(const wchar_t *format, ...);

namespace
{
    /// What the watch thread works with. It owns this and frees it.
    struct Watch
    {
        std::wstring path;
        FolderWatcher::ChangeCallback changed;
        HANDLE stop = nullptr; // The thread's own handle to the stop event
    };

    unsigned __stdcall WatchThread(void *param)
    {
        Watch *watch = static_cast<Watch *>(param);

        HANDLE hDir = CreateFileW(
            watch->path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (hDir == INVALID_HANDLE_VALUE)
        {
            DllLog(L"Folder watch: cannot open '%s' (error %lu), not watching it", watch->path.c_str(), GetLastError());
        }
        else
        {
            BYTE buffer[4096];
            OVERLAPPED overlapped = {};
            overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            bool watching = false; // A request was taken: the folder was being watched
            bool deleted = false;  // While it was: it has ended by itself

            while (overlapped.hEvent && WaitForSingleObject(watch->stop, 0) != WAIT_OBJECT_0)
            {
                DWORD bytesReturned = 0;
                if (!ReadDirectoryChangesW(
                        hDir, buffer, sizeof(buffer),
                        FALSE, // Don't watch subtree
                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                        &bytesReturned, &overlapped, nullptr))
                {
                    // The folder was removed or unmounted (or never takes requests)
                    const DWORD error = GetLastError();
                    DllLog(L"Folder watch: '%s' ended (error %lu)", watch->path.c_str(), error);
                    deleted = watching && error == ERROR_ACCESS_DENIED;
                    break;
                }
                watching = true;

                HANDLE handles[2] = {watch->stop, overlapped.hEvent};
                if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                {
                    // Stopped. The request still points at this stack frame: wait until it is really gone
                    CancelIo(hDir);
                    GetOverlappedResult(hDir, &overlapped, &bytesReturned, TRUE);
                    break;
                }

                // Report every change, including a completion that carries none
                // (the buffer overflowed, or the folder went away): the receiver
                // reloads the whole folder either way. Coalescing a burst is the
                // receiver's job (a restarted timer), so the last event is never lost.
                const BOOL completed = GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE);
                const DWORD error = completed ? ERROR_SUCCESS : GetLastError(); // Before the callback changes it
                if (watch->changed)
                    watch->changed(false);
                if (!completed)
                {
                    DllLog(L"Folder watch: '%s' ended (error %lu)", watch->path.c_str(), error);
                    deleted = error == ERROR_ACCESS_DENIED; // What a deleted folder reports
                    break; // A share that went away would otherwise report itself in a loop
                }
                ResetEvent(overlapped.hEvent);
            }

            if (overlapped.hEvent)
                CloseHandle(overlapped.hEvent);
            CloseHandle(hDir);

            // Only a deleted folder, so that a file system that fails every request
            // cannot make the owner restart the watch in a loop
            if (deleted && watch->changed)
                watch->changed(true);
        }

        CloseHandle(watch->stop);
        delete watch;
        return 0;
    }
} // namespace

void FolderWatcher::SetPath(const std::wstring &path)
{
    const bool wasRunning = stopEvent != nullptr;
    Stop();
    watchPath = path;
    if (wasRunning)
        Start();
}

void FolderWatcher::Start()
{
    if (stopEvent || watchPath.empty())
        return;

    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent)
        return;

    Watch *watch = new Watch{watchPath, changeCallback, nullptr};
    if (DuplicateHandle(GetCurrentProcess(), stopEvent, GetCurrentProcess(), &watch->stop, 0, FALSE, DUPLICATE_SAME_ACCESS))
        thread = (HANDLE)_beginthreadex(nullptr, 0, WatchThread, watch, 0, nullptr);

    if (!thread)
    {
        // No thread to be had: no watch, but nothing is thrown at the caller
        DllLog(L"Folder watch: no thread for '%s'", watchPath.c_str());
        if (watch->stop)
            CloseHandle(watch->stop);
        delete watch;
        CloseHandle(stopEvent);
        stopEvent = nullptr;
    }
}

void FolderWatcher::Stop()
{
    if (!stopEvent)
        return;

    SetEvent(stopEvent);
    // Normally the thread is gone within a millisecond. One that is stuck in a
    // call on a dead share is left to end by itself: it owns all it touches.
    WaitForSingleObject(thread, 200);
    CloseHandle(thread);
    thread = nullptr;
    CloseHandle(stopEvent);
    stopEvent = nullptr;
}
