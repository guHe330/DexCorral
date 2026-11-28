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
 * CorralWindow.cpp - Core window class implementation
 *
 * Implements the main CorralWindow class which manages layered windows with per-pixel
 * alpha blending, icon grid layout, drag-drop, and tab switching.
 * Handles window lifecycle, WindowProc message routing, tab management, and
 * configuration persistence. Window logic is split across multiple files:
 * CorralWindowRender.cpp (drawing), CorralWindowIcons.cpp (icon loading/drawing),
 * CorralWindowInput.cpp (input handling), CorralWindowDialogs.cpp (settings dialogs),
 * and CorralWindowCommands.cpp (context menu actions).
 */

#include "CorralWindow.h"
#include "Constants.h"
#include "App.h"
#include "DesktopIcons.h"
#include "FolderWatcher.h"
#include "IconUtils.h"
#include "HookBridge.h"
#include "Presets.h"
#include "Strings.h"
#include "../resources/resource.h"
#include <windowsx.h>
#include <ShlObj.h>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <cmath>

#pragma comment(lib, "msimg32.lib")

static const wchar_t *CORRAL_WINDOW_CLASS = L"DexCorralWindowClass";


// ============================================================================
// String conversion utilities (used across all implementation files)
// ============================================================================

// Kept under these names for the many callers; the conversion itself is in IconUtils
std::string CorralWindow::WideToUtf8(const std::wstring &wide)
{
    return IconUtils::WideToUtf8(wide);
}

std::wstring CorralWindow::Utf8ToWide(const std::string &utf8)
{
    return IconUtils::Utf8ToWide(utf8);
}

// ============================================================================
// Construction / Destruction
// ============================================================================

CorralWindow::CorralWindow(const CorralWindowConfig &cfg, HWND demoOwner)
    : config(cfg), isDragging(false), hwnd(nullptr)
{
    isDemo = (demoOwner != nullptr);

    // Ensure at least one tab exists
    if (config.Tabs.empty())
    {
        CorralTabConfig tab;
        tab.Title = WideToUtf8(Tr(Str::Name_NewTab));
        config.Tabs.push_back(tab);
    }
    // Clamp active tab index
    if (config.ActiveTabIndex < 0 || config.ActiveTabIndex >= (int)config.Tabs.size())
    {
        config.ActiveTabIndex = 0;
    }

    dragStart = {0, 0};
    dragStartRect = {0, 0, 0, 0};
    currentOpacity = (config.IconOpacity < 5) ? 5 : config.IconOpacity;
    currentTintStrength = config.IconTintStrength;
    config.HeaderOpacity = ChromeAlpha::ClampHeaderOpacity(config.HeaderOpacity);
    config.BorderOpacity = ChromeAlpha::ClampBorderOpacity(config.BorderOpacity);
    currentHeaderOpacity = config.HeaderOpacity;
    currentBorderOpacity = config.BorderOpacity;

    // Save the full height for roll-up restore
    savedHeight = config.Height;

    // Get icon size and spacing based on view mode (of active tab)
    iconSize = GetIconSizeForViewMode();
    UpdateIconSpacingForViewMode();

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = CORRAL_WINDOW_CLASS;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.style = CS_DBLCLKS;
        wc.hIcon = LoadIconW(HookBridge::GetDllModule(), MAKEINTRESOURCE(IDI_APPICON));
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    std::wstring wtitle = Utf8ToWide(GetActiveTab().Title);

    // If rolled up, create with title bar height; otherwise use full height
    int initialHeight = config.IsRolledUp ? GetTitleBarHeight() : (int)config.Height;

    // Set Progman (desktop) as owner so Win+D treats corrals as part of the desktop.
    // Owned popups of Progman are immune to Show Desktop minimize/hide.
    HWND progman = FindWindowW(L"Progman", nullptr);

    hwnd = CreateWindowExW(
        // The demo must not take the activation from its settings window, and
        // is looked at, never used: no mouse, no keyboard, no drops
        WS_EX_TOOLWINDOW | WS_EX_LAYERED | (isDemo ? WS_EX_NOACTIVATE : 0),
        CORRAL_WINDOW_CLASS,
        wtitle.c_str(),
        WS_POPUP | (isDemo ? WS_DISABLED : WS_VISIBLE), // Show() shows the demo, without activating it
        (int)config.Left, (int)config.Top,
        (int)config.Width, initialHeight,
        isDemo ? demoOwner : progman, nullptr, GetModuleHandleW(nullptr), this);

    if (!isDemo)
    {
        // Register OLE drop target (replaces WS_EX_ACCEPTFILES for richer drop support)
        dropTarget = new CorralDropTarget(this);
        RegisterDragDrop(hwnd, dropTarget);
    }

    // initialHeight above was computed before the window existed, so
    // GetTitleBarHeight() could not apply per-monitor DPI scaling (Dpi() returns
    // the logical value when hwnd is null). Now that the window exists and is
    // associated with its monitor, re-apply the correctly scaled rolled-up height
    // so a freshly created minimized corral isn't clipped on high-DPI displays.
    FitRolledUpHeight();

    // Sync config from actual window position/size (in case Windows adjusted it)
    SyncConfigFromWindow();

    // Initialize folder watcher for virtual corrals (if active tab is virtual)
    if (GetActiveTab().IsVirtual)
    {
        InitializeFolderWatcher();
    }
}

