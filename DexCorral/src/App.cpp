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
 * App.cpp - Main application controller and message pump
 *
 * Manages application initialization, shutdown, and the main message loop.
 * Handles corral window creation/destruction, application-wide settings,
 * appearance synchronization, and desktop icon management (pushing icons
 * out of the way when they overlap with corrals).
 */

#include "App.h"
#include "CorralWindow.h"
#include "DesktopIcons.h"
#include "IconUtils.h"
#include "DesktopMonitor.h"
#include "HookBridge.h"
#include "Version.h"
#include <CommCtrl.h>
#include <ShlObj.h>
#include <shobjidl.h>
#include <Psapi.h>
#include <cstdlib>
#include <algorithm>

// Stop event — signaled by DLL_PROCESS_DETACH to tell the App message loop to quit
static HANDLE g_AppStopEvent = nullptr;

// Entry point called from the DLL worker thread
extern "C" int RunApp(HANDLE hStopEvent)
{
    g_AppStopEvent = hStopEvent;
    App app;
    return app.Run();
}

App *App::instance = nullptr;

static const wchar_t *MESSAGE_WINDOW_CLASS = L"DexCorralMessageWindow";
App::App() : messageWindow(nullptr)
{
    instance = this;
}

App::~App()
{
    Shutdown();
    instance = nullptr;
}

void App::Initialize()
{
    // Register message window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MessageWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = MESSAGE_WINDOW_CLASS;
    RegisterClassExW(&wc);

    // Create hidden message window
    messageWindow = CreateWindowExW(
        0,
        MESSAGE_WINDOW_CLASS,
        L"DexCorral Message Window",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr,
        GetModuleHandleW(nullptr), this);

    // Register for Explorer restart so we can re-add the tray icon
    wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    // Register for shell image-list changes (e.g. Recycle Bin full→empty)
    SHChangeNotifyEntry shcnentry = {};
    shcnentry.pidl = nullptr; // all items
    shcnentry.fRecursive = TRUE;
    shellNotifyId = SHChangeNotifyRegister(
        messageWindow,
        SHCNRF_ShellLevel | SHCNRF_NewDelivery,
        SHCNE_UPDATEIMAGE,
        WM_APP + 101, // custom message
        1, &shcnentry);

    // Load configuration
    LoadConfig();
    HookBridge::SetDebugLogging(config.DebugLogging);

    // Create monitor manager (before corrals)
    monitorManager = std::make_unique<MonitorManager>();

    // Setup mouse hook
    mouseHook = std::make_unique<MouseHook>();
    mouseHook->SetLeftButtonDownCallback([this](POINT pt)
                                         { OnLeftButtonDown(pt); });
    mouseHook->SetLeftButtonUpCallback([this](POINT pt)
                                       { OnLeftButtonUp(pt); });
    mouseHook->SetMouseMoveCallback([this](POINT pt)
                                    { OnMouseMove(pt); });
    // Note: Mousewheel events are NOT hooked - they pass through naturally to windows
    mouseHook->Start();

    // Create tray icon
    HICON icon = LoadIconW(nullptr, IDI_APPLICATION);
    trayIcon = std::make_unique<TrayIcon>(messageWindow, icon, L"DexCorral - Double-click desktop to hide icons");

    // Restore corrals
    RestoreCorrals();

    // Restore desktop icons state
    // Default to true if not specified (backward compatibility)
    DesktopIcons::SetIconsVisible(config.DesktopIconsVisible);

    if (corrals.empty())
    {
        // First run: create a default catch-all corral at center of screen
        CorralWindowConfig defaultConfig;
        defaultConfig.Left = GetSystemMetrics(SM_CXSCREEN) / 2 - 150;
        defaultConfig.Top = GetSystemMetrics(SM_CYSCREEN) / 2 - 100;
        defaultConfig.Width = 300;
        defaultConfig.Height = 200;

        CorralTabConfig tab;
        tab.Title = "Desktop";
        tab.IsCatchAll = true; // First corral is catch-all
        tab.ColorHex = config.DefaultColorHex;
        tab.HeaderFontName = config.DefaultHeaderFontName;
        tab.HeaderFontSize = config.DefaultHeaderFontSize;
        tab.HeaderFontColor = config.DefaultHeaderFontColor;
        defaultConfig.Tabs.push_back(tab);

        // Apply default appearance settings (same as CreateCorral)
        defaultConfig.TitleBarHeight = config.DefaultTitleBarHeight;
        defaultConfig.IconOpacity = config.DefaultIconOpacity;
        defaultConfig.IconTintColor = config.DefaultIconTintColor;
        defaultConfig.IconTintStrength = config.DefaultIconTintStrength;
        defaultConfig.IconSpacingXPercent = config.DefaultIconSpacingXPercent;
        defaultConfig.IconSpacingYPercent = config.DefaultIconSpacingYPercent;

        auto corral = std::make_unique<CorralWindow>(defaultConfig);
        corral->Show();
        corrals.push_back(std::move(corral));

        // First run: enable autostart by default
        SetAutostart(true);

        SaveConfig();
    }

    // Ensure exactly one catch-all corral exists
    EnsureCatchAllCorral();

    // Start desktop monitoring
    desktopMonitor = std::make_unique<DesktopMonitor>();
    desktopMonitor->SetFileAddedCallback([this](const std::wstring &fileName)
                                         { OnDesktopFileAdded(fileName); });
    desktopMonitor->SetFileRenamedCallback([this](const std::wstring &oldName, const std::wstring &newName)
                                           { OnDesktopFileRenamed(oldName, newName); });
    desktopMonitor->SetFileDeletedCallback([this](const std::wstring &fileName)
                                           { OnDesktopFileDeleted(fileName); });
    desktopMonitor->Start();

    // Hook is in-process (shell extension) — just update hidden icon list
    UpdateHookHiddenIcons();
    PositionHiddenIconsUnderCorrals();
    HookBridge::RefreshDesktop();
}

