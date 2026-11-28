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

/**
 * Watches one folder and calls back when something in it changes.
 *
 * The watch runs on its own thread, and that thread owns everything it uses.
 * Stop() tells it to end and waits only a moment for it: a thread that is stuck
 * in a call on a share that went away must not hold up the caller, which is
 * the thread every Corral lives on. Consequences for the callback: it is called
 * on the watch thread, it may be called once more after Stop(), and so it must
 * own what it uses (a window handle by value, not a pointer to the window).
 *
 * When the watched folder is deleted, the watch ends and calls back once more
 * with ended = true: a folder made again under the same name needs a new
 * Start(). No other ending is reported (a folder that cannot be opened, a file
 * system that takes no requests), so restarting on "ended" cannot loop.
 */
class FolderWatcher
{
public:
    using ChangeCallback = std::function<void(bool ended)>;

    FolderWatcher() = default;
    ~FolderWatcher() { Stop(); }
    FolderWatcher(const FolderWatcher &) = delete;
    FolderWatcher &operator=(const FolderWatcher &) = delete;

    void SetPath(const std::wstring &path); /// Restarts the watch if it is running
    void SetChangeCallback(ChangeCallback callback) { changeCallback = std::move(callback); }

    void Start();
    void Stop();
    const std::wstring &GetPath() const { return watchPath; }

private:
    std::wstring watchPath;
    ChangeCallback changeCallback;
    HANDLE stopEvent = nullptr; /// Set: the thread ends. Not null: a watch was started
    HANDLE thread = nullptr;
};
