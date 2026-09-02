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
 * App.h - Main application controller
 *
 * Manages application initialization, shutdown, and the main message loop.
 * Coordinates corral window creation/destruction, appearance synchronization,
 * tray icon, desktop monitoring, and icon push-out-of-way
 * when corrals overlap with desktop icons.
 */

#pragma once
#include <Windows.h>
#include <memory>
#include <mutex>
#include <vector>
#include <map>
#include <string>
#include "Config.h"
#include "DesktopIcons.h"
#include "HookBridge.h"
#include "MouseHook.h"
#include "TrayIcon.h"
#include "DesktopMonitor.h"
#include "MonitorManager.h"

class CorralWindow;

/**
 * Main application class managing corrals, configuration, and system integration.
 */
class App
{
public:
    /// Constructor initializing the application
    App();
    ~App();

    /// Starts the application and runs the main message loop. Returns exit code.
    int Run();

    /// Saves the current configuration to disk
    void SaveConfig();

    /// Removes a corral and all its tabs
    void RemoveCorral(CorralWindowConfig *config);

    /// Removes a file from all corrals except the specified tab
    void RemoveFileFromOtherCorrals(const std::wstring &fileName, CorralTabConfig *exceptTab);

    /// Refreshes all corrals' content (folder contents, icons, etc.)
    void RefreshAllCorrals();

    /// Refreshes background images for all corrals (wallpaper or custom backgrounds)
    void RefreshAllCorralBackgrounds();

    /// Toggles visibility of desktop icons
    void ToggleDesktopIcons();

    /**
     * Quick-hide: hides/shows everything at once — native desktop icons plus
     * all corral windows (fade animation). Triggered by double-clicking an
     * empty spot on the desktop. Corrals with ExcludeFromQuickHide stay visible.
     */
    void ToggleQuickHide();

    /// Returns true while quick-hide is active (everything hidden)
    bool IsQuickHideActive() const { return quickHideActive; }

    /// Toggles shortcut arrow overlay on icons (requires Explorer restart)
    void ToggleShortcutArrows();

    /// Sets the default corral background color (hex format, e.g., "FF0000" for red)
    void SetDefaultColorHex(const std::string &colorHex);

    /// Sets the appearance new corrals and tabs are created with.
    /// ColorHex is handled separately by SetDefaultColorHex and is ignored here.
    void SetDefaultAppearance(const AppearanceSettings &settings);

    /// Applies the specified background color to all existing corrals
    void ApplyColorToAllCorrals(const std::string &colorHex);

    /**
     * Applies appearance settings to all existing corrals.
     * Only the parts marked in `apply` are written; the rest are left alone.
     * Font settings land on each corral's active tab, matching where the
     * Appearance dialog edits them.
     */
    void ApplyAppearanceToAllCorrals(const AppearanceSettings &settings,
                                     const AppearanceApplyFlags &apply);

    /**
     * Creates a new corral centred on `desiredCenter`, nudged to the nearest
     * spot that does not overlap an existing corral. Every creation path goes
     * through here, varying only the seed point; `exclude` drops one corral
     * from the overlap test (the source, when creating from a corral's own menu
     * or by detaching a tab).
     */
    void CreateCorralAt(POINT desiredCenter, HWND exclude = nullptr);

    /// Creates a new virtual (non-file) corral, placed like CreateCorralAt
    void CreateVirtualCorralAt(POINT desiredCenter);

    /**
     * Finds a center point for a new corral of the given size as close as
     * possible to desiredTopLeft while staying inside the work area of the
     * monitor under that point and not overlapping any existing corral. The
     * corral whose HWND equals `exclude` is ignored (pass the source window so
     * a detached tab can sit right beside its origin). Falls back to the
     * clamped desired position if no free spot exists or the nearest one is
     * further away than the shift cap.
     */
    POINT FindNearestFreeCorralPosition(POINT desiredTopLeft, int width, int height, HWND exclude = nullptr);

    /// DPI of the monitor under `pt` (96 when it cannot be determined).
    /// Usable before a window exists, unlike CorralWindow::Dpi.
    static UINT DpiForPoint(POINT pt);

    /// Default size for a new corral, scaled for the monitor under `nearPt`.
    static SIZE DefaultCorralSize(POINT nearPt);

    /**
     * Restacks the corral band: all corrals stay below ordinary windows, but
     * `newTop` (when given, and remembered afterwards) becomes the topmost
     * among its peers. This replaces per-window "sink me to the bottom" calls,
     * which sank the corral the user was interacting with below its siblings.
     */
    void RepinBand(CorralWindow *newTop = nullptr);

    /// Forgets a corral that is going away (band top / hover-expand arbitration)
    void ForgetCorral(CorralWindow *corral);

    /**
     * Hover-expand arbitration: only one rolled-up corral may hover-expand at a
     * time, so an exposed sliver of a neighbour cannot fight the corral the user
     * is actually in. Returns false when another corral currently owns it.
     */
    bool BeginHoverExpand(CorralWindow *corral);
    void EndHoverExpand(CorralWindow *corral);

    /// Returns pointer to monitor manager (tracks display configuration)
    MonitorManager *GetMonitorManager() { return monitorManager.get(); }

    /// Returns const reference to the list of all active corrals
    const std::vector<std::unique_ptr<CorralWindow>> &GetCorrals() const { return corrals; }

    /// Called when display configuration changes (monitors added/removed/resolution changed)
    void OnDisplayChange();

    /// Repositions all corrals based on current monitor layout
    void UpdateCorralPositions();

    /// Returns the singleton App instance
    static App *GetInstance() { return instance; }

    /// Caches current desktop icon positions for push-out-of-way algorithm
    void CacheDesktopIconPositions();

