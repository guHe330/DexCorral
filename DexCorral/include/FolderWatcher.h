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

class FolderWatcher
{
public:
    using ChangeCallback = std::function<void()>;

    FolderWatcher();
    ~FolderWatcher();

    void SetPath(const std::wstring &path);
    void SetChangeCallback(ChangeCallback callback) { changeCallback = callback; }

    void Start();
    void Stop();
    bool IsRunning() const { return running; }
    const std::wstring &GetPath() const { return watchPath; }

private:
    void MonitorThread();

    std::wstring watchPath;
    ChangeCallback changeCallback;

    std::atomic<bool> running;
    std::thread monitorThread;
    HANDLE stopEvent = nullptr;

    // Debouncing
    std::atomic<DWORD> lastChangeTime;
    static const DWORD DEBOUNCE_MS = 500;
};