void App::Shutdown()
{
    // Unregister shell change notifications
    if (shellNotifyId)
    {
        SHChangeNotifyDeregister(shellNotifyId);
        shellNotifyId = 0;
    }

    // Stop desktop monitor first
    if (desktopMonitor)
    {
        desktopMonitor->Stop();
    }

    // Save config before exit
    SaveConfig();

    // Clear hidden icons so hook stops hiding them
    HookBridge::ClearHiddenIcons();
    HookBridge::RefreshDesktop();

    // IMPORTANT: Show desktop icons before exit so user isn't stuck
    DesktopIcons::SetIconsVisible(true);

    // Clean up corrals
    corrals.clear();

    // Stop mouse hook
    if (mouseHook)
    {
        mouseHook->Stop();
    }
}

void App::LoadConfig()
{
    config = Config::Load();
}

void App::SaveConfig()
{
    // Update desktop icons state
    config.DesktopIconsVisible = DesktopIcons::AreIconsVisible();

    // Sync all corral configs from their current window states
    config.Corrals.clear();
    for (auto &corral : corrals)
    {
        corral->SyncConfigFromWindow();
        config.Corrals.push_back(corral->GetConfig());
    }
    Config::Save(config);

    // Update hook hidden icons (hook is always active in shell extension mode)
    UpdateHookHiddenIcons();
    PositionHiddenIconsUnderCorrals();
    HookBridge::RefreshDesktop();
}

void App::RestoreCorrals()
{
    for (auto &corralConfig : config.Corrals)
    {
        // Skip corrals with invalid dimensions (corrupted config)
        if (corralConfig.Width < 50 || corralConfig.Height < 50)
        {
            corralConfig.Width = 300;
            corralConfig.Height = 200;
        }
        if (corralConfig.Left < -1000 || corralConfig.Top < -1000)
        {
            corralConfig.Left = GetSystemMetrics(SM_CXSCREEN) / 2 - 150;
            corralConfig.Top = GetSystemMetrics(SM_CYSCREEN) / 2 - 100;
        }

        auto corral = std::make_unique<CorralWindow>(corralConfig);
        corral->Show();
        corrals.push_back(std::move(corral));
    }
}

void App::RemoveCorral(CorralWindowConfig *configToRemove)
{
    for (auto it = corrals.begin(); it != corrals.end(); ++it)
    {
        if (&(*it)->GetConfig() == configToRemove)
        {
            corrals.erase(it);
            break;
        }
    }
    SaveConfig();
}

void App::RemoveFileFromOtherCorrals(const std::wstring &fileName, CorralTabConfig *exceptTab)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    bool changed = false;
    for (auto &corral : corrals)
    {
        auto &corralConfig = corral->GetConfig();
        for (auto &tab : corralConfig.Tabs)
        {
            if (&tab != exceptTab)
            {
                auto it = std::find(tab.Files.begin(), tab.Files.end(), fileNameStr);
                if (it != tab.Files.end())
                {
                    tab.Files.erase(it);
                    changed = true;
                }
            }
        }
    }

    if (changed)
    {
        SaveConfig();
        RefreshAllCorrals();
    }
}

void App::RefreshAllCorrals()
{
    for (auto &corral : corrals)
    {
        corral->LoadFiles();
    }
}

void App::RefreshAllCorralBackgrounds()
{
    for (auto &corral : corrals)
    {
        corral->RecalculateLayout();
    }
}

void App::SetDefaultColorHex(const std::string &colorHex)
{
    config.DefaultColorHex = colorHex;
}

void App::SetDefaultAppearance(int titleBarHeight, const std::string &fontName,
                               int fontSize, const std::string &fontColor, int iconOpacity,
                               const std::string &tintColor, int tintStrength,
                               int spacingX, int spacingY)
{
    config.DefaultTitleBarHeight = titleBarHeight;
    config.DefaultHeaderFontName = fontName;
    config.DefaultHeaderFontSize = fontSize;
    config.DefaultHeaderFontColor = fontColor;
    config.DefaultIconOpacity = iconOpacity;
    config.DefaultIconTintColor = tintColor;
    config.DefaultIconTintStrength = tintStrength;
    config.DefaultIconSpacingXPercent = spacingX;
    config.DefaultIconSpacingYPercent = spacingY;
}

void App::ApplyColorToAllCorrals(const std::string &colorHex)
{
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            tab.ColorHex = colorHex;
        }
        corral->RecalculateLayout();
    }
}

void App::ApplyAppearanceToAllCorrals(const std::string &colorHex, bool applyColor,
                                      int titleBarHeight, bool applyHeight,
                                      const std::string &fontName, int fontSize, bool applyFont,
                                      const std::string &fontColor, bool applyFontColor,
                                      int iconOpacity, bool applyIconOpacity,
                                      const std::string &tintColor, int tintStrength, bool applyTint,
                                      int spacingX, int spacingY, bool applySpacing)
{
    for (auto &corral : corrals)
    {
        auto &cfg = corral->GetConfig();
        bool needsLayoutRecalc = false;

        if (applyColor)
        {
            for (auto &tab : cfg.Tabs)
            {
                tab.ColorHex = colorHex;
            }
        }
        if (applyHeight)
        {
            cfg.TitleBarHeight = titleBarHeight;
            needsLayoutRecalc = true;
        }
        if (applyFont || applyFontColor)
        {
            int activeIdx = cfg.ActiveTabIndex;
            if (activeIdx >= 0 && activeIdx < (int)cfg.Tabs.size())
            {
                if (applyFont)
                {
                    cfg.Tabs[activeIdx].HeaderFontName = fontName;
                    cfg.Tabs[activeIdx].HeaderFontSize = fontSize;
                }
                if (applyFontColor)
                    cfg.Tabs[activeIdx].HeaderFontColor = fontColor;
            }
        }
        if (applyIconOpacity)
        {
            cfg.IconOpacity = iconOpacity;
            corral->SetCurrentOpacity(iconOpacity);
        }
        if (applyTint)
        {
            cfg.IconTintColor = tintColor;
            cfg.IconTintStrength = tintStrength;
            corral->SetCurrentTintStrength(tintStrength);
        }
        if (applySpacing)
        {
            cfg.IconSpacingXPercent = spacingX;
            cfg.IconSpacingYPercent = spacingY;
            needsLayoutRecalc = true;
        }

        if (needsLayoutRecalc)
        {
            corral->RecalculateLayout();
        }
        else
        {
            corral->RecalculateLayout();
        }
    }
}

