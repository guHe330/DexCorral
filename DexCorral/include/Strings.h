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
 * Strings.h - Centralized catalog of user-facing UI strings (i18n)
 *
 * Every user-facing string lives behind the Str enum so adding a language is a
 * data task rather than a code change. Every shipped language is compiled in;
 * English is the fallback and Tr() never returns null.
 *
 * Technical strings (window class names, font names, IPC object names, registry
 * paths, printf formats, URLs) must NOT go through this catalog.
 */

#pragma once
#include <string>

enum class Str
{
    // Common
    App_Name,                    // "DexCorral"
    Title_Error,                 // "Error"
    Btn_OK,                      // "OK"
    Btn_Cancel,                  // "Cancel"

    // Tray icon / tray menu (App.cpp)
    Tray_Tooltip,                // "DexCorral - Double-click desktop to hide icons"
    Menu_About,                  // "About"
    Menu_CreateNewCorral,        // "Create New Corral"
    Menu_NewVirtualCorral,       // "New Virtual Corral"
    Menu_ShowDesktopIcons,       // "Show Desktop Icons"
    Menu_QuickHideEverything,    // "Quick-Hide Everything"
    Menu_CheckUpdatesAuto,       // "Check for Updates Automatically"
    Menu_CheckUpdatesNow,        // "Check for Updates Now"

    // Safe-mode notice (App.cpp)
    SafeMode_Title,              // "DexCorral started in safe mode"
    SafeMode_Body,               // explanation balloon text

    // About dialog (App.cpp)
    Title_About,                 // "About DexCorral"
    About_Body,                  // GPL notice; {0} = version

    // Shortcut-arrow toggle (App.cpp)
    Arrow_HideConfirm,           // hide arrows confirmation
    Arrow_RestoreConfirm,        // restore arrows confirmation
    Arrow_ChangeFailed,          // failure notice

    // Update check balloons (App.cpp)
    Update_AvailableTitle,       // "DexCorral update available"
    Update_AvailableBody,        // {0} = new version
    Update_UpToDateTitle,        // "DexCorral is up to date"
    Update_UpToDateBody,         // {0} = current version
    Update_FailedTitle,          // "Update check failed"
    Update_FailedBody,           // retry-later text

    // Virtual corral folder selection (App.cpp, CorralWindowCommands.cpp)
    Dlg_SelectVirtualFolder,     // "Select Folder for Virtual Corral"
    Err_InvalidFolderSelected,   // "Invalid folder selected."
    Err_NetworkPaths,            // "Network paths are not supported."
    Err_NetworkDrives,           // "Network drives are not supported."
    Err_FolderNotExist,          // "The specified folder does not exist."
    Err_PathNotFolder,           // "The specified path is not a folder."
    Err_NetworkPathsLocal,       // "... Please select a local folder." (path variant)
    Err_NetworkDrivesLocal,      // "... Please select a local folder." (drive variant)
    Title_InvalidFolder,         // "Invalid Folder"

    // Corral context menu (CorralWindowCommands.cpp, CorralWindowInput.cpp)
    Menu_AddTab,                 // "Add Tab"
    Menu_DetachTab,              // "Detach Tab"
    Menu_RenameTab,              // "Rename Tab"
    Menu_Appearance,             // "Appearance..."
    Menu_ChangeFolder,           // "Change Folder..."
    Menu_View,                   // "View"
    Menu_SmallIcons,             // "Small Icons"
    Menu_MediumIcons,            // "Medium Icons"
    Menu_LargeIcons,             // "Large Icons"
    Menu_Details,                // "Details"
    Menu_SortBy,                 // "Sort By"
    Menu_SortAscending,          // "Ascending"
    Menu_SortDescending,         // "Descending"
    Menu_CatchAll,               // "Catch-All (receives new files)"
    Menu_AddSpecialIcon,         // "Add Special Icon"
    Menu_ExcludeFromQuickHide,   // "Exclude from Quick-Hide"
    Menu_CloseTab,               // "Close Tab"
    Menu_DeleteCorral,           // "Delete Corral"
    Menu_RemoveFromCorral,       // "Remove from Corral"

    // Confirmations (CorralWindowCommands.cpp)
    Confirm_CloseTabBody,        // "Close this tab?"
    Confirm_CloseTabTitle,       // "Confirm Close"
    Confirm_DeleteCorralBody,    // "Delete this corral?"
    Confirm_DeleteCorralTitle,   // "Confirm Delete"