CorralWindow::~CorralWindow()
{
    // Drop any App-side references to this window (band top, hover-expand owner)
    if (App::GetInstance())
        App::GetInstance()->ForgetCorral(this);

    // Stop folder watcher
    if (folderWatcher)
    {
        folderWatcher->Stop();
        folderWatcher.reset();
    }
    ClearIcons();
    if (hwnd)
    {
        if (dropTarget)
            RevokeDragDrop(hwnd);
        DestroyWindow(hwnd);
    }
    if (dropTarget)
    {
        dropTarget->Release();
        dropTarget = nullptr;
    }
}

// ============================================================================
// DPI scaling
// ============================================================================

int CorralWindow::Dpi(int logicalPixels) const
{
    if (!hwnd)
        return logicalPixels;
    int dpi = (int)GetDpiForWindow(hwnd);
    if (dpi <= 0)
        dpi = 96;
    return MulDiv(logicalPixels, dpi, 96);
}

// ============================================================================
// Static helpers
// ============================================================================

int CorralWindow::GetDesktopIconSize()
{
    // Read icon size from registry
    // HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\Shell\Bags\1\Desktop\IconSize
    HKEY hKey;
    DWORD iconSizeValue = 48; // Default medium icons

    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\Shell\\Bags\\1\\Desktop",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        DWORD size = sizeof(DWORD);
        DWORD type = REG_DWORD;
        RegQueryValueExW(hKey, L"IconSize", nullptr, &type, (LPBYTE)&iconSizeValue, &size);
        RegCloseKey(hKey);
    }

    // Clamp to reasonable values
    if (iconSizeValue < 16)
        iconSizeValue = 16;
    if (iconSizeValue > MAX_ICON_SIZE)
        iconSizeValue = MAX_ICON_SIZE;

    return (int)iconSizeValue;
}

void CorralWindow::GetDesktopIconSpacing(int &spacingX, int &spacingY)
{
    // Query the desktop ListView's actual icon spacing directly.
    // LVM_GETITEMSPACING returns physical pixels (desktop is DPI-aware).
    HWND hListView = DesktopIcons::GetDesktopListView();
    if (hListView)
    {
        DWORD spacing = (DWORD)SendMessageW(hListView, LVM_GETITEMSPACING, FALSE, 0);
        int desktopSpacingX = LOWORD(spacing);
        int desktopSpacingY = HIWORD(spacing);

        if (desktopSpacingX > 0 && desktopSpacingY > 0)
        {
            // GetDesktopIconSize() returns logical pixels; scale it with the ListView's
            // own DPI (not this Corral's) so the ratio below is DPI-free.
            int desktopIconSizePhysical = MulDiv(GetDesktopIconSize(), (int)GetDpiForWindow(hListView), 96);
            if (desktopIconSizePhysical > 0)
            {
                // Scale proportionally: keep the same ratio as the desktop grid
                spacingX = desktopSpacingX * iconSize / desktopIconSizePhysical;
                spacingY = desktopSpacingY * iconSize / desktopIconSizePhysical;

                // Ensure minimum label space for 2 lines of text
                int minLabel = Dpi(40);
                if (spacingY < iconSize + minLabel)
                    spacingY = iconSize + minLabel;
                if (spacingX < iconSize + Dpi(32))
                    spacingX = iconSize + Dpi(32);
                return;
            }
        }
    }

    // Fallback: reasonable defaults
    spacingX = iconSize + Dpi(32);
    spacingY = iconSize + Dpi(40);
}

std::wstring CorralWindow::GetDesktopPath()
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path)))
    {
        return std::wstring(path);
    }
    return L"";
}

std::wstring CorralWindow::GetPublicDesktopPath()
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, path)))
    {
        return std::wstring(path);
    }
    return L"";
}

// ============================================================================
// Public methods
// ============================================================================

