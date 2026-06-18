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
#include <windowsx.h>
#include <ShlObj.h>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <cmath>

#pragma comment(lib, "msimg32.lib")

static const wchar_t *CORRAL_WINDOW_CLASS = L"DexCorralWindowClass";

/// Dead code: Incomplete precision touchpad support block.
/// WM_POINTERWHEEL message handling was planned but never implemented (Windows 8+).
/// Pointer input is currently not supported; mouse wheel events are handled instead.
// #ifndef WM_POINTERWHEEL
// #endif

// ============================================================================
// String conversion utilities (used across all implementation files)
// ============================================================================

std::string CorralWindow::WideToUtf8(const std::wstring &wide)
{
    if (wide.empty())
        return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring CorralWindow::Utf8ToWide(const std::string &utf8)
{
    if (utf8.empty())
        return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

CorralWindow::CorralWindow(const CorralWindowConfig &cfg)
    : config(cfg), isDragging(false), hwnd(nullptr)
{

    // Ensure at least one tab exists
    if (config.Tabs.empty())
    {
        CorralTabConfig tab;
        tab.Title = "New Tab";
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
        WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CORRAL_WINDOW_CLASS,
        wtitle.c_str(),
        WS_POPUP | WS_VISIBLE,
        (int)config.Left, (int)config.Top,
        (int)config.Width, initialHeight,
        progman, nullptr, GetModuleHandleW(nullptr), this);

    // Register OLE drop target (replaces WS_EX_ACCEPTFILES for richer drop support)
    dropTarget = new CorralDropTarget(this);
    RegisterDragDrop(hwnd, dropTarget);

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
    // Stop folder watcher
    if (folderWatcher)
    {
        folderWatcher->Stop();
        folderWatcher.reset();
    }
    ClearIcons();
    if (hwnd)
    {
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

bool CorralWindow::s_enableDpiScaling = true;

int CorralWindow::Dpi(int logicalPixels) const
{
    if (!s_enableDpiScaling || !hwnd)
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
            // GetDesktopIconSize() returns logical pixels; Dpi() scales to physical.
            // Both desktopSpacing and iconSize are now in physical pixels.
            int desktopIconSizePhysical = Dpi(GetDesktopIconSize());
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

void CorralWindow::SendToBottom()
{
    if (hwnd)
    {
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void CorralWindow::Show()
{
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
        SendToBottom();

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

void CorralWindow::Hide()
{
    if (hwnd)
    {
        allowHide = true;
        ShowWindow(hwnd, SW_HIDE);
        allowHide = false;
    }
}

void CorralWindow::SyncConfigFromWindow()
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
            if (mon)
            {
                // Update target monitor ID
                config.TargetMonitorId = mon->deviceId;

                // Store position for this monitor (relative to monitor origin)
                MonitorPosition &pos = config.MonitorPositions[mon->deviceId];
                pos.Left = (int)config.Left - mon->bounds.left;
                pos.Top = (int)config.Top - mon->bounds.top;
                pos.Width = (int)config.Width;
                pos.Height = (int)(config.IsRolledUp ? savedHeight : config.Height);
                pos.RefWidth = mon->width;
                pos.RefHeight = mon->height;
            }
        }
    }
}

void CorralWindow::LoadFiles()
{
    if (GetActiveTab().IsVirtual)
    {
        LoadVirtualFolderIcons();
    }
    else
    {
        LoadIconImages();
    }
    CalculateIconLayout();
    UpdateLayeredContent();
}

void CorralWindow::AddFile(const std::string &fileName)
{
    if (fileName.empty())
        return;

    // Virtual tabs don't accept manual file additions
    if (GetActiveTab().IsVirtual)
        return;

    // Check if already in this corral (active tab)
    auto &files = GetActiveTab().Files;
    auto it = std::find(files.begin(), files.end(), fileName);
    if (it == files.end())
    {
        files.push_back(fileName);
        LoadFiles();
    }
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

        // Handle switching between virtual/regular
        if (folderWatcher)
        {
            folderWatcher->Stop();
            folderWatcher.reset();
        }

        if (GetActiveTab().IsVirtual)
        {
            InitializeFolderWatcher();
        }

        // Update view mode specific settings
        iconSize = GetIconSizeForViewMode();
        UpdateIconSpacingForViewMode();

        scrollPosition = 0;
        LoadFiles();

        if (App::GetInstance())
        {
            App::GetInstance()->SaveConfig();
        }
    }
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
    if (GetActiveTab().IsVirtual)
    {
        if (folderWatcher)
        {
            folderWatcher->Stop();
            folderWatcher.reset();
        }
        InitializeFolderWatcher();
    }
    iconSize = GetIconSizeForViewMode();
    UpdateIconSpacingForViewMode();
    scrollPosition = 0;
    LoadFiles();

    // Create new window for the detached tab
    RECT rect;
    GetWindowRect(hwnd, &rect);
    POINT pt = {rect.left + 30, rect.top + 30};

    if (App::GetInstance())
    {
        App::GetInstance()->CreateCorralAt(pt);

        // Get the new corral and replace its default tab with our detached tab
        const auto &corrals = App::GetInstance()->GetCorrals();
        if (!corrals.empty())
        {
            CorralWindow *newWindow = corrals.back().get();
            newWindow->GetConfig().Tabs.clear();
            newWindow->GetConfig().Tabs.push_back(tabToDetach);
            newWindow->GetConfig().ActiveTabIndex = 0;
            newWindow->LoadFiles();
            newWindow->UpdateLayeredContent();
        }

        App::GetInstance()->SaveConfig();
    }
}

void CorralWindow::MergeWith(CorralWindow *other)
{
    if (!other || other == this)
        return;

    // Copy all tabs from other to this
    for (const auto &tab : other->GetConfig().Tabs)
    {
        config.Tabs.push_back(tab);
    }

    // Switch to the first merged tab
    SetActiveTab((int)config.Tabs.size() - (int)other->GetConfig().Tabs.size());

    // Remove the other corral
    if (App::GetInstance())
    {
        App::GetInstance()->RemoveCorral(&other->GetConfig());
    }
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
    int totalWidth = clientRect.right;

    int tabCount = (int)config.Tabs.size();
    int tabWidth = totalWidth / tabCount;

    return {
        index * tabWidth,
        0,
        (index + 1) * tabWidth,
        GetTitleBarHeight()};
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
            // Track mouse for hover-expand
            if (!window->mouseInsideWindow)
            {
                window->mouseInsideWindow = true;
                TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);

                // Check if we should hover-expand
                if (window->config.IsRolledUp && !window->isHoverExpanded && !window->isAnimating)
                {
                    window->StartHoverExpand();
                }

                // Fade to full opacity and remove tint on hover
                if (window->config.IconOpacity < 255 || window->config.IconTintStrength > 0)
                {
                    window->StartOpacityAnimation(255, 0);
                }
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

            window->isScrollbarHovered = isOverScrollbarRegion;

            // Redraw if hover state changed
            if (wasHovered != window->isScrollbarHovered)
            {
                InvalidateRect(hwnd, nullptr, FALSE);
            }

            // Track hovered icon
            if (!window->isDragging && !window->isResizing && !window->isDraggingIcon && !window->isDraggingScrollbar)
            {
                int prevHovered = window->hoveredIcon;
                window->hoveredIcon = window->HitTestIcon(x, y);
                if (prevHovered != window->hoveredIcon)
                {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }

            if (window->isResizing)
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
                    window->isDraggingIcon = true;
                }
                if (window->isDraggingIcon)
                {
                    window->OnIconDrag(x, y);
                }
            }
            else if (window->isDragging)
            {
                POINT pt;
                GetCursorPos(&pt);
                int newLeft = pt.x - (window->dragStart.x - window->dragStartRect.left);
                int newTop = pt.y - (window->dragStart.y - window->dragStartRect.top);
                int width = window->dragStartRect.right - window->dragStartRect.left;
                int height = window->dragStartRect.bottom - window->dragStartRect.top;

                // Apply snap unless Shift is held
                if (!(GetKeyState(VK_SHIFT) & 0x8000))
                {
                    window->ApplySnap(newLeft, newTop, width, height);
                }

                SetWindowPos(hwnd, nullptr, newLeft, newTop, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

                // Push desktop icons out of the way
                if (App::GetInstance())
                {
                    App::GetInstance()->PushDesktopIconsFromCorrals();
                }
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
                int hit = window->HitTestResize(pt.x, pt.y);
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
            if (wParam == VK_F2 && window->selectedIcon >= 0)
            {
                window->StartIconRename(window->selectedIcon);
                return 0;
            }
            break;
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
            window->OnFolderContentsChanged();
            return 0;
        case WM_DEFERRED_LOAD:
            // Deferred icon loading for virtual corrals (keeps UI responsive)
            window->LoadFiles();
            return 0;
        case WM_MOUSELEAVE:
            window->mouseInsideWindow = false;
            // Reset hovered icon
            if (window->hoveredIcon >= 0)
            {
                window->hoveredIcon = -1;
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
            // Fade back to configured opacity and tint
            if (window->config.IconOpacity < 255 || window->config.IconTintStrength > 0)
            {
                window->StartOpacityAnimation(window->config.IconOpacity, window->config.IconTintStrength);
            }
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
            return 0;
        }
        case WM_MOUSEACTIVATE:
            // Prevent corral from being brought to front when clicked
            return MA_NOACTIVATE;
        case WM_DESTROY:
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            KillTimer(hwnd, HOVER_CHECK_TIMER_ID);
            KillTimer(hwnd, OPACITY_TIMER_ID);
            KillTimer(hwnd, SCROLL_REPOSITION_TIMER_ID);
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
    HDC hdc = BeginPaint(hwnd, &ps);

    // Check if we're currently in non-layered mode (during rename)
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED))
    {
        // Paint a solid background when not layered
        RECT rect;
        GetClientRect(hwnd, &rect);

        // Use the active tab's background color
        BYTE bgR = 40, bgG = 40, bgB = 40;
        const std::string &colorHex = GetActiveTab().ColorHex;
        if (!colorHex.empty() && colorHex[0] == '#' && colorHex.length() >= 7)
        {
            unsigned int colorValue;
            sscanf_s(colorHex.c_str() + 1, "%x", &colorValue);
            bgR = (colorValue >> 16) & 0xFF;
            bgG = (colorValue >> 8) & 0xFF;
            bgB = colorValue & 0xFF;
        }

        HBRUSH brush = CreateSolidBrush(RGB(bgR, bgG, bgB));
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        // Draw title bar background (darker)
        HBRUSH titleBrush = CreateSolidBrush(RGB(bgR / 2, bgG / 2, bgB / 2));
        RECT titleBarRect = {0, 0, rect.right, GetTitleBarHeight()};
        FillRect(hdc, &titleBarRect, titleBrush);
        DeleteObject(titleBrush);

        // Draw title text (match the layered render: tab's header font/size)
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        std::wstring titleFontNameW = Utf8ToWide(GetActiveTab().HeaderFontName);
        int titleFontHeight = -MulDiv(GetActiveTab().HeaderFontSize, GetDpiForWindow(hwnd), 72);
        HFONT titleFont = CreateFontW(titleFontHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, titleFontNameW.c_str());
        HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
        std::wstring wtitle = Utf8ToWide(GetActiveTab().Title);
        RECT titleRect = {8, 8, rect.right - 8, 30};
        DrawTextW(hdc, wtitle.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, oldFont);
        DeleteObject(titleFont);

        // Draw icons. Use the system icon title font (DPI-aware) so labels
        // match the layered render and don't shrink during inline rename.
        LOGFONTW iconLogFont = {};
        HFONT labelFont;
        if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(iconLogFont), &iconLogFont, 0))
        {
            iconLogFont.lfQuality = CLEARTYPE_QUALITY;
            labelFont = CreateFontIndirectW(&iconLogFont);
        }
        else
        {
            labelFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        }
        SelectObject(hdc, labelFont);

        for (size_t i = 0; i < icons.size(); i++)
        {
            const auto &icon = icons[i];
            int drawTop = icon.iconRect.top - scrollPosition;

            // Skip if outside visible area
            if (drawTop + iconSize < GetIconAreaTop() || drawTop > rect.bottom)
                continue;

            // Draw icon
            if (icon.hIcon)
            {
                DrawIconEx(hdc, icon.iconRect.left, drawTop, icon.hIcon,
                           iconSize, iconSize, 0, nullptr, DI_NORMAL);
            }

            // Draw label (skip the one being edited)
            if ((int)i != renamingIconIndex)
            {
                RECT labelRect = {
                    icon.rect.left,
                    icon.iconRect.bottom + 2 - scrollPosition,
                    icon.rect.right,
                    icon.rect.bottom - scrollPosition};
                DrawTextW(hdc, icon.displayName.c_str(), -1, &labelRect,
                          DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
            }
        }

        DeleteObject(labelFont);
    }

    EndPaint(hwnd, &ps);

    // For layered windows, we use UpdateLayeredWindow instead of regular painting
    if (exStyle & WS_EX_LAYERED)
    {
        UpdateLayeredContent();
    }
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

void CorralWindow::OnMove()
{
    SyncConfigFromWindow();
    UpdateLayeredContent();
}
