#pragma once
#include <Windows.h>
#include <memory>
#include <vector>
#include "Config.h"
#include "MouseHook.h"
#include "WallpaperManager.h"
#include "TrayIcon.h"
#include "DesktopMonitor.h"

class CorralWindow;

class App {
public:
    App();
    ~App();

    int Run();
    void SaveConfig();
    void RemoveCorral(CorralConfig* config);
    void RemoveFileFromOtherCorrals(const std::wstring& fileName, CorralConfig* exceptCorral);
    void RefreshAllCorrals();
    void RefreshAllCorralBackgrounds();
    void ToggleDesktopIcons();  // Toggle desktop icons visibility
    void ToggleShortcutArrows(); // Toggle shortcut arrow overlay (requires Explorer restart)
    bool IsAutostartEnabled();
    void SetAutostart(bool enable);
    void SetDefaultColorHex(const std::string& colorHex);
    void ApplyColorToAllCorrals(const std::string& colorHex);
    WallpaperManager* GetWallpaperManager() { return wallpaperManager.get(); }
    const std::vector<std::unique_ptr<CorralWindow>>& GetCorrals() const { return corrals; }

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
    std::unique_ptr<WallpaperManager> wallpaperManager;
    std::unique_ptr<TrayIcon> trayIcon;
    std::unique_ptr<DesktopMonitor> desktopMonitor;
    std::vector<std::unique_ptr<CorralWindow>> corrals;
};
