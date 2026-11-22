// DexCorral - Organize your desktop icons into corrals
// Copyright (C) 2026 guHe330
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <functional>
#include <utility>

/// One thing the user asked for that has to wait for an answer (a network
/// share saying it is there). The latest request replaces an older one, and a
/// request that waited too long is dropped: a file must not open by itself
/// half a minute after the double-click.
struct PendingAction
{
    std::function<void()> action;
    unsigned long long since = 0;

    void Set(std::function<void()> a, unsigned long long now)
    {
        action = std::move(a);
        since = now;
    }

    /// Hands the action out, once. Empty if there is none or it waited longer than maxWaitMs.
    std::function<void()> Take(unsigned long long now, unsigned long long maxWaitMs)
    {
        std::function<void()> a = std::move(action);
        action = nullptr;
        if (a && now - since > maxWaitMs)
            a = nullptr;
        return a;
    }
};
