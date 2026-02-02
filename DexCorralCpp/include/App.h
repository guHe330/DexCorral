#pragma once
#include <Windows.h>
#include <memory>
#include <vector>
#include "Config.h"
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
    void HideRandomDesktopIcon();  // Experiment: hide random icon via ListView manipulation
    void RestoreHiddenIcons();     // Experiment: restore hidden icons by refreshing desktop
    bool IsAutostartEnabled();
    void SetAutostart(bool enable);
    void SetDefaultColorHex(const std::string& colorHex);
    void ApplyColorToAllCorrals(const std::string& colorHex);
    void CreateCorralAt(POINT pt);  // Create new corral at specified position
    void CreateVirtualCorralAt(POINT pt);  // Create new virtual corral at specified position
    WallpaperManager* GetWallpaperManager() { return wallpaperManager.get(); }
    MonitorManager* GetMonitorManager() { return monitorManager.get(); }
    const std::vector<std::unique_ptr<CorralWindow>>& GetCorrals() const { return corrals; }

    // Multi-monitor support
    void OnDisplayChange();  // Called when monitors are added/removed/resolution changed
    void UpdateCorralPositions();  // Reposition corrals based on monitor changes

    static App* GetInstance() { return instance; }

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
    // Removed: Mousewheel events now pass through naturally
    // void OnMouseWheel(POINT pt, int delta);

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
};
