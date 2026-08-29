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
 * Config.h - Configuration data structures
 *
 * Defines the JSON-serializable configuration structs for corrals, tabs, and application settings.
 * Uses nlohmann/json with NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT macro for automatic
 * JSON serialization/deserialization. Missing fields get default values instead of throwing.
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

/// Icon display mode for corral windows (small to large icons or details list)
enum class ViewMode : int
{
    SmallIcons = 0,  /// 32px icons in grid layout
    MediumIcons = 1, /// 48px icons in grid layout
    LargeIcons = 2,  /// 64px icons in grid layout
    Details = 3      /// List view with columns (name, type, size, modified date, sync status)
};

/// Position snapshot for a specific monitor at a specific resolution
struct MonitorPosition
{
    int Left = 0;         /// Window left edge
    int Top = 0;          /// Window top edge
    int Width = 300;      /// Window width
    int Height = 200;     /// Window height
    int RefWidth = 1920;  /// Reference monitor width when position was saved
    int RefHeight = 1080; /// Reference monitor height when position was saved

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MonitorPosition, Left, Top, Width, Height, RefWidth, RefHeight)
};

/// Configuration for a single tab within a corral window
struct CorralTabConfig
{
    std::string Title = "New Tab";           /// User-friendly tab name
    std::string ColorHex = "#99000000";      /// Background color (ARGB hex format)
    std::vector<std::string> Files;          /// List of file names or "shell:{CLSID}" for special icons
    int ViewModeInt = 0;                     /// View mode (0=Small, 1=Medium, 2=Large, 3=Details)
    bool IsCatchAll = false;                 /// True if this is the automatic catch-all folder tab
    bool IsVirtual = false;                  /// True if this tab mirrors a local folder
    std::string VirtualFolderPath;           /// UTF-8 path to folder (empty if not virtual)
    std::string CurrentSubPath;              /// UTF-8 sub-path relative to VirtualFolderPath (empty = at root)

    // Details-view column state (virtual corrals)
    std::vector<int> DetailsColumnWidths;    /// Logical-px widths for [Name, Type, Size, Date]; empty = seed from defaults
    int DetailsSortColumn = 0;               /// Sort column: 0=Name, 1=Type, 2=Size, 3=Date
    bool DetailsSortAscending = true;        /// Sort direction

    // Header font appearance (per-tab)
    std::string HeaderFontName = "Segoe UI"; /// Font face for header text
    int HeaderFontSize = 10;                 /// Font size in points (matches font picker)
    std::string HeaderFontColor = "#FFFFFF"; /// RGB hex color for header text

    /// Returns the current view mode as enum
    ViewMode GetViewMode() const { return static_cast<ViewMode>(ViewModeInt); }
    /// Sets the view mode from enum
    void SetViewMode(ViewMode mode) { ViewModeInt = static_cast<int>(mode); }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CorralTabConfig, Title, ColorHex, Files, ViewModeInt, IsCatchAll, IsVirtual, VirtualFolderPath, CurrentSubPath, DetailsColumnWidths, DetailsSortColumn, DetailsSortAscending, HeaderFontName, HeaderFontSize, HeaderFontColor)
};

/// Configuration for a single corral window
struct CorralWindowConfig
{
    double Left = 0;
    double Top = 0;
    double Width = 300;
    double Height = 200;
    bool IsRolledUp = false;
    bool ExcludeFromQuickHide = false; // Corral stays visible during quick-hide (double-click desktop)
    std::vector<CorralTabConfig> Tabs;
    int ActiveTabIndex = 0;

    // Multi-monitor support
    std::string TargetMonitorId;                             // Hardware ID of the monitor this corral belongs to
    std::map<std::string, MonitorPosition> MonitorPositions; // Position per monitor

    // Appearance settings (per-corral)
    int TitleBarHeight = 32;               // Header height in pixels (20-64), applies to all tabs
    int IconOpacity = 255;                 // Icon transparency (0=invisible, 255=opaque)
    std::string IconTintColor = "#000000"; // Tint color for icons (RGB hex)
    int IconTintStrength = 0;              // Tint strength (0=none, 255=full overlay)
    int IconSpacingXPercent = 100;         // Horizontal icon spacing (50-200%)
    int IconSpacingYPercent = 100;         // Vertical icon spacing (50-200%)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CorralWindowConfig, Left, Top, Width, Height, IsRolledUp, ExcludeFromQuickHide, Tabs, ActiveTabIndex, TargetMonitorId, MonitorPositions, TitleBarHeight, IconOpacity, IconTintColor, IconTintStrength, IconSpacingXPercent, IconSpacingYPercent)
};

struct AppConfig
{
    std::vector<CorralWindowConfig> Corrals;
    bool DesktopIconsVisible = true;
    std::string DefaultColorHex = "#7F0000FF"; // Default appearance for new corrals
    bool HideShortcutArrows = false;           // Hide the small arrow overlay on shortcut icons

    // Default appearance for new corrals/tabs
    int DefaultTitleBarHeight = 26;
    std::string DefaultHeaderFontName = "Segoe UI Semibold"; // Applied to each new tab's header font
    int DefaultHeaderFontSize = 10;
    std::string DefaultHeaderFontColor = "#FFFFFF";
    int DefaultIconOpacity = 210;
    std::string DefaultIconTintColor = "#0000FF";
    int DefaultIconTintStrength = 28;
    int DefaultIconSpacingXPercent = 100;
    int DefaultIconSpacingYPercent = 100;
    bool DebugLogging = false;             // Write debug log files (dllmain.log, CorralHook.log, CorralHook_DropTarget.log, CorralDrop.log)

    // Update check (opt-in, off by default). Checks the GitHub Releases API at
    // most once per 24h on startup and notifies via a tray balloon — no auto-download.
    bool CheckForUpdates = false;
    long long LastUpdateCheckEpoch = 0;    // Unix time of the last automatic check (throttle)

    // UI language code ("en", "de"). Empty = use the installer's choice
    // (HKCU\Software\DexCorral\Language), falling back to English.
    std::string Language = "";

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AppConfig, Corrals, DesktopIconsVisible, DefaultColorHex, HideShortcutArrows, DefaultTitleBarHeight, DefaultHeaderFontName, DefaultHeaderFontSize, DefaultHeaderFontColor, DefaultIconOpacity, DefaultIconTintColor, DefaultIconTintStrength, DefaultIconSpacingXPercent, DefaultIconSpacingYPercent, DebugLogging, CheckForUpdates, LastUpdateCheckEpoch, Language)
};

class Config
{
public:
    static std::string GetConfigPath();
    static AppConfig Load();
    static void Save(const AppConfig &config);
};
