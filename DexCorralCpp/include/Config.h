#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct CorralConfig {
    double Left = 0;
    double Top = 0;
    double Width = 300;
    double Height = 200;
    std::string Title = "New Corral";
    std::string ColorHex = "#99000000";
    bool IsRolledUp = false;
    bool IsCatchAll = false;  // First corral is catch-all for new desktop files
    std::vector<std::string> Files;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(CorralConfig, Left, Top, Width, Height, Title, ColorHex, IsRolledUp, IsCatchAll, Files)
};

struct AppConfig {
    std::vector<CorralConfig> Corrals;
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