    // Rename dialog / file rename (CorralWindowDialogs.cpp)
    Dlg_RenameCorral,            // "Rename Corral"
    Err_RenameFailedBody,        // file rename failure text
    Err_RenameFailedTitle,       // "Rename Error"

    // Appearance dialog (CorralWindowDialogs.cpp)
    Title_Appearance,            // "Appearance: {0}"  ({0} = tab title)
    Grp_BackgroundColor,         // "Background Color"
    Btn_Change,                  // "Change..."
    Grp_Opacity,                 // "Opacity"
    Grp_Header,                  // "Header"
    Lbl_Height,                  // "Height"
    Lbl_Font,                    // "Font"
    Btn_Choose,                  // "Choose..."
    Lbl_Color,                   // "Color"
    Grp_Icons,                   // "Icons"
    Lbl_Opacity,                 // "Opacity"
    Lbl_Tint,                    // "Tint"
    Lbl_Background,              // "Background" (opacity row)
    Lbl_Border,                  // "Border" (opacity row)
    Lbl_HeaderLabel,             // "Header Label" (opacity row)
    Lbl_IconLabel,               // "Icon Label" (opacity row)
    Btn_Color,                   // "Color..."
    Grp_IconSpacing,             // "Icon Spacing"
    Lbl_Horiz,                   // "Horiz"
    Lbl_Vert,                    // "Vert"
    Chk_UseAsDefault,            // "Use as default for new corrals"
    Chk_ApplyToAll,              // "Apply changes to all corrals"
    Chk_CopyStyleToAll,          // "Copy full style to all corrals"

    // Details view column headers / sort menu (CorralWindowIcons.cpp, Commands)
    Col_Name,                    // "Name"
    Col_Type,                    // "Type"
    Col_Size,                    // "Size"
    Col_DateModified,            // "Date modified"

    // Corral header states (CorralWindowRender.cpp)
    Hdr_FolderUnavailable,       // "Folder unavailable\nRight-click to relink"

    // File size units (CorralWindowRender.cpp)
    Unit_Bytes,                  // " B"
    Unit_KB,                     // " KB"
    Unit_MB,                     // " MB"
    Unit_GB,                     // " GB"

    // Default names (stored in config as UTF-8; convert with WideToUtf8)
    Name_NewCorral,              // "New Corral"
    Name_NewTab,                 // "New Tab"
    Name_Desktop,                // "Desktop"

    // Registration tool (main.cpp)
    Reg_NoDesktopWindow,         // Explorer desktop window not found
    Reg_HookLoadFailed,          // "Failed to load DexCorralHook.dll."
    Reg_WakeProcMissing,         // "WakeHookProc not found in DexCorralHook.dll."
    Reg_HookLoadFailedHint,      // load failure + same-folder hint
    Reg_RegisterProcMissing,     // "DllRegisterServer not found in DexCorralHook.dll."
    Reg_RegisterSuccess,         // registered + restart Explorer steps
    Reg_RegisterFailed,          // "Failed to register shell extension..."
    Reg_UnregisterProcMissing,   // "DllUnregisterServer not found in DexCorralHook.dll."
    Reg_UnregisterSuccess,       // unregistered + restart Explorer
    Reg_UnregisterFailed,        // "Failed to unregister shell extension."
    Reg_Usage,                   // command-line usage text
    Reg_NeedsWin11,              // {0} = required build, {1} = this build

    _Count // sentinel — keep last
};

// Returns the current-language string for id, falling back to the compiled-in
// English table. Never returns null; the pointer is valid for process lifetime.
const wchar_t *Tr(Str id);

// Returns Tr(id) with every "{0}" token replaced by arg0. Literal replacement
// only: a translated string is data, so a stray %s or %n in one must never be
// handed to a printf-family function as a format specifier.
std::wstring TrFmt(Str id, const std::wstring &arg0);

// As above, replacing "{0}" with arg0 and "{1}" with arg1.
std::wstring TrFmt(Str id, const std::wstring &arg0, const std::wstring &arg1);

// Switches the active language. Supported codes: "en" (default), "de".
// Unknown/empty codes select English. All shipped languages are compiled in,
// so Tr() never depends on files on disk.
void SetLanguage(const std::wstring &langCode);

// Returns the language code chosen during installation, read from
// HKCU\Software\DexCorral\Language (written by the installer). Empty if unset.
// config.json's "Language" takes precedence over this when non-empty.
std::wstring GetInstallerLanguage();