void App::ToggleDesktopIcons()
{
    bool currentlyVisible = DesktopIcons::AreIconsVisible();
    DesktopIcons::SetIconsVisible(!currentlyVisible);
    SaveConfig();
}

void App::ToggleShortcutArrows()
{
    bool currentlyHidden = DesktopIcons::AreShortcutArrowsHidden();
    bool newState = !currentlyHidden;

    // Confirm with user since this requires Explorer restart
    const wchar_t *message = newState
                                 ? L"This will hide shortcut arrows on desktop icons.\n\nExplorer will restart to apply the change. Continue?"
                                 : L"This will restore shortcut arrows on desktop icons.\n\nExplorer will restart to apply the change. Continue?";

    int result = MessageBoxW(nullptr, message, L"DexCorral", MB_YESNO | MB_ICONQUESTION);
    if (result != IDYES)
    {
        return;
    }

    if (DesktopIcons::SetShortcutArrowsHidden(newState))
    {
        config.HideShortcutArrows = newState;
        SaveConfig();
        DesktopIcons::RestartExplorer();
    }
    else
    {
        MessageBoxW(nullptr, L"Failed to change shortcut arrow setting.\n\nThis may require administrator privileges.", L"DexCorral", MB_OK | MB_ICONWARNING);
    }
}

void App::UpdateHookHiddenIcons()
{
    // Collect display names of all icons across all corrals and ALL tabs
    // (not just active tab - inactive tab icons should stay hidden too)
    std::vector<std::wstring> displayNames;
    for (const auto &corral : corrals)
    {
        for (const auto &tab : corral->GetConfig().Tabs)
        {
            if (tab.IsVirtual)
                continue; // Virtual tabs don't hide desktop icons

            for (const auto &fileUtf8 : tab.Files)
            {
                // Special icon: resolve CLSID to display name
                if (CorralWindow::IsSpecialIconEntry(fileUtf8))
                {
                    std::wstring clsid = CorralWindow::GetSpecialIconClsid(fileUtf8);
                    std::wstring name = DesktopIcons::GetSpecialIconDisplayName(clsid);
                    if (!name.empty())
                    {
                        displayNames.push_back(name);
                    }
                    continue;
                }

                // Convert UTF-8 filename to wide and strip .lnk extension
                int size = MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), nullptr, 0);
                std::wstring wName(size, 0);
                MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), &wName[0], size);
                wName = IconUtils::StripLnkExtension(wName);

                displayNames.push_back(wName);
            }
        }
    }

    HookBridge::UpdateHiddenIcons(displayNames);
}

void App::PositionHiddenIconsUnderCorrals()
{
    // Position hidden icons at per-icon screen positions matching their visual
    // location in the corral. Icons visible in the corral viewport get positioned
    // at their actual screen coords (under the corral window). Icons scrolled out
    // of view get positioned just outside the corral edge. If DexCorral crashes,
    // icons reappear near where the corral was.
    std::map<std::wstring, POINT2D> positions;

    for (const auto &corral : corrals)
    {
        // Get per-icon positions from the active tab (already in ListView client coords)
        auto iconPositions = corral->GetIconScreenPositions();
        for (auto &[name, pos] : iconPositions)
        {
            positions[name] = pos;
        }

        // For non-active tabs, position icons at corral center (they're not rendered)
        RECT r;
        if (!GetWindowRect(corral->GetHWND(), &r))
            continue;
        HWND hListView = DesktopIcons::GetDesktopListView();
        POINT center = {(r.left + r.right) / 2, (r.top + r.bottom) / 2};
        if (hListView)
        {
            ScreenToClient(hListView, &center);
        }

        int activeTabIndex = corral->GetConfig().ActiveTabIndex;
        for (int t = 0; t < (int)corral->GetConfig().Tabs.size(); t++)
        {
            if (t == activeTabIndex)
                continue; // Already handled by GetIconScreenPositions
            const auto &tab = corral->GetConfig().Tabs[t];
            if (tab.IsVirtual)
                continue;
            for (const auto &fileUtf8 : tab.Files)
            {
                std::wstring name;
                if (CorralWindow::IsSpecialIconEntry(fileUtf8))
                {
                    std::wstring clsid = CorralWindow::GetSpecialIconClsid(fileUtf8);
                    name = DesktopIcons::GetSpecialIconDisplayName(clsid);
                }
                else
                {
                    int size = MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), nullptr, 0);
                    name.resize(size);
                    MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), &name[0], size);
                    name = IconUtils::StripLnkExtension(name);
                }
                if (!name.empty())
                {
                    positions[name] = {center.x, center.y};
                }
            }
        }
    }

    if (!positions.empty())
    {
        DesktopIcons::PositionIcons(positions);
    }
}

// ============================================================================
// Desktop icon push-out-of-way support
// ============================================================================

