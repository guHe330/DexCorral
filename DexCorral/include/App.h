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
#include <vector>
#include <map>
#include <string>
#include "Config.h"
#include "DesktopIcons.h"
#include "MouseHook.h"
#include "TrayIcon.h"
#include "DesktopMonitor.h"
#include "MonitorManager.h"

class CorralWindow;

/**
 * Main application class managing corrals, configuration, and system integration.
 */
class App {
public:
    /// Constructor initializing the application
    App();
    ~App();

    /// Starts the application and runs the main message loop. Returns exit code.
    int Run();

    /// Saves the current configuration to disk
    void SaveConfig();

    /// Removes a corral and all its tabs
    void RemoveCorral(CorralWindowConfig* config);

    /// Removes a file from all corrals except the specified tab
    void RemoveFileFromOtherCorrals(const std::wstring& fileName, CorralTabConfig* exceptTab);

    /// Refreshes all corrals' content (folder contents, icons, etc.)
    void RefreshAllCorrals();

    /// Refreshes background images for all corrals (wallpaper or custom backgrounds)
    void RefreshAllCorralBackgrounds();

    /// Toggles visibility of desktop icons
    void ToggleDesktopIcons();

    /// Toggles shortcut arrow overlay on icons (requires Explorer restart)
    void ToggleShortcutArrows();

    /// Returns true if autostart on Windows login is enabled
    bool IsAutostartEnabled();

    /// Enables or disables autostart on Windows login
    void SetAutostart(bool enable);

    /// Sets the default corral background color (hex format, e.g., "FF0000" for red)
    void SetDefaultColorHex(const std::string& colorHex);

    /**
     * Sets default appearance for all new corrals.
     * Parameters: titleBarHeight, fontName, fontSize, fontColor (hex),
     * iconOpacity (0-100), tintColor (hex), tintStrength (0-100), spacingX, spacingY (pixels)
     */
    void SetDefaultAppearance(int titleBarHeight, const std::string& fontName,
        int fontSize, const std::string& fontColor, int iconOpacity,
        const std::string& tintColor, int tintStrength,
        int spacingX, int spacingY);

    /// Applies the specified background color to all existing corrals
    void ApplyColorToAllCorrals(const std::string& colorHex);

    /**
     * Applies appearance settings to all existing corrals with selective updating.
     * Boolean flags determine which settings are applied (changed flags pattern).
     * Only settings with corresponding flags set to true are updated.
     */
    void ApplyAppearanceToAllCorrals(const std::string& colorHex, bool applyColor,
        int titleBarHeight, bool applyHeight,
        const std::string& fontName, int fontSize, bool applyFont,
        const std::string& fontColor, bool applyFontColor,
        int iconOpacity, bool applyIconOpacity,
        const std::string& tintColor, int tintStrength, bool applyTint,
        int spacingX, int spacingY, bool applySpacing);

    /// Creates a new corral at the specified screen coordinates
    void CreateCorralAt(POINT pt);

    /// Creates a new virtual (non-file) corral at the specified screen coordinates
    void CreateVirtualCorralAt(POINT pt);

    /// Returns pointer to monitor manager (tracks display configuration)
    MonitorManager* GetMonitorManager() { return monitorManager.get(); }

    /// Returns const reference to the list of all active corrals
    const std::vector<std::unique_ptr<CorralWindow>>& GetCorrals() const { return corrals; }

    /// Called when display configuration changes (monitors added/removed/resolution changed)
    void OnDisplayChange();

    /// Repositions all corrals based on current monitor layout
    void UpdateCorralPositions();

    /// Returns the singleton App instance
    static App* GetInstance() { return instance; }

    /// Caches current desktop icon positions for push-out-of-way algorithm
    void CacheDesktopIconPositions();

    /// Invalidates cached desktop icon positions (forces refresh on next use)
    void InvalidateDesktopIconCache();

    /// Moves desktop icons out of the way if they overlap with corrals
    void PushDesktopIconsFromCorrals();

    void UpdateHookHiddenIcons();              // Push hidden icon list to shared memory
    void PositionHiddenIconsUnderCorrals();    // Reposition hidden icons to match corral scroll state

private:
    void Initialize();
    void Shutdown();
    void LoadConfig();
    void RestoreCorrals();
    void ShowTrayMenu();
    void ShowAbout();
    void ShowCreationMenu(POINT pt);
    void CreateCorral(POINT pt);
    bool IsDesktopUnderMouse(POINT pt);

    void OnLeftButtonDown(POINT pt);
    void OnLeftButtonUp(POINT pt);
    void OnMouseMove(POINT pt);

    // Desktop monitoring callbacks
    void EnsureCatchAllCorral();
    CorralWindow* GetCatchAllCorral();
    void OnDesktopFileAdded(const std::wstring& fileName);
    void OnDesktopFileRenamed(const std::wstring& oldName, const std::wstring& newName);
    void OnDesktopFileDeleted(const std::wstring& fileName);

    static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static App* instance;

    HWND messageWindow;
    AppConfig config;
    std::string configPath;
    std::unique_ptr<MouseHook> mouseHook;
    std::unique_ptr<TrayIcon> trayIcon;
    std::unique_ptr<DesktopMonitor> desktopMonitor;
    std::unique_ptr<MonitorManager> monitorManager;
    std::vector<std::unique_ptr<CorralWindow>> corrals;

    // Desktop icon push cache
    std::map<std::wstring, POINT2D> cachedDesktopIconPositions;
    bool desktopIconCacheValid = false;
    bool IsIconHiddenByCorral(const std::wstring& displayName) const;
    std::vector<RECT> GetAllCorralRects() const;
};
