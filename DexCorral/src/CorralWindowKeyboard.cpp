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
 * CorralWindowKeyboard.cpp - Selection model, keyboard handling and shell verbs
 *
 * Holds the multi-select state helpers, Explorer-style key handling (arrows, Home/End,
 * Delete/Enter/clipboard, F5, Menu key, type-ahead, Backspace) and rubber-band
 * selection. Destructive and clipboard actions go through the shell's own verbs so they
 * inherit Explorer's confirmations, recycle bin and undo.
 */

#include "CorralWindow.h"
#include "App.h"
#include "LayoutMath.h"
#include <ShlObj.h>
#include <shobjidl.h>
#include <algorithm>
#include <cwctype>

// ============================================================================
// Selection helpers
// ============================================================================

bool CorralWindow::IsSelected(int index) const
{
    return selection.find(index) != selection.end();
}

std::vector<int> CorralWindow::GetSelectedIndices() const
{
    std::vector<int> result;
    for (int i : selection)
    {
        if (i >= 0 && i < (int)icons.size())
            result.push_back(i);
    }
    return result;
}

void CorralWindow::ClearSelection()
{
    selection.clear();
    selectedIcon = -1;
    selectionAnchor = -1;
}

void CorralWindow::SelectSingle(int index)
{
    selection.clear();
    if (index >= 0 && index < (int)icons.size())
        selection.insert(index);
    selectedIcon = index;
    selectionAnchor = index;
}

void CorralWindow::ToggleSelection(int index)
{
    if (index < 0 || index >= (int)icons.size())
        return;
    if (IsSelected(index))
        selection.erase(index);
    else
        selection.insert(index);
    selectedIcon = index;
    selectionAnchor = index;
}

void CorralWindow::SelectRangeTo(int index, bool keepExisting)
{
    if (index < 0 || index >= (int)icons.size())
        return;
    if (selectionAnchor < 0 || selectionAnchor >= (int)icons.size())
        selectionAnchor = index;

    if (!keepExisting)
        selection.clear();

    int lo = (std::min)(selectionAnchor, index);
    int hi = (std::max)(selectionAnchor, index);
    for (int i = lo; i <= hi; i++)
        selection.insert(i);

    selectedIcon = index;
}

void CorralWindow::SelectAll()
{
    selection.clear();
    for (int i = 0; i < (int)icons.size(); i++)
        selection.insert(i);
    if (!icons.empty())
    {
        if (selectedIcon < 0 || selectedIcon >= (int)icons.size())
            selectedIcon = 0;
        selectionAnchor = selectedIcon;
    }
}

void CorralWindow::SetFocusIcon(int index, bool select)
{
    if (index < 0 || index >= (int)icons.size())
        return;
    if (select)
        SelectSingle(index);
    else
        selectedIcon = index;
    EnsureIconVisible(index);
    UpdateLayeredContent();
}

void CorralWindow::EnsureIconVisible(int index)
{
    if (index < 0 || index >= (int)icons.size())
        return;

    const RECT &r = icons[index].rect;
    int visibleTop = GetIconAreaTop();
    int visibleHeight = GetVisibleHeight();

    if (r.top - scrollPosition < visibleTop)
        scrollPosition = r.top - visibleTop;
    else if (r.bottom - scrollPosition > visibleHeight)
        scrollPosition = r.bottom - visibleHeight;

    ClampScrollPosition();
}

// ============================================================================
// Keyboard navigation
// ============================================================================

