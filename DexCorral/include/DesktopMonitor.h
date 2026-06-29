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

#pragma once
#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

class DesktopMonitor
{
public:
    using FileAddedCallback = std::function<void(const std::wstring &fileName)>;
    using FileRenamedCallback = std::function<void(const std::wstring &oldName, const std::wstring &newName)>;
    using FileDeletedCallback = std::function<void(const std::wstring &fileName)>;

    DesktopMonitor();
    ~DesktopMonitor();

    void SetFileAddedCallback(FileAddedCallback callback) { fileAddedCallback = callback; }
    void SetFileRenamedCallback(FileRenamedCallback callback) { fileRenamedCallback = callback; }
    void SetFileDeletedCallback(FileDeletedCallback callback) { fileDeletedCallback = callback; }

    void Start();
    void Stop();

private:
    void MonitorThread(const std::wstring &path);
    void ProcessNotification(BYTE *buffer, DWORD bytesReturned);

    static std::wstring GetDesktopPath();
    static std::wstring GetPublicDesktopPath();

    FileAddedCallback fileAddedCallback;
    FileRenamedCallback fileRenamedCallback;
    FileDeletedCallback fileDeletedCallback;

    std::atomic<bool> running;
    std::thread userDesktopThread;
    std::thread publicDesktopThread;
    HANDLE userStopEvent = nullptr;
    HANDLE publicStopEvent = nullptr;

    // For tracking renames (old name is stored until new name arrives)
    std::wstring pendingOldName;
};