void CorralWindow::PinToBandTop()
{
    if (!hwnd || isDemo) // The demo stays above its settings window
        return;

    if (App *app = App::GetInstance())
        app->RepinBand(this);
    else
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

int CorralWindow::MessageBoxBehind(const wchar_t *body, const wchar_t *title, UINT flags)
{
    // The box is owned by this corral. Windows keeps an owner right behind its
    // active popup, and would lift this corral and the others with it.
    blockRaise++;
    const int answer = MessageBoxW(hwnd, body, title, flags);
    blockRaise--;
    PinToBandTop(); // Whatever moved anyway goes back
    return answer;
}

void CorralWindow::ActivateInPlace(bool foreground)
{
    blockRaise++;
    if (foreground)
        SetForegroundWindow(hwnd);
    else
        SetFocus(hwnd);
    blockRaise--;
    PinToBandTop(); // Whatever moved anyway goes back, and this corral leads the band
}

void CorralWindow::Show()
{
    // Paused by the user: corrals exist but stay hidden until App::SetPaused(false)
    if (App *app = App::GetInstance(); app && app->IsPaused() && !isDemo)
        return;

    if (hwnd)
    {
        // Showing through the normal path cancels any quick-hide state
        if (isQuickHideAnimating)
        {
            KillTimer(hwnd, QUICKHIDE_TIMER_ID);
            isQuickHideAnimating = false;
        }
        isQuickHidden = false;
        quickHideAlpha = 255;

        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        PinToBandTop();

        if (GetActiveTab().IsVirtual)
        {
            // For virtual tabs, show window immediately and load icons asynchronously
            UpdateLayeredContent();
            PostMessageW(hwnd, WM_DEFERRED_LOAD, 0, 0);
        }
        else
        {
            LoadFiles(); // This calls UpdateLayeredContent
        }
    }
}

void CorralWindow::ApplyLook(const AppearanceSettings &look)
{
    Presets::ApplyLook(config, look);
    // What is drawn right now follows the hover fade; start it from the new look
    SetCurrentHeaderOpacity(config.HeaderOpacity);
    SetCurrentBorderOpacity(config.BorderOpacity);
    SetCurrentOpacity(config.IconOpacity);
    SetCurrentTintStrength(config.IconTintStrength);
    if (!isHoverExpanded && !isAnimating)
        FitRolledUpHeight(); // The header height may have changed
    RecalculateLayout();
}

void CorralWindow::Hide()
{
    if (hwnd)
    {
        allowHide = true;
        ShowWindow(hwnd, SW_HIDE);
        allowHide = false;
    }
}

void CorralWindow::SyncConfigFromWindow(bool userPlaced)
{
    if (hwnd)
    {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        config.Left = rect.left;
        config.Top = rect.top;
        config.Width = rect.right - rect.left;
        // When rolled up, preserve the saved full height in config
        if (!config.IsRolledUp)
        {
            config.Height = rect.bottom - rect.top;
            savedHeight = config.Height;
        }
        // When rolled up, config.Height keeps the savedHeight value

        // Update monitor position tracking
        App *app = App::GetInstance();
        if (app && app->GetMonitorManager())
        {
            MonitorManager *monMgr = app->GetMonitorManager();
            const MonitorInfo *mon = monMgr->FindMonitorForRect(rect);
            // Only the user arranges. A move made for the Corral (its monitor
            // went away, Windows relocated the window, a layout was applied)
            // leaves the layouts alone, or the Corral could never return to
            // where it was put. A Corral without any layout is new and
            // records where it was created.
            if (mon && (userPlaced || config.Layouts.empty()))
            {
                // Relative to the monitor's origin
                MonitorPosition pos;
                pos.MonitorId = mon->deviceId;
                pos.Left = (int)config.Left - mon->bounds.left;
                pos.Top = (int)config.Top - mon->bounds.top;
                pos.Width = (int)config.Width;
                pos.Height = (int)(config.IsRolledUp ? savedHeight : config.Height);
                pos.RefWidth = mon->width;
                pos.RefHeight = mon->height;
                pos.Dpi = (int)GetDpiForWindow(hwnd);
                Config::StoreLayout(config, monMgr->GetTopologyKey(), pos);
            }
        }
    }
}

void CorralWindow::FitRolledUpHeight()
{
    if (!config.IsRolledUp)
        return;
    int rolledHeight = GetTitleBarHeight();
    RECT r;
    GetWindowRect(hwnd, &r);
    if ((r.bottom - r.top) != rolledHeight)
    {
        SetWindowPos(hwnd, nullptr, 0, 0, (int)config.Width, rolledHeight,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CorralWindow::MoveTo(int left, int top, int width, int height)
{
    config.Left = left;
    config.Top = top;
    config.Width = width;
    config.Height = height;
    savedHeight = height;

    // The size was worked out for the monitor the Corral lands on. If that
    // monitor has another DPI, OnDpiChanged must not scale it a second time.
    isBeingPlaced = true;
    SetWindowPos(hwnd, nullptr, left, top, width,
                 config.IsRolledUp ? GetTitleBarHeight() : height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    isBeingPlaced = false;

    // The title bar height above was still that of the monitor it came from
    FitRolledUpHeight();
}

void CorralWindow::LoadFiles()
{
    // A reload (folder change, desktop change, F5) keeps what the user selected
    const SelectionByFile selected = SaveSelectionByFile();
    if (GetActiveTab().IsVirtual)
    {
        LoadVirtualFolderIcons();
    }
    else
    {
        LoadIconImages();
    }
    RestoreSelectionByFile(selected);
    CalculateIconLayout();
    UpdateLayeredContent();
}

// ============================================================================
// Tab Management
// ============================================================================

CorralTabConfig &CorralWindow::GetActiveTab()
{
    if (config.ActiveTabIndex < 0 || config.ActiveTabIndex >= (int)config.Tabs.size())
    {
        config.ActiveTabIndex = 0;
        if (config.Tabs.empty())
        {
            config.Tabs.push_back(CorralTabConfig());
        }
    }
    return config.Tabs[config.ActiveTabIndex];
}

const CorralTabConfig &CorralWindow::GetActiveTab() const
{
    if (config.ActiveTabIndex < 0 || config.ActiveTabIndex >= (int)config.Tabs.size())
    {
        if (config.Tabs.empty())
        {
            static CorralTabConfig empty;
            return empty;
        }
        return config.Tabs[0];
    }
    return config.Tabs[config.ActiveTabIndex];
}

void CorralWindow::SetActiveTab(int index)
{
    if (index >= 0 && index < (int)config.Tabs.size() && index != config.ActiveTabIndex)
    {
        config.ActiveTabIndex = index;
        ReloadActiveTab();

        if (App::GetInstance())
        {
            App::GetInstance()->SaveConfig();
        }
    }
}

void CorralWindow::ReloadActiveTab()
{
    // Everything that belongs to the tab shown before must go: its folder
    // watchers, its icon size and spacing, its scroll position, its icons.
    // Also needed when the active index stays the same but the tab behind it
    // is another one (a tab was closed or detached).
    // What is known about a share belongs to the tab shown before, even if the
    // new one shows the same share: it is asked first, and nothing waits for it.
    netProbeRoot.clear();
    netState = NetState::Unknown;
    netRetryMs = NET_PROBE_MS;
    reloadWhenReachable = false;
    netPending = PendingAction();

    InitializeFolderWatcher(); // Drops the old watchers; starts new ones for a virtual tab

    iconSize = GetIconSizeForViewMode();
    UpdateIconSpacingForViewMode();

    scrollPosition = 0;
    ClearSelection();
    LoadFiles();
}

void CorralWindow::AddTab(const CorralTabConfig &tab)
{
    config.Tabs.push_back(tab);
    SetActiveTab((int)config.Tabs.size() - 1);
}

void CorralWindow::DetachTab(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= (int)config.Tabs.size())
        return;
    if (config.Tabs.size() <= 1)
        return; // Can't detach the only tab

    // Copy the tab to detach
    CorralTabConfig tabToDetach = config.Tabs[tabIndex];

    // Remove from this window first
    config.Tabs.erase(config.Tabs.begin() + tabIndex);

    // Adjust active index if needed
    if (config.ActiveTabIndex >= (int)config.Tabs.size())
    {
        config.ActiveTabIndex = (int)config.Tabs.size() - 1;
    }
    else if (config.ActiveTabIndex > tabIndex)
    {
        config.ActiveTabIndex--;
    }

    // Reload this window with remaining tabs
    ReloadActiveTab();

    // Create new window for the detached tab, seeded at a +30 cascade off the
    // source's top-left. CreateCorralAt finds the nearest free spot from there;
    // the source is not excluded, so the detached tab moves fully clear of the
    // corral it came from rather than landing on top of it.
    RECT rect;
    GetWindowRect(hwnd, &rect);

    if (App::GetInstance())
    {
        SIZE size = App::DefaultCorralSize({rect.left, rect.top});
        POINT desiredCenter = {rect.left + 30 + size.cx / 2, rect.top + 30 + size.cy / 2};
        App::GetInstance()->CreateCorralAt(desiredCenter);

        // Get the new corral and replace its default tab with our detached tab
        const auto &corrals = App::GetInstance()->GetCorrals();
        if (!corrals.empty())
        {
            CorralWindow *newWindow = corrals.back().get();
            newWindow->GetConfig().Tabs.clear();
            newWindow->GetConfig().Tabs.push_back(tabToDetach);
            newWindow->GetConfig().ActiveTabIndex = 0;
            newWindow->ReloadActiveTab(); // Not just the icons: a virtual tab needs its folder watcher
            newWindow->UpdateLayeredContent();
        }

        App::GetInstance()->SaveConfig();
    }
}

void CorralWindow::MergeWith(CorralWindow *other)
{
    if (!other || other == this)
        return;

    // This is the drop target and survives as it is: its place, size, Lock and
    // other settings stay. Only the tabs of `other` (the dragged Corral) join it.
    const int firstMerged = (int)config.Tabs.size();
    for (const auto &tab : other->GetConfig().Tabs)
    {
        config.Tabs.push_back(tab);
    }

    // Switch to the first merged tab
    SetActiveTab(firstMerged);

    // Remove the other corral
    if (App::GetInstance())
    {
        App::GetInstance()->RemoveCorral(&other->GetConfig());
    }

    // SetActiveTab only reloads when the active index actually changes
    UpdateLayeredContent();
}

int CorralWindow::HitTestTab(int x, int y)
{
    if (y >= GetTitleBarHeight())
        return -1;

    for (int i = 0; i < (int)config.Tabs.size(); i++)
    {
        RECT tabRect = GetTabRect(i);
        POINT pt = {x, y};
        if (PtInRect(&tabRect, pt))
        {
            return i;
        }
    }
    return -1;
}

RECT CorralWindow::GetTabRect(int index) const
{
    if (index < 0 || index >= (int)config.Tabs.size())
    {
        return {0, 0, 0, 0};
    }

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int contentLeft = GetTitleBarContentLeft();
    int totalWidth = clientRect.right - contentLeft;
    if (totalWidth < 0)
        totalWidth = 0;

    int tabCount = (int)config.Tabs.size();
    int tabWidth = totalWidth / tabCount;

    return {
        contentLeft + index * tabWidth,
        0,
        contentLeft + (index + 1) * tabWidth,
        GetTitleBarHeight()};
}

RECT CorralWindow::GetTabGripRect(int index) const
{
    RECT tr = GetTabRect(index);
    return {tr.left, tr.top, tr.left + Dpi(TAB_GRIP_WIDTH), tr.bottom};
}

RECT CorralWindow::GetLockGlyphRect() const
{
    if (!config.IsLocked)
        return {0, 0, 0, 0};

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    const int barH = GetTitleBarHeight();
    int gh = barH * 55 / 100;
    if (gh < Dpi(LOCK_GLYPH_MIN_H))
        gh = Dpi(LOCK_GLYPH_MIN_H);
    if (gh > Dpi(LOCK_GLYPH_MAX_H))
        gh = Dpi(LOCK_GLYPH_MAX_H);
    if (gh > barH - 2)
        gh = barH - 2; // Never taller than the bar it sits in
    if (gh < 4)
        return {0, 0, 0, 0};

    const int gw = gh * 78 / 100;
    const int right = clientRect.right - Dpi(LOCK_GLYPH_MARGIN);
    const int top = (barH - gh) / 2;
    if (right - gw <= 0)
        return {0, 0, 0, 0}; // Corral too narrow to show it

    return {right - gw, top, right, top + gh};
}

int CorralWindow::HitTestTabGrip(int x, int y) const
{
    // Grips only make sense (and only appear) when there's more than one tab.
    if (config.Tabs.size() < 2 || y < 0 || y >= GetTitleBarHeight())
        return -1;

    // Only the tab that is actually showing its grip can be grabbed by it. The grip
    // is drawn on the hovered (or dragged) tab alone, so testing every tab made the
    // left edge of each one grab-able with nothing there to see — turning a plain
    // tab click into an accidental reorder.
    if (hoveredTab < 0 || hoveredTab >= (int)config.Tabs.size())
        return -1;

    RECT g = GetTabGripRect(hoveredTab);
    POINT pt = {x, y};
    return PtInRect(&g, pt) ? hoveredTab : -1;
}

void CorralWindow::MoveTab(int from, int to)
{
    int n = (int)config.Tabs.size();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;

    CorralTabConfig moved = config.Tabs[from];
    config.Tabs.erase(config.Tabs.begin() + from);
    config.Tabs.insert(config.Tabs.begin() + to, moved);

    // Keep ActiveTabIndex pointing at the same logical tab after the shuffle.
    int active = config.ActiveTabIndex;
    if (active == from)
        active = to;
    else if (from < active && to >= active)
        active--;
    else if (from > active && to <= active)
        active++;
    config.ActiveTabIndex = active;
}

void CorralWindow::OnTabDrag(int x, int y)
{
    if (draggedTabIndex < 0)
        return;

    // Choose the target slot by which tab midpoints the cursor has crossed.
    // Y is ignored so the user can wander vertically without dropping the drag.
    int n = (int)config.Tabs.size();
    int target = draggedTabIndex;
    for (int i = 0; i < n; i++)
    {
        RECT tr = GetTabRect(i);
        int mid = (tr.left + tr.right) / 2;
        if (i < draggedTabIndex && x < mid)
        {
            target = i; // Leftmost tab whose midpoint we've passed
            break;
        }
        if (i > draggedTabIndex && x > mid)
        {
            target = i; // Keep extending right past each crossed midpoint
        }
    }

    if (target != draggedTabIndex)
    {
        MoveTab(draggedTabIndex, target);
        draggedTabIndex = target;
        UpdateLayeredContent();
    }
}

// ============================================================================
// Virtual corral navigation + details-view geometry helpers
// ============================================================================

bool CorralWindow::IsNavBackVisible() const
{
    const CorralTabConfig &tab = GetActiveTab();
    return tab.IsVirtual && !tab.CurrentSubPath.empty();
}

int CorralWindow::GetTitleBarContentLeft() const
{
    return IsNavBackVisible() ? GetTitleBarHeight() : 0;
}

RECT CorralWindow::GetNavBackButtonRect() const
{
    if (!IsNavBackVisible())
        return {0, 0, 0, 0};
    int h = GetTitleBarHeight();
    return {0, 0, h, h};
}

int CorralWindow::GetDetailsHeaderHeight() const
{
    const CorralTabConfig &tab = GetActiveTab();
    if (tab.IsVirtual && tab.GetViewMode() == ViewMode::Details)
        return Dpi(DETAILS_HEADER_HEIGHT);
    return 0;
}

std::wstring CorralWindow::GetVirtualCurrentPath() const
{
    const CorralTabConfig &tab = GetActiveTab();
    std::wstring root = Utf8ToWide(tab.VirtualFolderPath);
    if (tab.CurrentSubPath.empty())
        return root;
    std::wstring sub = Utf8ToWide(tab.CurrentSubPath);
    if (!root.empty() && root.back() != L'\\')
        root += L'\\';
    return root + sub;
}

void CorralWindow::NavigateToSubfolder(const std::wstring &folder)
{
    if (folder.empty())
        return;

    CorralTabConfig &tab = GetActiveTab();
    if (!tab.CurrentSubPath.empty() && tab.CurrentSubPath.back() != '\\')
        tab.CurrentSubPath += '\\';
    tab.CurrentSubPath += WideToUtf8(folder);

    scrollPosition = 0;
    ClearSelection();
    InitializeFolderWatcher();
    LoadFiles();
    SyncConfigFromWindow();
    if (App::GetInstance())
        App::GetInstance()->SaveConfig();
}

void CorralWindow::NavigateUp()
{
    CorralTabConfig &tab = GetActiveTab();
    if (tab.CurrentSubPath.empty())
        return;
    if (!ShareJustAnswered())
    {
        WhenShareAnswers([this]() { NavigateUp(); });
        return;
    }

    // Strip trailing separators then the last path component
    std::string sub = tab.CurrentSubPath;
    while (!sub.empty() && (sub.back() == '\\' || sub.back() == '/'))
        sub.pop_back();
    size_t slash = sub.find_last_of("\\/");
    sub = (slash == std::string::npos) ? std::string() : sub.substr(0, slash);
    tab.CurrentSubPath = sub;

    scrollPosition = 0;
    ClearSelection();
    InitializeFolderWatcher();
    LoadFiles();
    SyncConfigFromWindow();
    if (App::GetInstance())
        App::GetInstance()->SaveConfig();
}

// ============================================================================
// Window procedure
// ============================================================================

LRESULT CALLBACK CorralWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CorralWindow *window = nullptr;

    if (uMsg == WM_CREATE)
    {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        window = (CorralWindow *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    else
    {
        window = (CorralWindow *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (window)
    {
        switch (uMsg)
        {
        case WM_PAINT:
            window->OnPaint();
            return 0;
        case WM_SIZE:
            window->OnSize();
            return 0;
        case WM_MOVE:
            window->OnMove();
            return 0;
        case WM_DPICHANGED:
            window->OnDpiChanged(HIWORD(wParam), (const RECT *)lParam);
            return 0;
        case WM_LBUTTONDOWN:
            window->OnLeftButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
            window->OnLeftButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONDBLCLK:
            window->OnLeftButtonDblClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_RBUTTONDOWN:
            window->OnRightButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEMOVE:
        {
            // Recover from a lost button-up: a corral is a background window, so a
            // release that happens while the cursor is off the corral never reaches
            // us and the drag or resize would otherwise run forever.
            //
            // Both the message's button flags and the live button state have to agree
            // before an operation is ended. wParam alone is not enough: a synthesized
            // or re-routed move can arrive with no MK_LBUTTON while the user is very
            // much still holding the button, and ending on that cancels the drag out
            // from under them.
            if (!(wParam & MK_LBUTTON) && !(GetKeyState(VK_LBUTTON) & 0x8000) &&
                window->HasCapturedOperation())
            {
                window->EndCapturedOperationWithoutDrop();
            }

            // Track mouse for hover-expand
            if (!window->mouseInsideWindow)
            {
                window->mouseInsideWindow = true;
                window->OnPointerEntered();
                TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);

                // Hover-expand only after a dwell, and only if no other corral
                // is already expanded (arbitrated by App).
                if (window->config.IsRolledUp && !window->isHoverExpanded && !window->isAnimating)
                {
                    SetTimer(hwnd, HOVER_DWELL_TIMER_ID, HOVER_DWELL_DELAY, nullptr);
                }

                // Fade icons, header and border to full and remove tint on hover
                window->StartHoverFade(true);
            }

            // Check scrollbar hover state (PowerShell-style expand on hover)
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            bool wasHovered = window->isScrollbarHovered;

            // Detect hover over scrollbar region (wider detection for easier hover)
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int scrollbarRegionLeft = clientRect.right - window->Dpi(SCROLLBAR_WIDTH) - window->Dpi(SCROLLBAR_MARGIN) * 2;
            bool isOverScrollbarRegion = window->NeedsScrollbar() &&
                                         x >= scrollbarRegionLeft &&
                                         y >= window->GetIconAreaTop();

            // Mid-drag the bar stays wide: collapsing it changes the thumb's scale
            window->isScrollbarHovered = isOverScrollbarRegion || window->isDraggingScrollbar;

            // Redraw if hover state changed
            if (wasHovered != window->isScrollbarHovered)
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }

            // Track hovered icon and hovered tab (the latter reveals the reorder grip)
            if (!window->isDragging && !window->isResizing && !window->isDraggingIcon &&
                !window->isDraggingScrollbar && !window->isDraggingTab && !window->isRubberBanding)
            {
                int prevHovered = window->hoveredIcon;
                window->hoveredIcon = window->HitTestIcon(x, y);
                int prevHoveredTab = window->hoveredTab;
                window->hoveredTab = (y < window->GetTitleBarHeight()) ? window->HitTestTab(x, y) : -1;
                if (prevHovered != window->hoveredIcon || prevHoveredTab != window->hoveredTab)
                {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }

            if (window->isRubberBanding)
            {
                window->DoRubberBand(x, y);
            }
            else if (window->isDraggingTab)
            {
                window->OnTabDrag(x, y);
            }
            else if (window->isResizingColumn)
            {
                window->DoColumnResize(x);
            }
            else if (window->isResizing)
            {
                POINT pt;
                GetCursorPos(&pt);
                window->DoResize(pt.x, pt.y);
            }
            else if (window->isDraggingScrollbar)
            {
                window->DoScrollbarDrag(GET_Y_LPARAM(lParam));
            }
            else if (window->draggedIconIndex >= 0)
            {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                // Check if we've moved enough to start a drag
                int dx = x - window->iconDragStart.x;
                int dy = y - window->iconDragStart.y;
                if (!window->isDraggingIcon && (abs(dx) > window->Dpi(DRAG_THRESHOLD) || abs(dy) > window->Dpi(DRAG_THRESHOLD)))
                {
                    // A virtual tab shows real files in a real folder: hand the drag
                    // to the shell, so every drop target on the system can take it.
                    // StartShellDrag only returns once the drag is over.
                    if (window->GetActiveTab().IsVirtual)
                    {
                        // The drag reads the files. Ask a share first; the next
                        // mouse move after its answer starts the drag.
                        if (!window->ShareJustAnswered())
                        {
                            window->StartNetworkProbe();
                            return 0;
                        }
                        window->StartShellDrag();
                        return 0;
                    }
                    window->isDraggingIcon = true;
                }
                if (window->isDraggingIcon)
                {
                    window->OnIconDrag(x, y);

                    // Over a window that is neither a corral nor the desktop: only
                    // a shell drag lets that window take the files.
                    POINT screenPt;
                    GetCursorPos(&screenPt);
                    App *app = App::GetInstance();
                    if (app && !app->IsPointOnCorralOrDesktop(screenPt))
                        window->StartShellDrag();
                }
            }
            else if (window->isDragging)
            {
                POINT pt;
                GetCursorPos(&pt);
                window->DoWindowDrag(pt.x, pt.y);
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
            window->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_POINTERWHEEL:
        {
            // Handle precision touchpad scrolling (Windows 8+)
            // Extract wheel delta from pointer input
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            window->OnMouseWheel(delta);
            return 0;
        }
        // WM_DROPFILES removed - now handled by IDropTarget (CorralDropTarget)
        case WM_SETCURSOR:
            // Set resize cursor based on position
            if (LOWORD(lParam) == HTCLIENT && !window->config.IsRolledUp)
            {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                // Tab reorder grip takes priority inside the title bar
                if (window->HitTestTabGrip(pt.x, pt.y) >= 0)
                {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
                    return TRUE;
                }
                // Column resize grips take priority inside the details header band
                if (window->HitTestColumnGrip(pt.x, pt.y) >= 0)
                {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                    return TRUE;
                }

                // Same rule as the click path: don't advertise a resize where a
                // click will activate a tab — or where the corral is locked and the
                // resize would be refused.
                int hit = window->config.IsLocked ? 0 : window->HitTestResizeAllowingTabs(pt.x, pt.y);
                LPCWSTR cursor = IDC_ARROW;
                switch (hit)
                {
                case HTLEFT:
                case HTRIGHT:
                    cursor = IDC_SIZEWE;
                    break;
                case HTTOP:
                case HTBOTTOM:
                    cursor = IDC_SIZENS;
                    break;
                case HTTOPLEFT:
                case HTBOTTOMRIGHT:
                    cursor = IDC_SIZENWSE;
                    break;
                case HTTOPRIGHT:
                case HTBOTTOMLEFT:
                    cursor = IDC_SIZENESW;
                    break;
                }
                SetCursor(LoadCursorW(nullptr, cursor));
                return TRUE;
            }
            break;
        case WM_KEYDOWN:
            if (window->OnKeyDown(wParam))
                return 0;
            break;
        case WM_SYSKEYDOWN:
            // Alt+Enter comes as a system key. Every other Alt key stays with
            // DefWindowProc (Alt+F4, Alt+Space).
            if (wParam == VK_RETURN && window->OnKeyDown(wParam))
                return 0;
            break;
        case WM_SYSCHAR:
            // The character of a handled Alt+Enter: DefWindowProc would beep for it
            if (wParam == VK_RETURN)
                return 0;
            break;
        case WM_CHAR:
            window->OnChar((wchar_t)wParam);
            return 0;
        case WM_TIMER:
            if (wParam == ANIMATION_TIMER_ID)
            {
                window->OnAnimationTimer();
                return 0;
            }
            if (wParam == HOVER_CHECK_TIMER_ID)
            {
                window->OnHoverCheckTimer();
                return 0;
            }
            if (wParam == HOVER_DWELL_TIMER_ID)
            {
                KillTimer(hwnd, HOVER_DWELL_TIMER_ID);
                POINT cur;
                RECT wr;
                GetCursorPos(&cur);
                GetWindowRect(hwnd, &wr);
                if (window->mouseInsideWindow && PtInRect(&wr, cur) &&
                    window->config.IsRolledUp && !window->isHoverExpanded && !window->isAnimating)
                {
                    window->StartHoverExpand();
                }
                return 0;
            }
            if (wParam == OPACITY_TIMER_ID)
            {
                window->OnOpacityAnimationTimer();
                return 0;
            }
            if (wParam == QUICKHIDE_TIMER_ID)
            {
                window->OnQuickHideAnimationTimer();
                return 0;
            }
            if (wParam == RENAME_TIMER_ID)
            {
                KillTimer(hwnd, RENAME_TIMER_ID);
                int idx = window->pendingRenameIcon;
                window->pendingRenameIcon = -1;
                // Only rename if the icon is still the selected one (no double-click,
                // no reselection, no folder reload happened in the meantime).
                if (idx >= 0 && idx == window->selectedIcon && idx < (int)window->icons.size())
                {
                    window->StartIconRename(idx);
                }
                return 0;
            }
            if (wParam == FOLDER_RELOAD_TIMER_ID)
            {
                KillTimer(hwnd, FOLDER_RELOAD_TIMER_ID);
                window->OnFolderContentsChanged();
                return 0;
            }
            if (wParam == DESKTOP_RETRY_TIMER_ID)
            {
                KillTimer(hwnd, DESKTOP_RETRY_TIMER_ID);
                if (!window->GetActiveTab().IsVirtual || window->virtualFolderMissing)
                    window->LoadFiles(); // Arms the timer again if the folder still does not answer
                return 0;
            }
            if (wParam == NET_PROBE_TIMER_ID)
            {
                if (window->GetActiveTab().IsVirtual &&
                    IsNetworkFolder(Utf8ToWide(window->GetActiveTab().VirtualFolderPath)))
                    window->StartNetworkProbe();
                else
                {
                    // Left the network tab: nobody keeps the answer fresh, so it must not be trusted later
                    KillTimer(hwnd, NET_PROBE_TIMER_ID);
                    window->netState = NetState::Unknown;
                    window->netProbeRoot.clear();
                }
                return 0;
            }
            if (wParam == TYPEAHEAD_TIMER_ID)
            {
                window->OnTypeAheadTimeout();
                return 0;
            }
            if (wParam == CURSOR_TRACK_TIMER_ID)
            {
                window->OnCursorTrackTimer();
                return 0;
            }
            if (wParam == SCROLL_REPOSITION_TIMER_ID)
            {
                KillTimer(hwnd, SCROLL_REPOSITION_TIMER_ID);
                // Reposition desktop icons to match scrolled positions
                App *app = App::GetInstance();
                if (app)
                {
                    app->PositionHiddenIconsUnderCorrals();
                }
                return 0;
            }
            break;
        case WM_FOLDER_CHANGED:
            // Reload once the folder has been quiet for a moment: restarting
            // the timer on every event turns a burst (an archive being
            // extracted) into one reload that sees all of it.
            SetTimer(hwnd, FOLDER_RELOAD_TIMER_ID, FOLDER_RELOAD_DELAY_MS, nullptr);
            return 0;
        case WM_NET_PROBE_DONE:
            window->OnNetworkProbeDone((LONG)lParam, wParam != 0);
            return 0;
        case WM_DEFERRED_LOAD:
            // Deferred icon loading for virtual corrals (keeps UI responsive)
            window->LoadFiles();
            return 0;
        case WM_MOUSELEAVE:
            window->mouseInsideWindow = false;
            KillTimer(hwnd, HOVER_DWELL_TIMER_ID);
            // Reset hovered icon
            if (window->hoveredIcon >= 0)
            {
                window->hoveredIcon = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            // Reset hovered tab so the reorder grip disappears
            if (window->hoveredTab >= 0)
            {
                window->hoveredTab = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            // Reset scrollbar hover state
            if (window->isScrollbarHovered)
            {
                window->isScrollbarHovered = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            // Start collapse if hover-expanded
            if (window->isHoverExpanded && !window->isAnimating)
            {
                window->StartHoverCollapse();
            }
            // Fade back to the configured icon, header and border opacity
            window->StartHoverFade(false);
            return 0;
        case WM_WINDOWPOSCHANGING:
        {
            // Prevent Show Desktop from hiding corral windows.
            // Primary defense is Progman ownership (set at creation), but this
            // strips SWP_HIDEWINDOW as a fallback for edge cases.
            auto *wp = reinterpret_cast<WINDOWPOS *>(lParam);
            if (wp && (wp->flags & SWP_HIDEWINDOW) && !window->allowHide)
            {
                wp->flags &= ~SWP_HIDEWINDOW;
            }
            // Our own activation must not lift a corral, not even for one frame
            if (wp && blockRaise && wp->hwndInsertAfter != HWND_BOTTOM)
                wp->flags |= SWP_NOZORDER;
            return 0;
        }
        case WM_CAPTURECHANGED:
            // DexCorral's UI lives inside Explorer's process, where any other component
            // can take the capture out from under us.
            //
            // Losing capture is not on its own a reason to stop: corrals are never the
            // foreground window (MA_NOACTIVATE), and Windows hands background windows a
            // weaker capture that it can cancel mid-drag. Ending the operation there
            // would abort a resize the user is still performing. Only a capture loss
            // with the button already released means the button-up is never coming.
            if ((HWND)lParam != hwnd && !(GetKeyState(VK_LBUTTON) & 0x8000))
            {
                window->EndCapturedOperationWithoutDrop();
            }
            return 0;
        case WM_MOUSEACTIVATE:
            // Prevent corral from being brought to front when clicked
            return MA_NOACTIVATE;
        case WM_DESTROY:
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            KillTimer(hwnd, HOVER_CHECK_TIMER_ID);
            KillTimer(hwnd, HOVER_DWELL_TIMER_ID);
            KillTimer(hwnd, OPACITY_TIMER_ID);
            KillTimer(hwnd, SCROLL_REPOSITION_TIMER_ID);
            KillTimer(hwnd, FOLDER_RELOAD_TIMER_ID);
            window->StopCursorTracking();
            return 0;
        }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Paint - Uses UpdateLayeredWindow for true per-pixel transparency
// ============================================================================

void CorralWindow::OnPaint()
{
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);

    // The window is layered: its content goes through UpdateLayeredWindow
    UpdateLayeredContent();
}

// ============================================================================
// Event handlers
// ============================================================================

void CorralWindow::OnSize()
{
    SyncConfigFromWindow();
    CalculateIconLayout();
    UpdateLayeredContent();
}

void CorralWindow::OnDpiChanged(UINT newDpi, const RECT *suggested)
{
    // The suggested rect is the current window scaled to the new DPI. For a
    // rolled-up corral that is only the title bar, so the height it unrolls
    // to has to be scaled here.
    if (!isBeingPlaced && config.IsRolledUp && lastRenderDpi != 0 && newDpi != lastRenderDpi)
    {
        savedHeight = MulDiv((int)savedHeight, newDpi, lastRenderDpi);
        config.Height = savedHeight;
    }

    // Before SetWindowPos: its WM_SIZE and WM_MOVE already render at the new DPI
    iconSize = GetIconSizeForViewMode();
    UpdateIconSpacingForViewMode();

    // MoveTo has already chosen the size for this DPI
    if (suggested && !isBeingPlaced)
    {
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // The loaded icon images keep the resolution they were loaded at, so a
    // relayout alone would leave them at the old scale.
    LoadFiles();
}

void CorralWindow::OnMove()
{
    SyncConfigFromWindow();
    // A move changes no pixel, so there is nothing to render: this runs for
    // every step of a title-bar drag. The one exception is landing on a monitor
    // with another scale factor, where the DPI-dependent chrome has to be redrawn.
    if (GetDpiForWindow(hwnd) != lastRenderDpi)
        UpdateLayeredContent();
}