void CorralWindow::MoveFocusByDirection(int vk, bool extend, bool focusOnly)
{
    if (icons.empty())
        return;

    if (selectedIcon < 0)
    {
        SelectSingle(0);
        EnsureIconVisible(0);
        UpdateLayeredContent();
        return;
    }

    // Geometry is authoritative: every rect was recomputed by the last layout pass,
    // so the true on-screen arrangement is already in hand (no column arithmetic).
    std::vector<LayoutMath::IconCell> cells(icons.size());
    for (size_t i = 0; i < icons.size(); i++)
        cells[i].rect = icons[i].rect;

    LayoutMath::NavDirection dir = LayoutMath::NavDirection::Up;
    switch (vk)
    {
    case VK_DOWN:
        dir = LayoutMath::NavDirection::Down;
        break;
    case VK_LEFT:
        dir = LayoutMath::NavDirection::Left;
        break;
    case VK_RIGHT:
        dir = LayoutMath::NavDirection::Right;
        break;
    default:
        break;
    }

    int next = LayoutMath::FindNeighbor(cells, selectedIcon, dir);

    // Left/Right at a row edge — and details view, where rows have no horizontal
    // neighbour at all — falls back to the previous/next item, like Explorer.
    if (next < 0 && (vk == VK_LEFT || vk == VK_RIGHT))
    {
        int candidate = selectedIcon + (vk == VK_RIGHT ? 1 : -1);
        if (candidate >= 0 && candidate < (int)icons.size())
            next = candidate;
    }

    if (next < 0)
        return;

    if (extend)
        SelectRangeTo(next, false);
    else if (focusOnly)
        selectedIcon = next;
    else
        SelectSingle(next);

    EnsureIconVisible(next);
    UpdateLayeredContent();
}

void CorralWindow::ShowContextMenuForFocusedIcon()
{
    if (selectedIcon >= 0 && selectedIcon < (int)icons.size())
    {
        // Anchor at the icon rect, not the cursor — the cursor may be anywhere.
        RECT r = icons[selectedIcon].rect;
        POINT pt = {r.left + Dpi(8), r.bottom - scrollPosition - Dpi(4)};
        ClientToScreen(hwnd, &pt);
        ShowShellContextMenu(selectedIcon, pt.x, pt.y);
        return;
    }
    ShowContextMenu(Dpi(4), GetTitleBarHeight() / 2);
}

// ============================================================================
// Type-ahead
// ============================================================================

void CorralWindow::OnTypeAheadTimeout()
{
    KillTimer(hwnd, TYPEAHEAD_TIMER_ID);
    typeAheadPrefix.clear();
}

void CorralWindow::OnChar(wchar_t ch)
{
    if (isRenamingIcon || ch < L' ' || icons.empty())
        return;

    typeAheadPrefix += (wchar_t)towlower(ch);
    SetTimer(hwnd, TYPEAHEAD_TIMER_ID, TYPEAHEAD_RESET_MS, nullptr);

    // Start just past the focused icon so a repeated letter cycles through matches.
    int count = (int)icons.size();
    int from = (selectedIcon < 0) ? -1 : selectedIcon;
    bool cycling = (typeAheadPrefix.size() == 1);
    for (int step = cycling ? 1 : 0; step <= count; step++)
    {
        int i = ((from + step) % count + count) % count;
        std::wstring name = icons[i].displayName;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](wchar_t c) { return (wchar_t)towlower(c); });
        if (name.size() >= typeAheadPrefix.size() &&
            name.compare(0, typeAheadPrefix.size(), typeAheadPrefix) == 0)
        {
            SelectSingle(i);
            EnsureIconVisible(i);
            UpdateLayeredContent();
            return;
        }
    }
}

// ============================================================================
// Key handling
// ============================================================================

bool CorralWindow::OnKeyDown(WPARAM vk)
{
    if (isRenamingIcon)
        return false; // The rename edit control owns the keyboard

    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    switch (vk)
    {
    case VK_F2:
        if (selectedIcon >= 0)
        {
            StartIconRename(selectedIcon);
            return true;
        }
        return false;

    case VK_F5:
        LoadFiles();
        return true;

    case VK_ESCAPE:
        if (!selection.empty() || selectedIcon >= 0)
        {
            ClearSelection();
            UpdateLayeredContent();
        }
        typeAheadPrefix.clear();
        return true;

    case VK_APPS:
        ShowContextMenuForFocusedIcon();
        return true;

    case VK_BACK:
        if (GetActiveTab().IsVirtual && !GetActiveTab().CurrentSubPath.empty())
        {
            NavigateUp();
            return true;
        }
        return false;

    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
        MoveFocusByDirection((int)vk, shift, ctrl && !shift);
        return true;

    case VK_HOME:
    case VK_END:
    {
        if (icons.empty())
            return true;
        int target = (vk == VK_HOME) ? 0 : (int)icons.size() - 1;
        if (shift)
            SelectRangeTo(target, false);
        else
            SelectSingle(target);
        EnsureIconVisible(target);
        UpdateLayeredContent();
        return true;
    }

    case VK_DELETE:
        InvokeVerbOnSelection("delete");
        return true;

    case VK_RETURN:
        if (alt)
            InvokeVerbOnSelection("properties");
        else
            OpenSelection();
        return true;

    case 'A':
        if (ctrl)
        {
            SelectAll();
            UpdateLayeredContent();
            return true;
        }
        return false;

    case 'C':
        if (ctrl)
        {
            InvokeVerbOnSelection("copy");
            return true;
        }
        return false;

    case 'X':
        if (ctrl)
        {
            InvokeVerbOnSelection("cut");
            return true;
        }
        return false;

    case 'V':
        if (ctrl)
        {
            InvokeVerbOnFolder("paste");
            return true;
        }
        return false;
    }

    return false;
}

