#pragma once
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

// View mode for corral icons
enum class ViewMode : int {
    SmallIcons = 0,   // 32px icons in grid
    MediumIcons = 1,  // 48px icons in grid
    LargeIcons = 2,   // 64px icons in grid
    Details = 3       // List view with columns (name, type, size, date, sync status)
};

// Position snapshot for a specific monitor at a specific resolution
struct MonitorPosition {
    int Left = 0;
    int Top = 0;
    int Width = 300;
    int Height = 200;
    int RefWidth = 1920;   // Resolution when position was stored
    int RefHeight = 1080;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MonitorPosition, Left, Top, Width, Height, RefWidth, RefHeight)
};

struct CorralTabConfig {
    std::string Title = "New Tab";
    std::string ColorHex = "#99000000";
    std::vector<std::string> Files;
    int ViewModeInt = 0;  // ViewMode as int for JSON serialization (0=Small, 1=Medium, 2=Large, 3=Details)
    bool IsCatchAll = false;
    bool IsVirtual = false;              // True if this corral mirrors a folder
    std::string VirtualFolderPath;       // UTF-8 path to the local folder (empty if not virtual)

    // Helper to get/set ViewMode enum
    ViewMode GetViewMode() const { return static_cast<ViewMode>(ViewModeInt); }
    void SetViewMode(ViewMode mode) { ViewModeInt = static_cast<int>(mode); }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CorralTabConfig, Title, ColorHex, Files, ViewModeInt, IsCatchAll, IsVirtual, VirtualFolderPath)
};

struct CorralWindowConfig {
    double Left = 0;
    double Top = 0;
    double Width = 300;
    double Height = 200;
    bool IsRolledUp = false;
    std::vector<CorralTabConfig> Tabs;
    int ActiveTabIndex = 0;

    // Multi-monitor support
    std::string TargetMonitorId;  // Hardware ID of the monitor this corral belongs to
    std::map<std::string, MonitorPosition> MonitorPositions;  // Position per monitor

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CorralWindowConfig, Left, Top, Width, Height, IsRolledUp, Tabs, ActiveTabIndex, TargetMonitorId, MonitorPositions)
};

struct AppConfig {
    std::vector<CorralWindowConfig> Corrals;
    bool DesktopIconsVisible = true;
    std::string DefaultColorHex = "#99000000";  // Default appearance for new corrals
    bool HideShortcutArrows = false;  // Hide the small arrow overlay on shortcut icons

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AppConfig, Corrals, DesktopIconsVisible, DefaultColorHex, HideShortcutArrows)
};

class Config {
public:
    static std::string GetConfigPath();
    static AppConfig Load();
    static void Save(const AppConfig& config);
};