void App::CacheDesktopIconPositions()
{
    cachedDesktopIconPositions = DesktopIcons::GetAllIconPositions();

    // Remove icons that are hidden by corrals (they're managed, not free)
    auto it = cachedDesktopIconPositions.begin();
    while (it != cachedDesktopIconPositions.end())
    {
        if (IsIconHiddenByCorral(it->first))
        {
            it = cachedDesktopIconPositions.erase(it);
        }
        else
        {
            ++it;
        }
    }

    desktopIconCacheValid = true;
}

void App::InvalidateDesktopIconCache()
{
    desktopIconCacheValid = false;
    cachedDesktopIconPositions.clear();
}

bool App::IsIconHiddenByCorral(const std::wstring &displayName) const
{
    for (const auto &corral : corrals)
    {
        for (const auto &tab : corral->GetConfig().Tabs)
        {
            if (tab.IsVirtual)
                continue;

            for (const auto &fileUtf8 : tab.Files)
            {
                std::wstring name;
                if (CorralWindow::IsSpecialIconEntry(fileUtf8))
                {
                    std::wstring clsid = CorralWindow::GetSpecialIconClsid(fileUtf8);
                    name = DesktopIcons::GetSpecialIconDisplayName(clsid);
                }
                else
                {
                    int size = MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), nullptr, 0);
                    name.resize(size);
                    MultiByteToWideChar(CP_UTF8, 0, fileUtf8.c_str(), (int)fileUtf8.size(), &name[0], size);
                    name = IconUtils::StripLnkExtension(name);
                }
                if (!name.empty() && _wcsicmp(name.c_str(), displayName.c_str()) == 0)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

std::vector<RECT> App::GetAllCorralRects() const
{
    // Returns corral rects in desktop ListView client coordinates
    // (LVM_GETITEMPOSITION returns client coords, so we need to match)
    HWND hListView = DesktopIcons::GetDesktopListView();
    std::vector<RECT> rects;
    for (const auto &corral : corrals)
    {
        RECT r;
        if (GetWindowRect(corral->GetHWND(), &r))
        {
            if (hListView)
            {
                // Convert screen coords to ListView client coords
                POINT topLeft = {r.left, r.top};
                POINT bottomRight = {r.right, r.bottom};
                ScreenToClient(hListView, &topLeft);
                ScreenToClient(hListView, &bottomRight);
                r.left = topLeft.x;
                r.top = topLeft.y;
                r.right = bottomRight.x;
                r.bottom = bottomRight.y;
            }
            rects.push_back(r);
        }
    }
    return rects;
}

static bool RectsOverlap(const RECT &a, const RECT &b)
{
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

/**
 * Pushes desktop icons out of the way if they overlap with corral windows.
 *
 * Algorithm:
 * 1. Cache current desktop icon positions if not already cached
 * 2. For each cached icon that overlaps with a corral:
 *    - Calculate distance from icon center to each edge of the overlapping corral
 *    - Try pushing the icon out from the nearest edge first (shortest distance)
 *    - Search for a free position one grid step away (75px increments)
 *    - Continue stepping outward until a free position is found
 * 3. Update Explorer with new positions for moved icons
 *
 * Uses a 75x75px icon footprint for collision detection (standard desktop icon size).
 * Icons already off-screen (< -1000) are skipped. Icons are pushed to the nearest
 * free grid position rather than to arbitrary coordinates, maintaining alignment.
 */
void App::PushDesktopIconsFromCorrals()
{
    if (!desktopIconCacheValid)
    {
        CacheDesktopIconPositions();
    }

    auto corralRects = GetAllCorralRects();
    if (corralRects.empty())
        return;

    /// Icon bounding box size (approximate desktop icon footprint)
    const int ICON_W = 75;
    const int ICON_H = 75;
    const int STEP = 75; /// Push step size (one grid jump)

    // Get screen bounds in ListView client coordinates
    HWND hListView = DesktopIcons::GetDesktopListView();
    int screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenRight = screenLeft + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenBottom = screenTop + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (hListView)
    {
        POINT tl = {screenLeft, screenTop};
        POINT br = {screenRight, screenBottom};
        ScreenToClient(hListView, &tl);
        ScreenToClient(hListView, &br);
        screenLeft = tl.x;
        screenTop = tl.y;
        screenRight = br.x;
        screenBottom = br.y;
    }

    // Helper: check if a position is free (no corral overlap, no icon overlap, on screen)
    auto isPositionFree = [&](int x, int y, const std::wstring &skipName) -> bool
    {
        if (x < screenLeft || y < screenTop ||
            x + ICON_W > screenRight || y + ICON_H > screenBottom)
            return false;
        RECT r = {x, y, x + ICON_W, y + ICON_H};
        for (const auto &cr : corralRects)
        {
            if (RectsOverlap(r, cr))
                return false;
        }
        for (const auto &[otherName, otherPos] : cachedDesktopIconPositions)
        {
            if (otherName == skipName)
                continue;
            if (otherPos.x < -1000 || otherPos.y < -1000)
                continue;
            RECT otherRect = {otherPos.x, otherPos.y, otherPos.x + ICON_W, otherPos.y + ICON_H};
            if (RectsOverlap(r, otherRect))
                return false;
        }
        return true;
    };

    bool anyMoved = false;
    std::vector<std::pair<std::wstring, POINT2D>> toMove;

    for (auto &[name, pos] : cachedDesktopIconPositions)
    {
        // Skip icons far off-screen (already hidden)
        if (pos.x < -1000 || pos.y < -1000)
            continue;

        RECT iconRect = {pos.x, pos.y, pos.x + ICON_W, pos.y + ICON_H};

        // Find which corral this icon overlaps (if any)
        const RECT *overlappingCorral = nullptr;
        for (const auto &cr : corralRects)
        {
            if (RectsOverlap(iconRect, cr))
            {
                overlappingCorral = &cr;
                break;
            }
        }
        if (!overlappingCorral)
            continue;

        // Find nearest edge of the overlapping corral and push one step past it
        // Calculate distance to each edge from icon center
        int iconCenterX = pos.x + ICON_W / 2;
        int iconCenterY = pos.y + ICON_H / 2;
        int distLeft = iconCenterX - overlappingCorral->left;
        int distRight = overlappingCorral->right - iconCenterX;
        int distTop = iconCenterY - overlappingCorral->top;
        int distBottom = overlappingCorral->bottom - iconCenterY;

        // Try each direction, ordered by shortest distance to edge
        struct PushDir
        {
            int dx;
            int dy;
            int dist;
        };
        PushDir dirs[4] = {
            {-1, 0, distLeft},
            {1, 0, distRight},
            {0, -1, distTop},
            {0, 1, distBottom}};
        // Sort by distance (nearest edge first)
        for (int i = 0; i < 3; i++)
            for (int j = i + 1; j < 4; j++)
                if (dirs[j].dist < dirs[i].dist)
                    std::swap(dirs[i], dirs[j]);

        bool found = false;
        int newX = pos.x, newY = pos.y;

        for (const auto &dir : dirs)
        {
            for (int step = 1; step <= 40; step++)
            {
                int candidateX = pos.x + dir.dx * STEP * step;
                int candidateY = pos.y + dir.dy * STEP * step;

                if (isPositionFree(candidateX, candidateY, name))
                {
                    newX = candidateX;
                    newY = candidateY;
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }

        if (newX != pos.x || newY != pos.y)
        {
            toMove.push_back({name, {newX, newY}});
            // Update cache so subsequent icons see the new position
            pos.x = newX;
            pos.y = newY;
            anyMoved = true;
        }
    }

    // Apply all moves
    for (const auto &[name, newPos] : toMove)
    {
        DesktopIcons::PositionIcon(name, newPos.x, newPos.y);
    }
    if (anyMoved)
    {
        HookBridge::RefreshDesktop();
    }
}

void App::OnLeftButtonDown(POINT pt)
{
    // Reserved for future desktop interactions
    // Currently no action on desktop click
}

void App::OnLeftButtonUp(POINT pt)
{
    // Global mouse up - could be used for drag-end if needed
    // But currently CorralWindow handles its own drag
}

void App::OnMouseMove(POINT pt)
{
    // Handle mouse move if needed
}

void App::ShowTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 3, L"About");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1, L"Create New Corral");
    AppendMenuW(menu, MF_STRING, 5, L"New Virtual Corral");

    // Icon visibility toggle
    UINT flags = DesktopIcons::AreIconsVisible() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | flags, 2, L"Show Desktop Icons");

    // Autostart toggle
    UINT autostartFlags = IsAutostartEnabled() ? MF_CHECKED : MF_UNCHECKED;
    AppendMenuW(menu, MF_STRING | autostartFlags, 4, L"Start with Windows");

    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(messageWindow);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, messageWindow, nullptr);
    PostMessageW(messageWindow, WM_NULL, 0, 0);

    DestroyMenu(menu);

    switch (cmd)
    {
    case 3:
        ShowAbout();
        break;
    case 1:
    {
        int offsetX = (int)corrals.size() * 30;
        int offsetY = (int)corrals.size() * 30;
        POINT centerPt = {
            GetSystemMetrics(SM_CXSCREEN) / 2 + offsetX,
            GetSystemMetrics(SM_CYSCREEN) / 2 + offsetY};
        ShowCreationMenu(centerPt);
        break;
    }
    case 2:
        ToggleDesktopIcons();
        break;
    case 4:
        SetAutostart(!IsAutostartEnabled());
        break;
    case 5:
    {
        int offsetX = (int)corrals.size() * 30;
        int offsetY = (int)corrals.size() * 30;
        POINT centerPt = {
            GetSystemMetrics(SM_CXSCREEN) / 2 + offsetX,
            GetSystemMetrics(SM_CYSCREEN) / 2 + offsetY};
        CreateVirtualCorralAt(centerPt);
        break;
    }
    }
}

