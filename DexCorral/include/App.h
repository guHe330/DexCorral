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

    /**
     * Sets default appearance for all new corrals.
     * Parameters: titleBarHeight, fontName, fontSize, fontColor (hex),
     * iconOpacity (0-100), tintColor (hex), tintStrength (0-100), spacingX, spacingY (pixels)
     */
    void SetDefaultAppearance(int titleBarHeight, const std::string &fontName,
                              int fontSize, const std::string &fontColor, int iconOpacity,
                              const std::string &tintColor, int tintStrength,
                              int spacingX, int spacingY);

    /// Applies the specified background color to all existing corrals
    void ApplyColorToAllCorrals(const std::string &colorHex);

    /**
     * Applies appearance settings to all existing corrals with selective updating.
     * Boolean flags determine which settings are applied (changed flags pattern).
     * Only settings with corresponding flags set to true are updated.
     */
    void ApplyAppearanceToAllCorrals(const std::string &colorHex, bool applyColor,
                                     int titleBarHeight, bool applyHeight,
                                     const std::string &fontName, int fontSize, bool applyFont,
                                     const std::string &fontColor, bool applyFontColor,
                                     int iconOpacity, bool applyIconOpacity,
                                     const std::string &tintColor, int tintStrength, bool applyTint,
                                     int spacingX, int spacingY, bool applySpacing);

    /// Creates a new corral at the specified screen coordinates
    void CreateCorralAt(POINT pt);

    /// Creates a new virtual (non-file) corral at the specified screen coordinates
    void CreateVirtualCorralAt(POINT pt);

    /**
     * Finds a center point for a new corral of the given size that does not
     * overlap any existing corral. Tiles from the top-right corner of the
     * primary monitor's work area, walking columns right-to-left and rows
     * top-to-bottom. Falls back to a cascade from the top-right corner if no
     * free tile exists.
     */
    POINT FindFreeCorralPosition(int width, int height);

    /**
     * Finds a center point for a new corral of the given size as close as
     * possible to desiredTopLeft while staying inside the work area of the
     * monitor under that point and not overlapping any existing corral. The
     * corral whose HWND equals `exclude` is ignored (pass the source window so
     * a detached tab can sit right beside its origin). Falls back to the
     * clamped desired position if no free spot exists.
     */
    POINT FindNearestFreeCorralPosition(POINT desiredTopLeft, int width, int height, HWND exclude = nullptr);

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

    // Opt-in update check (AppConfig::CheckForUpdates). Spawns an async GitHub
    // Releases query; the result arrives as WM_UPDATE_CHECK_DONE. userInitiated
    // bypasses the 24h throttle and surfaces "up to date" / "couldn't check".
    void StartUpdateCheck(bool userInitiated);
    std::wstring pendingUpdateUrl; // Release page opened when the update balloon is clicked
    void ShowCreationMenu(POINT pt);
    void CreateCorral(POINT pt);
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

    // Reasserts SendToBottom() on every corral whenever some other window
    // becomes the foreground window. HWND_BOTTOM is a one-time placement, not
    // a persistent style (unlike WS_EX_TOPMOST for the top side) — without
    // this, ordinary window activation elsewhere can leave a corral sitting
    // above other apps again.
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

    // Desktop icon push cache (free icons only — corral-owned ones filtered out)
    std::vector<DesktopIconInfo> cachedDesktopIconPositions;
    bool desktopIconCacheValid = false;
    std::vector<RECT> GetAllCorralRects() const;

    // Identities (display + parsing name) of every icon owned by any corral —
    // shared by hidden-list updates and the push cache filter
    std::vector<HiddenIconInfo> CollectCorralIconIdentities() const;
};
