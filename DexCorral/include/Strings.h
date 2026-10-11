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
    Tray_Tooltip,                // "DexCorral - double-click here or on the desktop to hide everything"
    Menu_About,                  // "About"
    Menu_UserManual,             // "User Manual" (opens the manual in the browser)
    Menu_OpenConfigFolder,       // "Open Config Folder"
    Menu_CreateNewCorral,        // "Create New Corral"
    Menu_NewVirtualCorral,       // "New Virtual Corral"
    Menu_ShowDesktopIcons,       // "Show Desktop Icons"
    Menu_QuickHideEverything,    // "Quick-Hide Everything"
    Menu_QuickHideHotkey,        // "Quick-Hide Hotkey ({0})"  ({0} = combination)
    Menu_CheckUpdatesAuto,       // "Check for Updates Automatically"
    Menu_CheckUpdatesNow,        // "Check for Updates Now"
    Menu_Language,               // "Language" (submenu)

    // Standby: DexCorral deliberately not running (App.cpp)
    Tray_TooltipPaused,          // tray tooltip while in standby
    Menu_Resume,                 // "Resume DexCorral"
    Standby_CrashStatus,         // greyed status line in the tray menu
    SafeMode_Title,              // title of the standby dialog and of the reminder balloon
    SafeMode_Body,               // reminder balloon at later starts: still paused, how to resume
    Standby_CrashDialogBody,     // dialog at the start that enters standby: the full explanation
    Resume_FailedTitle,          // resume could not install the hook
    Resume_FailedBody,           // ... try signing out and in
    Menu_WhyPaused,              // "Why is DexCorral paused?" (shows the explanation again)
    Standby_ConflictStatus,      // greyed status line in the tray menu
    Conflict_StandbyBody,        // dialog: paused because of the conflict; {0} = product
    Conflict_StandbyBalloon,     // reminder balloon at later starts
    Conflict_LateBody,           // dialog: the conflicting program started mid-session; {0} = product

    // Unreadable config.json notice (App.cpp)
    Config_RestoredTitle,        // layout restored from config.bak
    Config_RestoredBody,         // where the unreadable file was kept
    Config_ResetTitle,           // no usable backup, fresh layout
    Config_ResetBody,            // where the unreadable file was kept

    // Quick-hide hotkey could not be registered (App.cpp)
    Hotkey_FailedTitle,          // "Hotkey not available"
    Hotkey_FailedBody,           // {0} = combination

    // About dialog (App.cpp)
    Title_About,                 // "About DexCorral"
    About_Body,                  // name, copyright and license in brief; {0} = version
    Btn_Website,                 // "Website"
    Btn_License,                 // "License"

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
    Menu_LockPosition,           // "Lock Position"
    Menu_ExpandOnHover,          // "Expand on Hover" (rolled-up corrals)
    Menu_CloseTab,               // "Close Tab"
    Menu_DeleteCorral,           // "Delete Corral"
    Menu_RemoveFromCorral,       // "Remove from Corral"
    Menu_MoveToTab,              // "Move to Tab" (submenu listing the corral's other tabs)

    // Confirmations (CorralWindowCommands.cpp)
    Confirm_CloseTabBody,        // {0} = tab title; icons return to the desktop
    Confirm_CloseVirtualTabBody, // {0} = tab title; the folder is not touched
    Confirm_CloseTabTitle,       // "Close Tab"
    Confirm_DeleteCorralBody,    // icons return to the desktop
    Confirm_DeleteVirtualCorralBody, // the folder is not touched
    Confirm_DeleteCorralTitle,   // "Delete Corral"

    // Rename dialog / file rename (CorralWindowDialogs.cpp)
    Dlg_RenameTab,               // "Rename Tab" (also shown right after creating a corral or tab)
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
    Chk_UseAsDefault,            // save this style as the default for new corrals
    Chk_ApplyToAll,              // transfer only the settings touched in this session
    Chk_CopyStyleToAll,          // overwrite every corral's style with this one
    Btn_ResetToDefaults,         // "Reset to Defaults"

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
    Reg_RegisterProcMissing,     // "DexCorralRegister not found in DexCorralHook.dll."
    Reg_RegisterSuccess,         // registered + restart Explorer steps
    Reg_RegisterFailed,          // "Failed to register shell extension..."
    Reg_UnregisterProcMissing,   // "DexCorralUnregister not found in DexCorralHook.dll."
    Reg_UnregisterSuccess,       // unregistered + restart Explorer
    Reg_UnregisterFailed,        // "Failed to unregister shell extension."
    Reg_Usage,                   // command-line usage text
    Reg_NeedsWin11,              // {0} = required build, {1} = this build
    Reg_NeedsElevation,          // --scope=machine without admin rights
    Conflict_Refuse,             // --register / --check-conflicts refusal; {0} = product, e.g. "Stardock Fences 6.5.4.1"

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

// Writes langCode to HKCU\Software\DexCorral\Language. The in-app switcher
// calls this so DexCorral.exe's own dialogs, which read only that key, follow
// the language picked in the app.
void SetInstallerLanguage(const std::wstring &langCode);
