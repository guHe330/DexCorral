#pragma once
#include <Windows.h>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "Config.h"
#include "DesktopIcons.h"
#include "MouseHook.h"
#include "WallpaperManager.h"
#include "TrayIcon.h"
#include "DesktopMonitor.h"
#include "MonitorManager.h"

class CorralWindow;

class App {
public:
    App();
    ~App();

    int Run();
    void SaveConfig();
    void RemoveCorral(CorralWindowConfig* config);
    void RemoveFileFromOtherCorrals(const std::wstring& fileName, CorralTabConfig* exceptTab);
    void RefreshAllCorrals();
    void RefreshAllCorralBackgrounds();
    void ToggleDesktopIcons();  // Toggle desktop icons visibility
    void ToggleShortcutArrows(); // Toggle shortcut arrow overlay (requires Explorer restart)
    bool IsAutostartEnabled();
    void SetAutostart(bool enable);
    void SetDefaultColorHex(const std::string& colorHex);
    void SetDefaultAppearance(int titleBarHeight, const std::string& fontName,
        int fontSize, const std::string& fontColor, int iconOpacity,
        const std::string& tintColor, int tintStrength);
    void ApplyColorToAllCorrals(const std::string& colorHex);
    void ApplyAppearanceToAllCorrals(const std::string& colorHex, bool applyColor,
        int titleBarHeight, bool applyHeight,
        const std::string& fontName, int fontSize, bool applyFont,
        const std::string& fontColor, bool applyFontColor,
        int iconOpacity, bool applyIconOpacity,
        const std::string& tintColor, int tintStrength, bool applyTint);
    void CreateCorralAt(POINT pt);  // Create new corral at specified position
    void CreateVirtualCorralAt(POINT pt);  // Create new virtual corral at specified position
    WallpaperManager* GetWallpaperManager() { return wallpaperManager.get(); }
    MonitorManager* GetMonitorManager() { return monitorManager.get(); }
    const std::vector<std::unique_ptr<CorralWindow>>& GetCorrals() const { return corrals; }

    // Multi-monitor support
    void OnDisplayChange();  // Called when monitors are added/removed/resolution changed
    void UpdateCorralPositions();  // Reposition corrals based on monitor changes

    static App* GetInstance() { return instance; }

    // Desktop icon push-out-of-way support (public: called from CorralWindow during drag)
    void CacheDesktopIconPositions();
    void InvalidateDesktopIconCache();
    void PushDesktopIconsFromCorrals();

private:
    void Initialize();
    void Shutdown();
    void LoadConfig();
    void RestoreCorrals();
    void ShowTrayMenu();
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

    // Watchdog support
    void StartWatchdog();
    void SignalGracefulExit();

    static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static App* instance;

    HWND messageWindow;
    AppConfig config;
    std::string configPath;
    std::unique_ptr<MouseHook> mouseHook;
    std::unique_ptr<WallpaperManager> wallpaperManager;
    std::unique_ptr<TrayIcon> trayIcon;
    std::unique_ptr<DesktopMonitor> desktopMonitor;
    std::unique_ptr<MonitorManager> monitorManager;
    std::vector<std::unique_ptr<CorralWindow>> corrals;

    // Watchdog support
    HANDLE hGracefulExitEvent = nullptr;

    // Hook integration (in-process shell extension)
    void UpdateHookHiddenIcons();              // Push hidden icon list to shared memory
    void PositionHiddenIconsUnderCorrals();    // Move hidden icons under corral windows for drag-drop occlusion

    // Desktop icon push cache
    std::map<std::wstring, POINT2D> cachedDesktopIconPositions;
    bool desktopIconCacheValid = false;
    bool IsIconHiddenByCorral(const std::wstring& displayName) const;
    std::vector<RECT> GetAllCorralRects() const;
};