void App::ShowCreationMenu(POINT pt)
{
    CreateCorral(pt);
}

void App::ShowAbout()
{
    // Build the About message with GPL-3.0 license notice
    std::wstring aboutText = L"DexCorral - a free and open source Windows desktop icon organizer\n\n";
    aboutText += L"Version: ";
    aboutText += DEXCORRAL_VERSION;
    aboutText += L"\n\n";
    aboutText += L"Copyright (C) 2026 Gunter Heiss\n\n";
    aboutText += L"This program is free software: you can redistribute it and/or modify\n";
    aboutText += L"it under the terms of the GNU General Public License as published by\n";
    aboutText += L"the Free Software Foundation, either version 3 of the License, or\n";
    aboutText += L"(at your option) any later version.\n\n";
    aboutText += L"This program is distributed in the hope that it will be useful,\n";
    aboutText += L"but WITHOUT ANY WARRANTY; without even the implied warranty of\n";
    aboutText += L"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n";
    aboutText += L"GNU General Public License for more details.\n\n";
    aboutText += L"You should have received a copy of the GNU General Public License\n";
    aboutText += L"along with this program.  If not, see https://www.gnu.org/licenses/\n\n";
    aboutText += L"Website: https://dexcorral.app\n";
    aboutText += L"GitHub: https://github.com/guHe300/DexCorral";

    MessageBoxW(messageWindow, aboutText.c_str(), L"About DexCorral", MB_OK | MB_ICONINFORMATION);
}

