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
 * RecentlyLeft.h - Where a desktop file sat in a corral just before it went away
 *
 * Programs rarely change a file in place. An installer deletes its desktop
 * shortcut and writes a new one; an editor saves to a temporary file, renames
 * the old one away and the new one into place. To the folder watcher that is
 * "deleted" or "renamed away" followed a moment later by the same name coming
 * back, and handled event by event it throws the file out of its corral.
 *
 * This remembers, for a few seconds, the slot a name was removed from, so the
 * name can be put back where it was when it returns. No Win32 calls and an
 * injected clock, so it is unit-tested.
 */

#pragma once
#include <map>
#include <string>

/// One remembered slot. `corral` is an opaque identity; the caller checks that
/// it still exists before using it.
struct LeftSlot
{
    const void *corral = nullptr;
    int tabIndex = -1;
    size_t index = 0;
    unsigned long tick = 0;
};

class RecentlyLeft
{
public:
    /// How long a slot stays claimable, in milliseconds
    static const unsigned long kWindowMs = 3000;

    void Remember(const std::string &name, const void *corral, int tabIndex, size_t index, unsigned long now)
    {
        Prune(now);
        slots[name] = LeftSlot{corral, tabIndex, index, now};
    }

    /// Hands out and forgets the slot `name` left, if it left recently enough
    bool Take(const std::string &name, unsigned long now, LeftSlot &out)
    {
        Prune(now);
        auto it = slots.find(name);
        if (it == slots.end())
            return false;
        out = it->second;
        slots.erase(it);
        return true;
    }

    size_t Size() const { return slots.size(); }

private:
    /// Unsigned subtraction, so a wrapping tick counter is handled
    void Prune(unsigned long now)
    {
        for (auto it = slots.begin(); it != slots.end();)
        {
            if (now - it->second.tick > kWindowMs)
                it = slots.erase(it);
            else
                ++it;
        }
    }

    std::map<std::string, LeftSlot> slots;
};