void CorralWindow::OpenSelection()
{
    std::vector<int> sel = GetSelectedIndices();
    if (sel.empty())
        return;

    // A single sub-folder navigates inline, matching the double-click behaviour.
    if (sel.size() == 1 && GetActiveTab().IsVirtual && icons[sel[0]].isFolder)
    {
        DWORD attrs = GetFileAttributesW(icons[sel[0]].fullPath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            LoadFiles();
            return;
        }
        NavigateToSubfolder(icons[sel[0]].wFileName);
        return;
    }

    for (int i : sel)
        OpenFile(i);
}

// ============================================================================
// Shell verbs
// ============================================================================

namespace
{
    /// Absolute PIDL for a corral icon, or nullptr if it cannot be resolved.
    LPITEMIDLIST ParseIconPidl(const CorralIcon &icon)
    {
        std::wstring parseName = icon.isSpecialIcon ? (L"::" + icon.clsid) : icon.fullPath;
        if (parseName.empty())
            return nullptr;
        LPITEMIDLIST pidl = nullptr;
        if (FAILED(SHParseDisplayName(parseName.c_str(), nullptr, &pidl, 0, nullptr)))
            return nullptr;
        return pidl;
    }

    /// Builds the menu (some handlers need it) and invokes a verb by name.
    void InvokeVerb(HWND hwnd, IContextMenu *menu, const char *verb)
    {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu)
            return;

        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        UINT flags = CMF_NORMAL | (shift ? CMF_EXTENDEDVERBS : 0u);
        if (SUCCEEDED(menu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, flags)))
        {
            CMINVOKECOMMANDINFO ci = {};
            ci.cbSize = sizeof(ci);
            ci.hwnd = hwnd;
            ci.lpVerb = verb;
            ci.nShow = SW_SHOWNORMAL;
            // The shell reads the modifiers itself (Shift+Delete = permanent).
            if (shift)
                ci.fMask |= CMIC_MASK_SHIFT_DOWN;
            if (ctrl)
                ci.fMask |= CMIC_MASK_CONTROL_DOWN;
            menu->InvokeCommand(&ci);
        }
        DestroyMenu(hMenu);
    }
} // namespace