void App::CreateCorral(POINT pt)
{
    CorralWindowConfig newConfig;

    // Create at fixed default size centered on point
    newConfig.Left = (double)pt.x - 150;
    newConfig.Top = (double)pt.y - 100;
    newConfig.Width = 300;
    newConfig.Height = 200;

    CorralTabConfig tab;
    tab.Title = "New Corral";
    tab.ColorHex = config.DefaultColorHex; // Use saved default appearance
    tab.HeaderFontName = config.DefaultHeaderFontName;
    tab.HeaderFontSize = config.DefaultHeaderFontSize;
    tab.HeaderFontColor = config.DefaultHeaderFontColor;
    newConfig.Tabs.push_back(tab);

    // Apply default appearance settings
    newConfig.TitleBarHeight = config.DefaultTitleBarHeight;
    newConfig.IconOpacity = config.DefaultIconOpacity;
    newConfig.IconTintColor = config.DefaultIconTintColor;
    newConfig.IconTintStrength = config.DefaultIconTintStrength;
    newConfig.IconSpacingXPercent = config.DefaultIconSpacingXPercent;
    newConfig.IconSpacingYPercent = config.DefaultIconSpacingYPercent;

    auto corral = std::make_unique<CorralWindow>(newConfig);
    corral->Show();
    corrals.push_back(std::move(corral));
    SaveConfig();
}

void App::CreateCorralAt(POINT pt)
{
    CreateCorral(pt);
}

void App::CreateVirtualCorralAt(POINT pt)
{
    // Show folder browser dialog
    IFileDialog *pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pfd));
    if (FAILED(hr))
        return;

    DWORD dwOptions;
    hr = pfd->GetOptions(&dwOptions);
    if (SUCCEEDED(hr))
    {
        hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }

    if (SUCCEEDED(hr))
    {
        pfd->SetTitle(L"Select Folder for Virtual Corral");
    }

    hr = pfd->Show(messageWindow);
    std::wstring folderPath;
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
                folderPath = pszPath;
                CoTaskMemFree(pszPath);
            }
            psi->Release();
        }
    }
    pfd->Release();

    if (folderPath.empty())
        return;

    // Validate folder
    DWORD attrs = GetFileAttributesW(folderPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBoxW(messageWindow, L"Invalid folder selected.", L"Error", MB_OK | MB_ICONWARNING);
        return;
    }

    // Check for network path
    if (folderPath.length() >= 2 && folderPath[0] == L'\\' && folderPath[1] == L'\\')
    {
        MessageBoxW(messageWindow, L"Network paths are not supported.", L"Error", MB_OK | MB_ICONWARNING);
        return;
    }

    if (folderPath.length() >= 2 && folderPath[1] == L':')
    {
        wchar_t rootPath[4] = {folderPath[0], L':', L'\\', L'\0'};
        if (GetDriveTypeW(rootPath) == DRIVE_REMOTE)
        {
            MessageBoxW(messageWindow, L"Network drives are not supported.", L"Error", MB_OK | MB_ICONWARNING);
            return;
        }
    }

    // Extract folder name for title
    size_t lastSlash = folderPath.find_last_of(L"\\/");
    std::wstring folderName = (lastSlash != std::wstring::npos) ? folderPath.substr(lastSlash + 1) : folderPath;

    // Convert wide string to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, folderPath.c_str(), (int)folderPath.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8Path(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, folderPath.c_str(), (int)folderPath.size(), &utf8Path[0], size, nullptr, nullptr);

    size = WideCharToMultiByte(CP_UTF8, 0, folderName.c_str(), (int)folderName.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8Name(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, folderName.c_str(), (int)folderName.size(), &utf8Name[0], size, nullptr, nullptr);

    // Create config
    CorralWindowConfig newConfig;
    newConfig.Left = (double)pt.x - 150;
    newConfig.Top = (double)pt.y - 100;
    newConfig.Width = 300;
    newConfig.Height = 200;

    CorralTabConfig tab;
    tab.Title = utf8Name;
    tab.ColorHex = config.DefaultColorHex;
    tab.IsVirtual = true;
    tab.VirtualFolderPath = utf8Path;
    tab.IsCatchAll = false; // Virtual corrals cannot be catch-all
    tab.HeaderFontName = config.DefaultHeaderFontName;
    tab.HeaderFontSize = config.DefaultHeaderFontSize;
    tab.HeaderFontColor = config.DefaultHeaderFontColor;
    newConfig.Tabs.push_back(tab);

    // Apply default appearance settings
    newConfig.TitleBarHeight = config.DefaultTitleBarHeight;
    newConfig.IconOpacity = config.DefaultIconOpacity;
    newConfig.IconTintColor = config.DefaultIconTintColor;
    newConfig.IconTintStrength = config.DefaultIconTintStrength;
    newConfig.IconSpacingXPercent = config.DefaultIconSpacingXPercent;
    newConfig.IconSpacingYPercent = config.DefaultIconSpacingYPercent;

    auto corral = std::make_unique<CorralWindow>(newConfig);
    corral->Show();
    corrals.push_back(std::move(corral));
    SaveConfig();
}

bool App::IsDesktopUnderMouse(POINT pt)
{
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd)
        return false;

    wchar_t className[256];
    while (hwnd)
    {
        GetClassNameW(hwnd, className, 256);
        std::wstring classStr(className);

        if (classStr == L"Progman" || classStr == L"WorkerW")
        {
            return true;
        }

        HWND shellWindow = GetShellWindow();
        if (hwnd == shellWindow)
        {
            return true;
        }

        hwnd = GetParent(hwnd);
    }

    return false;
}