    /// Invalidates cached desktop icon positions (forces refresh on next use)
    void InvalidateDesktopIconCache();

    /// Moves desktop icons out of the way if they overlap with corrals
    void PushDesktopIconsFromCorrals();

    void UpdateHookHiddenIcons();           // Push hidden icon list to shared memory
    void PositionHiddenIconsUnderCorrals(); // Reposition hidden icons to match corral scroll state

    /**
     * Keeps an old identity (display + parsing name) hidden for a grace
     * period after a rename. The shell updates the desktop item
     * asynchronously, so for a moment the item still carries the pre-rename
     * identity — hiding both prevents the icon from flickering onto the
     * desktop during the transition.
     */
    void AddTransientHiddenIcon(const std::wstring &displayName, const std::wstring &parsingName);

private:
    void Initialize();
    void Shutdown();
    void LoadConfig();
    void RestoreCorrals();
    void ShowTrayMenu();
    void ShowAbout();

    /// Switches the UI language at runtime and persists the choice.
    void ApplyLanguage(const std::string &langCode);

    // Opt-in update check (AppConfig::CheckForUpdates). Spawns an async GitHub
    // Releases query; the result arrives as WM_UPDATE_CHECK_DONE. userInitiated
    // bypasses the 24h throttle and surfaces "up to date" / "couldn't check".
    void StartUpdateCheck(bool userInitiated);
    std::wstring pendingUpdateUrl; // Release page opened when the update balloon is clicked
    void ShowCreationMenu(POINT pt);

    /// Appearance defaults for a brand-new corral / tab. Single source of truth
    /// for every creation path — geometry is the caller's business.
    CorralWindowConfig MakeDefaultCorralConfig() const;
    CorralTabConfig MakeDefaultTabConfig(const std::string &title) const;
    bool IsDesktopUnderMouse(POINT pt);

    /**
     * Returns true if the point is on an empty spot of the desktop: the window
     * under the point is Explorer's desktop (ListView/DefView/Progman/WorkerW,
     * not a corral or another app) and — when checkIcons — no icon is hit there.
     */
    bool IsPointOnEmptyDesktop(POINT pt, bool checkIcons);

    void OnLeftButtonDown(POINT pt);
    void OnLeftButtonUp(POINT pt);
    void OnMouseMove(POINT pt);

    // Quick-hide state
    bool quickHideActive = false;
    bool desktopIconsVisibleBeforeQuickHide = true; // Restored when quick-hide ends

    // Double-click detection for the low-level mouse hook (WH_MOUSE_LL delivers
    // no WM_LBUTTONDBLCLK, so pair consecutive clicks manually)
    DWORD lastClickTick = 0;
    POINT lastClickPt = {};

    // Desktop monitoring callbacks
    void EnsureCatchAllCorral();
    CorralWindow *GetCatchAllCorral();
    void OnDesktopFileAdded(const std::wstring &fileName);
    void OnDesktopFileRenamed(const std::wstring &oldName, const std::wstring &newName);
    void OnDesktopFileDeleted(const std::wstring &fileName);

    // Deferred catch-all adoption: new desktop files are queued and only
    // adopted once Explorer's inline rename (New > ... edit box) has finished
    void ProcessPendingAdoptions();
    std::vector<std::wstring> pendingAdoptions;
    std::mutex pendingAdoptionsLock;

    // Transition aliases for renames (see AddTransientHiddenIcon)
    struct TransientHiddenIcon
    {
        HiddenIconInfo icon;
        DWORD expiresAtTick;
    };
    std::vector<TransientHiddenIcon> transientHiddenIcons;
    std::mutex transientHiddenLock;

    static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Fires whenever some other window becomes the foreground window, and posts
    // WM_REPIN_CORRALS so the app thread restacks the corral band (RepinBand).
    // HWND_BOTTOM is a one-time placement, not a persistent style (unlike
    // WS_EX_TOPMOST for the top side) — without this, ordinary window activation
    // elsewhere can leave a corral sitting above other apps again. The callback
    // only posts: see WM_REPIN_CORRALS in App.cpp for why touching windows here
    // deadlocks Explorer.
    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                      LONG idObject, LONG idChild, DWORD idEventThread, DWORD idEventTime);

    static App *instance;

    HWND messageWindow;
    UINT wmTaskbarCreated = 0; // RegisterWindowMessage("TaskbarCreated")
    ULONG shellNotifyId = 0;   // SHChangeNotifyRegister token
    HWINEVENTHOOK foregroundHook = nullptr;
    AppConfig config;
    std::string configPath;
    std::unique_ptr<MouseHook> mouseHook;
    std::unique_ptr<TrayIcon> trayIcon;
    std::unique_ptr<DesktopMonitor> desktopMonitor;
    std::unique_ptr<MonitorManager> monitorManager;
    std::vector<std::unique_ptr<CorralWindow>> corrals;

    // Topmost corral within the band (see RepinBand). Raw pointer into `corrals`;
    // cleared by ForgetCorral when the window goes away.
    CorralWindow *topCorral = nullptr;

    // The corral currently hover-expanded, if any (see BeginHoverExpand)
    CorralWindow *hoverExpandedCorral = nullptr;

    // Desktop icon push cache (free icons only — corral-owned ones filtered out)
    std::vector<DesktopIconInfo> cachedDesktopIconPositions;
    bool desktopIconCacheValid = false;
    std::vector<RECT> GetAllCorralRects() const;

    // Identities (display + parsing name) of every icon owned by any corral —
    // shared by hidden-list updates and the push cache filter
    std::vector<HiddenIconInfo> CollectCorralIconIdentities() const;
};
