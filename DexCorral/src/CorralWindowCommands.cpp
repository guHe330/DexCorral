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
 * CorralWindowCommands.cpp - Context menu handling and animation
 *
 * Implements right-click context menu and associated file operations (open, rename,
 * delete, properties, move to tab/corral). Handles command execution, undo/redo,
 * and smooth animations for adding/removing items and tab transitions.
 */

#include "CorralWindow.h"
#include "App.h"
#include "DesktopIcons.h"
#include "FolderWatcher.h"
#include <shobjidl.h>
#include <algorithm>

// ============================================================================
// File-local helper functions for folder browsing
// ============================================================================

static std::wstring BrowseForLocalFolder(HWND hwndOwner, const wchar_t *title)
{
    std::wstring result;

    IFileDialog *pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr))
    {
        DWORD dwOptions;
        hr = pfd->GetOptions(&dwOptions);
        if (SUCCEEDED(hr))
        {
            hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }

        if (SUCCEEDED(hr))
        {
            pfd->SetTitle(title);
        }

        hr = pfd->Show(hwndOwner);
        if (SUCCEEDED(hr))
        {
            IShellItem *psi = nullptr;
            hr = pfd->GetResult(&psi);
            if (SUCCEEDED(hr))
            {
                PWSTR pszPath = nullptr;
                hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (SUCCEEDED(hr))
                {
                    result = pszPath;
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    return result;
}

static bool ValidateLocalFolder(const std::wstring &path, std::wstring &errorMsg)
{
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        errorMsg = L"The specified folder does not exist.";
        return false;
    }

    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        errorMsg = L"The specified path is not a folder.";
        return false;
    }

    // Check for network path (starts with \\)
    if (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\')
    {
        errorMsg = L"Network paths are not supported. Please select a local folder.";
        return false;
    }

    // Check if it's a network drive
    if (path.length() >= 2 && path[1] == L':')
    {
        wchar_t rootPath[4] = {path[0], L':', L'\\', L'\0'};
        UINT driveType = GetDriveTypeW(rootPath);
        if (driveType == DRIVE_REMOTE)
        {
            errorMsg = L"Network drives are not supported. Please select a local folder.";
            return false;
        }
    }

    return true;
}

// ============================================================================
// Context menu
// ============================================================================

void CorralWindow::ShowContextMenu(int x, int y)
{
    HMENU menu = CreatePopupMenu();

    // Tab operations
    AppendMenuW(menu, MF_STRING, 15, L"Add Tab");
    if (config.Tabs.size() > 1)
    {
        AppendMenuW(menu, MF_STRING, 14, L"Detach Tab");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, 1, L"Rename Tab");
    AppendMenuW(menu, MF_STRING, 2, L"Appearance...");

    // Change Folder option for virtual tabs
    if (GetActiveTab().IsVirtual)
    {
        AppendMenuW(menu, MF_STRING, 7, L"Change Folder...");
    }

    // View submenu
    HMENU viewMenu = CreatePopupMenu();
    ViewMode currentMode = GetActiveTab().GetViewMode();
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::SmallIcons ? MF_CHECKED : 0), 10, L"Small Icons");
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::MediumIcons ? MF_CHECKED : 0), 11, L"Medium Icons");
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::LargeIcons ? MF_CHECKED : 0), 12, L"Large Icons");
    AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(viewMenu, MF_STRING | (currentMode == ViewMode::Details ? MF_CHECKED : 0), 13, L"Details");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)viewMenu, L"View");

    // Sort By submenu — virtual corrals in Details view (Explorer-style)
    if (GetActiveTab().IsVirtual && currentMode == ViewMode::Details)
    {
        int sortCol = GetActiveTab().DetailsSortColumn;
        bool asc = GetActiveTab().DetailsSortAscending;
        HMENU sortMenu = CreatePopupMenu();
        AppendMenuW(sortMenu, MF_STRING | (sortCol == 0 ? MF_CHECKED : 0), 60, L"Name");
        AppendMenuW(sortMenu, MF_STRING | (sortCol == 1 ? MF_CHECKED : 0), 61, L"Type");
        AppendMenuW(sortMenu, MF_STRING | (sortCol == 2 ? MF_CHECKED : 0), 62, L"Size");
        AppendMenuW(sortMenu, MF_STRING | (sortCol == 3 ? MF_CHECKED : 0), 63, L"Date modified");
        AppendMenuW(sortMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(sortMenu, MF_STRING | (asc ? MF_CHECKED : 0), 64, L"Ascending");
        AppendMenuW(sortMenu, MF_STRING | (!asc ? MF_CHECKED : 0), 65, L"Descending");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)sortMenu, L"Sort By");
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Catch-all option - only for non-virtual tabs
    if (!GetActiveTab().IsVirtual)
    {
        UINT catchAllFlags = MF_STRING | (GetActiveTab().IsCatchAll ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(menu, catchAllFlags, 3, L"Catch-All (receives new files)");
    }

    // Add Special Icons submenu - only for non-virtual tabs
    std::vector<SpecialDesktopIcon> specialIcons;
    if (!GetActiveTab().IsVirtual)
    {
        specialIcons = DesktopIcons::GetSpecialDesktopIcons();
        if (!specialIcons.empty())
        {
            HMENU specialMenu = CreatePopupMenu();
            for (int i = 0; i < (int)specialIcons.size() && i < 20; i++)
            {
                // Check if already in this corral
                std::string shellEntry = "shell:" + WideToUtf8(specialIcons[i].clsid);
                bool alreadyAdded = std::find(GetActiveTab().Files.begin(), GetActiveTab().Files.end(), shellEntry) != GetActiveTab().Files.end();
                UINT flags = MF_STRING | (alreadyAdded ? (MF_CHECKED | MF_GRAYED) : 0);
                AppendMenuW(specialMenu, flags, 20 + i, specialIcons[i].displayName.c_str());
            }
            AppendMenuW(menu, MF_POPUP, (UINT_PTR)specialMenu, L"Add Special Icon");
        }
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Show Desktop Icons with checkmark
    UINT iconFlags = MF_STRING | (DesktopIcons::AreIconsVisible() ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(menu, iconFlags, 5, L"Show Desktop Icons");

    // Quick-hide exclusion (corral stays visible when double-clicking the desktop)
    UINT quickHideFlags = MF_STRING | (config.ExcludeFromQuickHide ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(menu, quickHideFlags, 9, L"Exclude from Quick-Hide");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 6, L"Create New Corral");
    AppendMenuW(menu, MF_STRING, 8, L"New Virtual Corral");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Delete Corral or Close Tab depending on tab count
    if (config.Tabs.size() > 1)
    {
        AppendMenuW(menu, MF_STRING, 4, L"Close Tab");
    }
    else
    {
        AppendMenuW(menu, MF_STRING, 4, L"Delete Corral");
    }

    POINT pt = {x, y};
    ClientToScreen(hwnd, &pt);

    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    SendToBottom();

    DestroyMenu(menu);

    switch (cmd)
    {
    case 1:
        ShowRenameDialog();
        break;
    case 2:
        ShowAppearanceDialog();
        break;
    case 3:
        ToggleCatchAll();
        break;
    case 4:
        DeleteCorral();
        break;
    case 5:
        if (App::GetInstance())
        {
            App::GetInstance()->ToggleDesktopIcons();
        }
        break;
    case 6:
        if (App::GetInstance())
        {
            // Find a good position for the new corral
            RECT currentRect;
            GetWindowRect(hwnd, &currentRect);

            // Get screen dimensions
            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfo(hMon, &mi);

            POINT newPos;
            int corralWidth = 300;
            int corralHeight = 200;
            int gap = 20;

            // Try to place to the right of current corral
            newPos.x = currentRect.right + gap + corralWidth / 2;
            newPos.y = currentRect.top + corralHeight / 2;

            // If that would go off screen, try below
            if (newPos.x + corralWidth / 2 > mi.rcWork.right)
            {
                newPos.x = currentRect.left + corralWidth / 2;
                newPos.y = currentRect.bottom + gap + corralHeight / 2;
            }

            // If that would also go off screen, offset from current
            if (newPos.y + corralHeight / 2 > mi.rcWork.bottom)
            {
                newPos.x = currentRect.left + 50 + corralWidth / 2;
                newPos.y = currentRect.top + 50 + corralHeight / 2;
            }

            App::GetInstance()->CreateCorralAt(newPos);
        }
        break;
    case 7:
        // Change Folder (virtual corrals only)
        ChangeFolderPath();
        break;
    case 9:
        config.ExcludeFromQuickHide = !config.ExcludeFromQuickHide;
        if (App::GetInstance())
        {
            // If quick-hide is active right now, apply the new setting immediately
            if (App::GetInstance()->IsQuickHideActive())
            {
                if (config.ExcludeFromQuickHide)
                    StartQuickShow();
                else
                    StartQuickHide();
            }
            App::GetInstance()->SaveConfig();
        }
        break;
    case 8:
        // New Virtual Corral
        if (App::GetInstance())
        {
            RECT currentRect;
            GetWindowRect(hwnd, &currentRect);

            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfo(hMon, &mi);

            POINT newPos;
            int corralWidth = 300;
            int corralHeight = 200;
            int gap = 20;

            newPos.x = currentRect.right + gap + corralWidth / 2;
            newPos.y = currentRect.top + corralHeight / 2;

            if (newPos.x + corralWidth / 2 > mi.rcWork.right)
            {
                newPos.x = currentRect.left + corralWidth / 2;
                newPos.y = currentRect.bottom + gap + corralHeight / 2;
            }

            if (newPos.y + corralHeight / 2 > mi.rcWork.bottom)
            {
                newPos.x = currentRect.left + 50 + corralWidth / 2;
                newPos.y = currentRect.top + 50 + corralHeight / 2;
            }

            App::GetInstance()->CreateVirtualCorralAt(newPos);
        }
        break;
    case 10:
        SetViewMode(ViewMode::SmallIcons);
        break;
    case 11:
        SetViewMode(ViewMode::MediumIcons);
        break;
    case 12:
        SetViewMode(ViewMode::LargeIcons);
        break;
    case 13:
        SetViewMode(ViewMode::Details);
        break;
    case 14:
        DetachTab(config.ActiveTabIndex);
        break;
    case 60:
    case 61:
    case 62:
    case 63:
        // Sort By column
        GetActiveTab().DetailsSortColumn = cmd - 60;
        SortVirtualIcons();
        CalculateIconLayout();
        UpdateLayeredContent();
        if (App::GetInstance())
            App::GetInstance()->SaveConfig();
        break;
    case 64:
    case 65:
        // Sort direction (64 = Ascending, 65 = Descending)
        GetActiveTab().DetailsSortAscending = (cmd == 64);
        SortVirtualIcons();
        CalculateIconLayout();
        UpdateLayeredContent();
        if (App::GetInstance())
            App::GetInstance()->SaveConfig();
        break;
    default:
        // Handle special icon additions (IDs 20-39)
        if (cmd >= 20 && cmd < 40)
        {
            int idx = cmd - 20;
            if (idx < (int)specialIcons.size())
            {
                std::string shellEntry = "shell:" + WideToUtf8(specialIcons[idx].clsid);
                auto it = std::find(GetActiveTab().Files.begin(), GetActiveTab().Files.end(), shellEntry);
                if (it == GetActiveTab().Files.end())
                {
                    GetActiveTab().Files.push_back(shellEntry);
                    LoadFiles();
                    if (App::GetInstance())
                    {
                        App::GetInstance()->SaveConfig();
                    }
                }
            }
        }
        break;
    case 15:
    {
        // Add new empty tab
        CorralTabConfig newTab;
        newTab.Title = "New Tab";
        newTab.ColorHex = GetActiveTab().ColorHex;             // Inherit color from current tab
        newTab.HeaderFontName = GetActiveTab().HeaderFontName;  // Inherit font from current tab
        newTab.HeaderFontSize = GetActiveTab().HeaderFontSize;
        newTab.HeaderFontColor = GetActiveTab().HeaderFontColor;
        AddTab(newTab);
        if (App::GetInstance())
        {
            App::GetInstance()->SaveConfig();
        }
        break;
    }
    }
}

// ============================================================================
// Command handlers
// ============================================================================

void CorralWindow::SetViewMode(ViewMode mode)
{
    if (GetActiveTab().GetViewMode() == mode)
        return;

    GetActiveTab().SetViewMode(mode);
    iconSize = GetIconSizeForViewMode();
    UpdateIconSpacingForViewMode();

    // Reload icons to get appropriate size (uses LoadFiles which handles both normal and virtual corrals)
    scrollPosition = 0; // Reset scroll when changing view
    LoadFiles();

    if (App::GetInstance())
    {
        App::GetInstance()->SaveConfig();
    }
}

void CorralWindow::DeleteCorral()
{
    // If there are multiple tabs, just close the active tab
    if (config.Tabs.size() > 1)
    {
        if (MessageBoxW(hwnd, L"Close this tab?", L"Confirm Close", MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            config.Tabs.erase(config.Tabs.begin() + config.ActiveTabIndex);
            if (config.ActiveTabIndex >= (int)config.Tabs.size())
            {
                config.ActiveTabIndex = (int)config.Tabs.size() - 1;
            }
            SetActiveTab(config.ActiveTabIndex);
            if (App::GetInstance())
            {
                App::GetInstance()->SaveConfig();
            }
        }
    }
    else
    {
        // Only one tab - delete the entire window
        if (MessageBoxW(hwnd, L"Delete this corral?", L"Confirm Delete", MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            if (App::GetInstance())
            {
                App::GetInstance()->RemoveCorral(&config);
            }
        }
    }
}

void CorralWindow::ToggleRollUp()
{
    // Cancel any hover animation in progress
    isHoverExpanded = false;
    isAnimating = false;
    KillTimer(hwnd, ANIMATION_TIMER_ID);
    KillTimer(hwnd, HOVER_CHECK_TIMER_ID);

    config.IsRolledUp = !config.IsRolledUp;

    if (config.IsRolledUp)
    {
        // Save current height before rolling up
        savedHeight = config.Height;
        SetWindowPos(hwnd, nullptr, 0, 0, (int)config.Width, GetTitleBarHeight(),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else
    {
        // Restore to saved height
        SetWindowPos(hwnd, nullptr, 0, 0, (int)config.Width, (int)savedHeight,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        config.Height = savedHeight;
    }

    SyncConfigFromWindow();
    CalculateIconLayout();
    UpdateLayeredContent();

    if (App::GetInstance())
    {
        App::GetInstance()->SaveConfig();
    }
}

void CorralWindow::ToggleCatchAll()
{
    App *app = App::GetInstance();
    if (!app)
        return;

    if (GetActiveTab().IsCatchAll)
    {
        // Already catch-all - toggle it off (catch-all can be disabled entirely)
        GetActiveTab().IsCatchAll = false;
        UpdateLayeredContent(); // Remove symbol
        app->SaveConfig();
        return;
    }

    // Set this tab as catch-all and remove from all others
    for (const auto &corral : app->GetCorrals())
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            if (&tab != &GetActiveTab() && tab.IsCatchAll)
            {
                tab.IsCatchAll = false;
            }
        }
        if (corral.get() != this)
        {
            corral->UpdateLayeredContent(); // Remove symbol
        }
    }

    GetActiveTab().IsCatchAll = true;
    UpdateLayeredContent(); // Show symbol
    app->SaveConfig();
}

void CorralWindow::ChangeFolderPath()
{
    if (!GetActiveTab().IsVirtual)
        return;

    std::wstring newPath = BrowseForLocalFolder(hwnd, L"Select Folder for Virtual Corral");
    if (newPath.empty())
        return;

    std::wstring errorMsg;
    if (!ValidateLocalFolder(newPath, errorMsg))
    {
        MessageBoxW(hwnd, errorMsg.c_str(), L"Invalid Folder", MB_OK | MB_ICONWARNING);
        return;
    }

    // Stop existing watcher
    if (folderWatcher)
    {
        folderWatcher->Stop();
        folderWatcher.reset();
    }

    // Update active tab config — new root resets any in-folder navigation
    GetActiveTab().VirtualFolderPath = WideToUtf8(newPath);
    GetActiveTab().CurrentSubPath.clear();

    // Update title to folder name
    size_t lastSlash = newPath.find_last_of(L"\\/");
    std::wstring folderName = (lastSlash != std::wstring::npos) ? newPath.substr(lastSlash + 1) : newPath;
    GetActiveTab().Title = WideToUtf8(folderName);
    SetWindowTextW(hwnd, folderName.c_str());

    // Restart watcher with new path
    InitializeFolderWatcher();

    // Reload icons
    LoadFiles();

    SendToBottom();

    // Save config
    if (App::GetInstance())
    {
        App::GetInstance()->SaveConfig();
    }
}

// ============================================================================
// Hover-expand animation for rolled-up corrals
// ============================================================================

void CorralWindow::StartHoverExpand()
{
    if (!config.IsRolledUp || isHoverExpanded || isAnimating)
        return;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Determine expand direction based on screen position
    // If expanding downward would go off screen, expand upward
    int expandedHeight = (int)savedHeight;
    int bottomIfDown = rect.top + expandedHeight;

    expandUpward = (bottomIfDown > screenHeight - 50); // 50px buffer for taskbar

    animationStartHeight = rect.bottom - rect.top;
    animationTargetHeight = expandedHeight;
    animationStartTop = rect.top;

    if (expandUpward)
    {
        // Move window up while expanding
        animationTargetTop = rect.top - (expandedHeight - animationStartHeight);
    }
    else
    {
        animationTargetTop = rect.top; // Stay in place
    }

    animationStartTime = GetTickCount();
    isAnimating = true;
    isHoverExpanded = true;

    // Start animation timer (60 FPS)
    SetTimer(hwnd, ANIMATION_TIMER_ID, 16, nullptr);
}

void CorralWindow::StartHoverCollapse()
{
    if (!isHoverExpanded || isAnimating)
        return;

    RECT rect;
    GetWindowRect(hwnd, &rect);

    animationStartHeight = rect.bottom - rect.top;
    animationTargetHeight = GetTitleBarHeight();
    animationStartTop = rect.top;

    if (expandUpward)
    {
        // Move window back down while collapsing
        animationTargetTop = rect.top + (animationStartHeight - GetTitleBarHeight());
    }
    else
    {
        animationTargetTop = rect.top; // Stay in place
    }

    animationStartTime = GetTickCount();
    isAnimating = true;

    SetTimer(hwnd, ANIMATION_TIMER_ID, 16, nullptr);
}

void CorralWindow::OnAnimationTimer()
{
    DWORD elapsed = GetTickCount() - animationStartTime;
    float progress = (float)elapsed / ANIMATION_DURATION;

    if (progress >= 1.0f)
    {
        progress = 1.0f;
        isAnimating = false;
        KillTimer(hwnd, ANIMATION_TIMER_ID);

        // If we just collapsed, mark as not hover-expanded
        if (animationTargetHeight == GetTitleBarHeight())
        {
            isHoverExpanded = false;
        }

        // Start hover check timer to detect when mouse leaves
        if (isHoverExpanded)
        {
            SetTimer(hwnd, HOVER_CHECK_TIMER_ID, 100, nullptr);
        }
        else
        {
            KillTimer(hwnd, HOVER_CHECK_TIMER_ID);
        }
    }

    // Ease-out interpolation for smoother animation
    float easedProgress = 1.0f - (1.0f - progress) * (1.0f - progress);

    int currentHeight = animationStartHeight + (int)((animationTargetHeight - animationStartHeight) * easedProgress);
    int currentTop = animationStartTop + (int)((animationTargetTop - animationStartTop) * easedProgress);

    SetWindowPos(hwnd, nullptr, (int)config.Left, currentTop, (int)config.Width, currentHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Recalculate layout and redraw
    CalculateIconLayout();
    UpdateLayeredContent();
}

void CorralWindow::OnHoverCheckTimer()
{
    // Check if mouse is still inside the window
    POINT pt;
    GetCursorPos(&pt);

    RECT rect;
    GetWindowRect(hwnd, &rect);

    if (!PtInRect(&rect, pt))
    {
        // Mouse has left - collapse
        mouseInsideWindow = false;
        KillTimer(hwnd, HOVER_CHECK_TIMER_ID);
        if (isHoverExpanded && !isAnimating)
        {
            StartHoverCollapse();
        }
    }
}

void CorralWindow::StartOpacityAnimation(int target)
{
    StartOpacityAnimation(target, (target == 255) ? 0 : config.IconTintStrength);
}

void CorralWindow::StartOpacityAnimation(int target, int tintTargetVal)
{
    bool opacityChanged = (currentOpacity != target);
    bool tintChanged = (currentTintStrength != tintTargetVal);
    if (!opacityChanged && !tintChanged)
        return; // Already at target
    opacityStart = currentOpacity;
    opacityTarget = target;
    tintStart = currentTintStrength;
    tintTarget = tintTargetVal;
    opacityAnimationStartTime = GetTickCount();
    isOpacityAnimating = true;
    SetTimer(hwnd, OPACITY_TIMER_ID, 16, nullptr); // ~60fps
}

void CorralWindow::OnOpacityAnimationTimer()
{
    DWORD elapsed = GetTickCount() - opacityAnimationStartTime;
    float progress = (float)elapsed / OPACITY_ANIMATION_DURATION;

    if (progress >= 1.0f)
    {
        progress = 1.0f;
        isOpacityAnimating = false;
        KillTimer(hwnd, OPACITY_TIMER_ID);
    }

    // Ease-out quadratic
    float easedProgress = 1.0f - (1.0f - progress) * (1.0f - progress);

    currentOpacity = opacityStart + (int)((opacityTarget - opacityStart) * easedProgress);
    if (currentOpacity < 0)
        currentOpacity = 0;
    if (currentOpacity > 255)
        currentOpacity = 255;

    currentTintStrength = tintStart + (int)((tintTarget - tintStart) * easedProgress);
    if (currentTintStrength < 0)
        currentTintStrength = 0;
    if (currentTintStrength > 255)
        currentTintStrength = 255;

    UpdateLayeredContent();
}

// ============================================================================
// Quick-hide (double-click empty desktop hides/shows everything)
// ============================================================================

void CorralWindow::StartQuickHide()
{
    if (!hwnd || (isQuickHidden && !isQuickHideAnimating))
        return;

    isQuickHidden = true;
    quickHideStartAlpha = quickHideAlpha;
    quickHideAnimationStartTime = GetTickCount();
    isQuickHideAnimating = true;
    SetTimer(hwnd, QUICKHIDE_TIMER_ID, 16, nullptr); // ~60fps
}

void CorralWindow::StartQuickShow()
{
    if (!hwnd || (!isQuickHidden && !isQuickHideAnimating))
        return;

    isQuickHidden = false;
    if (!IsWindowVisible(hwnd))
    {
        // Window already faded out completely — bring it back invisible and fade in.
        // Plain ShowWindow (not Show()) so files are not reloaded on every toggle.
        quickHideAlpha = 0;
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SendToBottom();
    }
    quickHideStartAlpha = quickHideAlpha;
    quickHideAnimationStartTime = GetTickCount();
    isQuickHideAnimating = true;
    SetTimer(hwnd, QUICKHIDE_TIMER_ID, 16, nullptr);
    UpdateLayeredContent();
}

void CorralWindow::OnQuickHideAnimationTimer()
{
    DWORD elapsed = GetTickCount() - quickHideAnimationStartTime;
    float progress = (float)elapsed / QUICKHIDE_ANIMATION_DURATION;
    if (progress >= 1.0f)
        progress = 1.0f;

    // Ease-out quadratic (same curve as the opacity hover animation)
    float easedProgress = 1.0f - (1.0f - progress) * (1.0f - progress);

    int target = isQuickHidden ? 0 : 255;
    quickHideAlpha = quickHideStartAlpha + (int)((target - quickHideStartAlpha) * easedProgress);
    if (quickHideAlpha < 0)
        quickHideAlpha = 0;
    if (quickHideAlpha > 255)
        quickHideAlpha = 255;

    if (progress >= 1.0f)
    {
        isQuickHideAnimating = false;
        KillTimer(hwnd, QUICKHIDE_TIMER_ID);
        if (isQuickHidden)
        {
            Hide();
            return; // Window hidden — no need to repaint
        }
    }

    UpdateLayeredContent();
}