void CorralWindow::InvokeVerbOnSelection(const char *verb)
{
    std::vector<int> sel = GetSelectedIndices();
    if (sel.empty())
        return;

    // Group by parent folder: GetUIObjectOf takes a PIDL array, but every PIDL in it
    // must be a child of the same folder. A corral tab can mix user desktop, public
    // desktop and virtual shell items, so more than one group is normal.
    std::vector<std::pair<LPITEMIDLIST, std::vector<LPITEMIDLIST>>> groups;
    for (int i : sel)
    {
        LPITEMIDLIST full = ParseIconPidl(icons[i]);
        if (!full)
            continue; // No resolvable PIDL — skip it rather than guess

        LPITEMIDLIST parent = ILClone(full);
        if (!parent)
        {
            CoTaskMemFree(full);
            continue;
        }
        ILRemoveLastID(parent);

        auto it = std::find_if(groups.begin(), groups.end(),
                               [&](const std::pair<LPITEMIDLIST, std::vector<LPITEMIDLIST>> &g)
                               { return ILIsEqual(g.first, parent) != FALSE; });
        if (it == groups.end())
        {
            groups.emplace_back(parent, std::vector<LPITEMIDLIST>{full});
        }
        else
        {
            it->second.push_back(full);
            CoTaskMemFree(parent);
        }
    }

    if (groups.empty())
        return;

    // Shell dialogs (delete confirmation, properties) need a foreground owner; the
    // corral is normally pinned to the bottom, so restore that afterwards.
    SetForegroundWindow(hwnd);

    for (auto &group : groups)
    {
        IShellFolder *folder = nullptr;
        if (SUCCEEDED(SHBindToObject(nullptr, group.first, nullptr, IID_IShellFolder, (void **)&folder)) && folder)
        {
            std::vector<LPCITEMIDLIST> children;
            for (LPITEMIDLIST full : group.second)
                children.push_back(ILFindLastID(full));

            IContextMenu *menu = nullptr;
            if (SUCCEEDED(folder->GetUIObjectOf(hwnd, (UINT)children.size(), children.data(),
                                                IID_IContextMenu, nullptr, (void **)&menu)) &&
                menu)
            {
                InvokeVerb(hwnd, menu, verb);
                menu->Release();
            }
            folder->Release();
        }

        for (LPITEMIDLIST full : group.second)
            CoTaskMemFree(full);
        CoTaskMemFree(group.first);
    }

    SendToBottom();

    // Deleted entries are pruned by the reload. The folder watcher / DesktopMonitor
    // would get here eventually; this just makes it immediate.
    LoadFiles();
}

void CorralWindow::InvokeVerbOnFolder(const char *verb)
{
    // Paste targets a folder, not an item — the item menu has no paste target.
    // A virtual corral pastes into the folder it shows; a normal one to the desktop.
    std::wstring path = GetActiveTab().IsVirtual ? GetVirtualCurrentPath() : GetDesktopPath();
    if (path.empty())
        return;

    LPITEMIDLIST pidl = nullptr;
    if (FAILED(SHParseDisplayName(path.c_str(), nullptr, &pidl, 0, nullptr)) || !pidl)
        return;

    IShellFolder *folder = nullptr;
    if (SUCCEEDED(SHBindToObject(nullptr, pidl, nullptr, IID_IShellFolder, (void **)&folder)) && folder)
    {
        IContextMenu *menu = nullptr;
        if (SUCCEEDED(folder->CreateViewObject(hwnd, IID_IContextMenu, (void **)&menu)) && menu)
        {
            SetForegroundWindow(hwnd);
            InvokeVerb(hwnd, menu, verb);
            SendToBottom();
            menu->Release();
        }
        folder->Release();
    }
    CoTaskMemFree(pidl);

    LoadFiles();
}

// ============================================================================
// Rubber-band selection
// ============================================================================

void CorralWindow::StartRubberBand(int x, int y)
{
    isRubberBanding = true;
    rubberBandStart = {x, y + scrollPosition};
    rubberBandCurrent = rubberBandStart;
    // Ctrl keeps what was already selected and adds to it.
    if (GetKeyState(VK_CONTROL) & 0x8000)
    {
        rubberBandBase = selection;
    }
    else
    {
        rubberBandBase.clear();
        ClearSelection();
    }
    SetCapture(hwnd);
}

void CorralWindow::DoRubberBand(int x, int y)
{
    rubberBandCurrent = {x, y + scrollPosition};

    RECT band = GetRubberBandRect();
    selection = rubberBandBase;
    for (int i = 0; i < (int)icons.size(); i++)
    {
        RECT dummy;
        if (IntersectRect(&dummy, &band, &icons[i].rect))
            selection.insert(i);
    }
    if (selection.empty())
        selectedIcon = -1;
    UpdateLayeredContent();
}

void CorralWindow::EndRubberBand()
{
    isRubberBanding = false;
    ReleaseCapture();
    if (!selection.empty())
    {
        selectedIcon = *selection.begin();
        selectionAnchor = selectedIcon;
    }
    UpdateLayeredContent();
}

RECT CorralWindow::GetRubberBandRect() const
{
    RECT r;
    r.left = (std::min)(rubberBandStart.x, rubberBandCurrent.x);
    r.right = (std::max)(rubberBandStart.x, rubberBandCurrent.x);
    r.top = (std::min)(rubberBandStart.y, rubberBandCurrent.y);
    r.bottom = (std::max)(rubberBandStart.y, rubberBandCurrent.y);
    return r;
}