bool App::IsAutostartEnabled()
{
    // Check SharedTaskScheduler — this is what RegisterShellExtension actually writes.
    // (ShellServiceObjectDelayLoad is no longer honored for third-party DLLs on Win10/11.)
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SharedTaskScheduler",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        wchar_t value[256];
        DWORD size = sizeof(value);
        DWORD type = REG_SZ;
        LONG result = RegQueryValueExW(hKey, L"{7A3B9E42-D1F8-4C6A-B5E3-9F2A1D8C4E7B}",
                                       nullptr, &type, (LPBYTE)value, &size);
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }
    return false;
}

void App::SetAutostart(bool enable)
{
    // Shell extension mode: register/unregister via DllRegisterServer/DllUnregisterServer
    // For now, autostart is always on when the shell extension is registered
    // The user can unregister via "DexCorral.exe --unregister"
}

LRESULT CALLBACK App::MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    App *app = nullptr;

    if (uMsg == WM_CREATE)
    {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        app = (App *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    }
    else
    {
        app = (App *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (app && uMsg == TrayIcon::WM_TRAYICON)
    {
        if (lParam == WM_RBUTTONUP)
        {
            app->ShowTrayMenu();
            return 0;
        }
        if (lParam == WM_LBUTTONDBLCLK)
        {
            app->ToggleDesktopIcons();
            return 0;
        }
    }

    if (app && uMsg == WM_DISPLAYCHANGE)
    {
        app->OnDisplayChange();
        return 0;
    }

    // Hook retry thread succeeded — refresh hidden icon data now that the hook is active
    if (app && uMsg == (WM_APP + 100))
    {
        app->UpdateHookHiddenIcons();
        app->PositionHiddenIconsUnderCorrals();
        HookBridge::RefreshDesktop();
        return 0;
    }

    // Shell image list changed (e.g. Recycle Bin emptied/filled)
    if (app && uMsg == (WM_APP + 101))
    {
        // Free the PIDL list delivered with SHCNRF_NewDelivery
        LONG lEvent = 0;
        PIDLIST_ABSOLUTE *pidls = nullptr;
        HANDLE hLock = SHChangeNotification_Lock((HANDLE)wParam, (DWORD)lParam, &pidls, &lEvent);
        if (hLock)
        {
            SHChangeNotification_Unlock(hLock);
        }
        app->RefreshAllCorrals();
        return 0;
    }

    // Explorer restarted — re-add the tray icon
    if (app && app->wmTaskbarCreated && uMsg == app->wmTaskbarCreated)
    {
        if (app->trayIcon)
            app->trayIcon->Show();
        return 0;
    }

    // Handle Windows shutdown/logoff
    if (uMsg == WM_QUERYENDSESSION)
    {
        return TRUE;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int App::Run()
{
    Initialize();

    // If we have a stop event (shell extension mode), watch it alongside messages
    if (g_AppStopEvent)
    {
        MSG msg;
        for (;;)
        {
            DWORD result = MsgWaitForMultipleObjects(1, &g_AppStopEvent, FALSE, INFINITE, QS_ALLINPUT);
            if (result == WAIT_OBJECT_0)
            {
                // Stop event signaled — DLL is being unloaded
                break;
            }
            // Process all pending messages
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                    goto done;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    done:
        return 0;
    }

    // Standalone mode (no stop event) — classic message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

void App::EnsureCatchAllCorral()
{
    // Find if any corral is marked as catch-all
    bool foundCatchAll = false;
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            // Virtual corrals cannot be catch-all
            if (tab.IsVirtual)
            {
                tab.IsCatchAll = false;
                continue;
            }

            if (tab.IsCatchAll)
            {
                if (foundCatchAll)
                {
                    // Only one catch-all allowed
                    tab.IsCatchAll = false;
                }
                foundCatchAll = true;
            }
        }
    }

    // If no catch-all exists, make the first non-virtual corral catch-all
    if (!foundCatchAll && !corrals.empty())
    {
        for (auto &corral : corrals)
        {
            bool found = false;
            for (auto &tab : corral->GetConfig().Tabs)
            {
                if (!tab.IsVirtual)
                {
                    tab.IsCatchAll = true;
                    corral->RecalculateLayout();
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
    }
}

CorralWindow *App::GetCatchAllCorral()
{
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            if (tab.IsCatchAll && !tab.IsVirtual)
            {
                return corral.get();
            }
        }
    }
    // Fallback to first non-virtual corral if no catch-all defined
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            if (!tab.IsVirtual)
            {
                return corral.get();
            }
        }
    }
    return nullptr;
}

void App::OnDisplayChange()
{
    if (!monitorManager)
        return;

    // Refresh monitor list
    monitorManager->Refresh();

    // Reposition all corrals
    UpdateCorralPositions();

    // Refresh all corral backgrounds
    RefreshAllCorralBackgrounds();
}

void App::UpdateCorralPositions()
{
    if (!monitorManager)
        return;

    const MonitorInfo *primaryMon = monitorManager->GetPrimaryMonitor();
    if (!primaryMon)
        return;

    bool configChanged = false;

    for (auto &corral : corrals)
    {
        CorralWindowConfig &cfg = corral->GetConfig();
        const std::string &targetId = cfg.TargetMonitorId;

        // Check if target monitor is active
        const MonitorInfo *targetMon = monitorManager->FindMonitor(targetId);

        if (targetMon)
        {
            // Target monitor is active - restore/scale position
            auto it = cfg.MonitorPositions.find(targetId);
            if (it != cfg.MonitorPositions.end())
            {
                MonitorPosition &stored = it->second;

                // Check if resolution changed
                if (stored.RefWidth != targetMon->width || stored.RefHeight != targetMon->height)
                {
                    // Scale position and size based on percentage (stored positions are relative to monitor)
                    double xPercent = (double)stored.Left / stored.RefWidth;
                    double yPercent = (double)stored.Top / stored.RefHeight;
                    double wPercent = (double)stored.Width / stored.RefWidth;
                    double hPercent = (double)stored.Height / stored.RefHeight;

                    // Calculate new relative position on monitor
                    int newRelLeft = (int)(xPercent * targetMon->width);
                    int newRelTop = (int)(yPercent * targetMon->height);
                    int newWidth = (int)(wPercent * targetMon->width);
                    int newHeight = (int)(hPercent * targetMon->height);

                    // Clamp relative position to monitor bounds
                    if (newRelLeft + newWidth > targetMon->width)
                    {
                        newRelLeft = targetMon->width - newWidth;
                    }
                    if (newRelTop + newHeight > targetMon->height)
                    {
                        newRelTop = targetMon->height - newHeight;
                    }
                    if (newRelLeft < 0)
                        newRelLeft = 0;
                    if (newRelTop < 0)
                        newRelTop = 0;

                    // Convert to screen coordinates
                    int screenLeft = targetMon->bounds.left + newRelLeft;
                    int screenTop = targetMon->bounds.top + newRelTop;

                    // Update stored position with new relative values
                    stored.Left = newRelLeft;
                    stored.Top = newRelTop;
                    stored.Width = newWidth;
                    stored.Height = newHeight;
                    stored.RefWidth = targetMon->width;
                    stored.RefHeight = targetMon->height;

                    // Apply to corral (using screen coordinates)
                    cfg.Left = screenLeft;
                    cfg.Top = screenTop;
                    cfg.Width = newWidth;
                    cfg.Height = newHeight;

                    SetWindowPos(corral->GetHWND(), nullptr,
                                 screenLeft, screenTop, newWidth, newHeight,
                                 SWP_NOZORDER | SWP_NOACTIVATE);

                    configChanged = true;
                }
            }
        }
        else if (!targetId.empty())
        {
            // Target monitor is offline - move to primary monitor
            // Calculate position based on percentage from stored position
            auto it = cfg.MonitorPositions.find(targetId);
            if (it != cfg.MonitorPositions.end())
            {
                MonitorPosition &stored = it->second;

                // Calculate percentages from original relative position
                double xPercent = (double)stored.Left / stored.RefWidth;
                double yPercent = (double)stored.Top / stored.RefHeight;
                double wPercent = (double)stored.Width / stored.RefWidth;
                double hPercent = (double)stored.Height / stored.RefHeight;

                // Apply to primary monitor (calculate relative position first)
                int newRelLeft = (int)(xPercent * primaryMon->width);
                int newRelTop = (int)(yPercent * primaryMon->height);
                int newWidth = (int)(wPercent * primaryMon->width);
                int newHeight = (int)(hPercent * primaryMon->height);

                // Clamp relative position to primary monitor bounds
                if (newRelLeft + newWidth > primaryMon->width)
                {
                    newRelLeft = primaryMon->width - newWidth;
                }
                if (newRelTop + newHeight > primaryMon->height)
                {
                    newRelTop = primaryMon->height - newHeight;
                }
                if (newRelLeft < 0)
                    newRelLeft = 0;
                if (newRelTop < 0)
                    newRelTop = 0;

                // Convert to screen coordinates
                int screenLeft = primaryMon->bounds.left + newRelLeft;
                int screenTop = primaryMon->bounds.top + newRelTop;

                // Update current position (but keep TargetMonitorId unchanged so it returns when monitor reconnects)
                cfg.Left = screenLeft;
                cfg.Top = screenTop;
                cfg.Width = newWidth;
                cfg.Height = newHeight;

                SetWindowPos(corral->GetHWND(), nullptr,
                             screenLeft, screenTop, newWidth, newHeight,
                             SWP_NOZORDER | SWP_NOACTIVATE);

                configChanged = true;
            }
        }
    }

    if (configChanged)
    {
        SaveConfig();
    }
}

void App::OnDesktopFileAdded(const std::wstring &fileName)
{
    // Convert to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    // Check if file is already in any corral
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            auto &files = tab.Files;
            if (std::find(files.begin(), files.end(), fileNameStr) != files.end())
            {
                return; // Already tracked
            }
        }
    }

    // Add to catch-all corral
    CorralWindow *catchAll = GetCatchAllCorral();
    if (catchAll)
    {
        catchAll->AddFile(fileNameStr);
        SaveConfig();
    }
}

void App::OnDesktopFileRenamed(const std::wstring &oldName, const std::wstring &newName)
{
    // Convert to UTF-8
    int oldSize = WideCharToMultiByte(CP_UTF8, 0, oldName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string oldNameStr(oldSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, oldName.c_str(), -1, &oldNameStr[0], oldSize, nullptr, nullptr);

    int newSize = WideCharToMultiByte(CP_UTF8, 0, newName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string newNameStr(newSize - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, newName.c_str(), -1, &newNameStr[0], newSize, nullptr, nullptr);

    // Update in all corrals
    bool changed = false;
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            auto &files = tab.Files;
            auto it = std::find(files.begin(), files.end(), oldNameStr);
            if (it != files.end())
            {
                *it = newNameStr;
                corral->LoadFiles();
                changed = true;
            }
        }
    }

    if (changed)
    {
        SaveConfig();
    }
}

void App::OnDesktopFileDeleted(const std::wstring &fileName)
{
    // Convert to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileNameStr(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &fileNameStr[0], size, nullptr, nullptr);

    // Remove from all corrals
    bool changed = false;
    for (auto &corral : corrals)
    {
        for (auto &tab : corral->GetConfig().Tabs)
        {
            auto &files = tab.Files;
            auto it = std::find(files.begin(), files.end(), fileNameStr);
            if (it != files.end())
            {
                files.erase(it);
                corral->LoadFiles();
                changed = true;
            }
        }
    }

    if (changed)
    {
        SaveConfig();
    }
}
