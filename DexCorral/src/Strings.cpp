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
 * Strings.cpp - Compiled-in string tables + lookup
 *
 * One flat table per shipped language, indexed by (size_t)Str in enum order.
 * Nothing is ever loaded from disk. Locale files in a user-writable folder were
 * considered and rejected: they would put a parser on the startup path, fed by
 * an input any process running as the user can replace, before there is a UI to
 * report the failure in. Compiled in, no file can blank, truncate, or replace
 * the interface, and a missing row is a build error rather than a runtime one.
 * Adding a language means adding a table here plus a code in SetLanguage(),
 * then rebuilding.
 */

#include "Strings.h"
#include <Windows.h>

namespace
{

// Indexed by (size_t)Str — keep in the exact order of the enum.
const wchar_t *const kEnglish[] = {
    // Common
    /* App_Name */                 L"DexCorral",
    /* Title_Error */              L"Error",
    /* Btn_OK */                   L"OK",
    /* Btn_Cancel */               L"Cancel",

    // Tray icon / tray menu
    /* Tray_Tooltip */             L"DexCorral - double-click here or on the desktop to hide everything",
    /* Menu_About */               L"About",
    /* Menu_UserManual */          L"User Manual",
    /* Menu_OpenConfigFolder */    L"Open Config Folder",
    /* Menu_CreateNewCorral */     L"Create New Corral",
    /* Menu_NewVirtualCorral */    L"New Virtual Corral",
    /* Menu_ShowDesktopIcons */    L"Show Desktop Icons",
    /* Menu_QuickHideEverything */ L"Quick-Hide Everything",
    /* Menu_QuickHideHotkey */     L"Quick-Hide Hotkey ({0})",
    /* Menu_CheckUpdatesAuto */    L"Check for Updates Automatically",
    /* Menu_CheckUpdatesNow */     L"Check for Updates Now",
    /* Menu_Language */            L"Language",

    // Standby: DexCorral deliberately not running
    /* Tray_TooltipPaused */       L"DexCorral is paused - right-click for details",
    /* Menu_Resume */              L"Resume DexCorral",
    /* Standby_CrashStatus */      L"Paused: Explorer crashed twice in a row",
    /* SafeMode_Title */           L"DexCorral is paused",
    /* SafeMode_Body */            L"DexCorral is still paused after Explorer crashed twice in a row. Your icons "
                                   L"are on the desktop and your Corrals are saved. To try again, right-click "
                                   L"this icon and choose Resume DexCorral.",
    /* Standby_CrashDialogBody */  L"Windows Explorer crashed twice within two minutes while DexCorral was starting, "
                                   L"so DexCorral has paused itself instead of trying again.\n\n"
                                   L"Nothing is lost. All your icons are back on the desktop, and your Corrals "
                                   L"and their settings are saved.\n\n"
                                   L"DexCorral stays paused, also after a restart, until you right-click its "
                                   L"tray icon and choose \"Resume DexCorral\".\n\n"
                                   L"If it happens again, please report it. \"Open Config Folder\" in the same "
                                   L"menu shows guard.log and config.json, which are what a report needs.",
    /* Resume_FailedTitle */       L"DexCorral could not resume",
    /* Resume_FailedBody */        L"The desktop was not ready. Sign out and back in, and DexCorral will start "
                                   L"normally.",

    // Unreadable config.json notice
    /* Config_RestoredTitle */     L"DexCorral restored your layout from a backup",
    /* Config_RestoredBody */      L"config.json could not be read, so the last working backup was loaded. "
                                   L"The unreadable file was kept as config.corrupt.json in %APPDATA%\\DexCorral.",
    /* Config_ResetTitle */        L"DexCorral could not read its configuration",
    /* Config_ResetBody */         L"config.json could not be read and no backup was usable, so DexCorral started "
                                   L"with a fresh layout. The unreadable file was kept as config.corrupt.json in "
                                   L"%APPDATA%\\DexCorral.",

    // Quick-hide hotkey could not be registered
    /* Hotkey_FailedTitle */       L"Hotkey not available",
    /* Hotkey_FailedBody */        L"{0} is not a usable combination, or Windows or another program already uses it. "
                                   L"Set a different one as QuickHideHotkey in config.json.",

    // About dialog
    /* Title_About */              L"About DexCorral",
    /* About_Body */               L"DexCorral {0}\n"
                                   L"A free and open source desktop icon organizer for Windows.\n\n"
                                   L"Copyright (C) 2026 Gunter Heiss\n\n"
                                   L"Free software under the GNU General Public License, version 3 or later. "
                                   L"It comes with no warranty.",
    /* Btn_Website */              L"Website",
    /* Btn_License */              L"License",

    // Update check balloons
    /* Update_AvailableTitle */    L"DexCorral update available",
    /* Update_AvailableBody */     L"Version {0} is available. Click here to open the download page.",
    /* Update_UpToDateTitle */     L"DexCorral is up to date",
    /* Update_UpToDateBody */      L"You are running the latest version ({0}).",
    /* Update_FailedTitle */       L"Update check failed",
    /* Update_FailedBody */        L"Could not reach the update server. Please try again later.",

    // Virtual corral folder selection
    /* Dlg_SelectVirtualFolder */  L"Select Folder for Virtual Corral",
    /* Err_InvalidFolderSelected */L"Invalid folder selected.",
    /* Err_NetworkPaths */         L"Network paths are not supported.",
    /* Err_NetworkDrives */        L"Network drives are not supported.",
    /* Err_FolderNotExist */       L"The specified folder does not exist.",
    /* Err_PathNotFolder */        L"The specified path is not a folder.",
    /* Err_NetworkPathsLocal */    L"Network paths are not supported. Please select a local folder.",
    /* Err_NetworkDrivesLocal */   L"Network drives are not supported. Please select a local folder.",
    /* Title_InvalidFolder */      L"Invalid Folder",

    // Corral context menu
    /* Menu_AddTab */              L"Add Tab",
    /* Menu_DetachTab */           L"Detach Tab",
    /* Menu_RenameTab */           L"Rename Tab",
    /* Menu_Appearance */          L"Appearance...",
    /* Menu_ChangeFolder */        L"Change Folder...",
    /* Menu_View */                L"View",
    /* Menu_SmallIcons */          L"Small Icons",
    /* Menu_MediumIcons */         L"Medium Icons",
    /* Menu_LargeIcons */          L"Large Icons",
    /* Menu_Details */             L"Details",
    /* Menu_SortBy */              L"Sort By",
    /* Menu_SortAscending */       L"Ascending",
    /* Menu_SortDescending */      L"Descending",
    /* Menu_CatchAll */            L"Catch-All (receives new files)",
    /* Menu_AddSpecialIcon */      L"Add Special Icon",
    /* Menu_ExcludeFromQuickHide */L"Exclude from Quick-Hide",
    /* Menu_LockPosition */        L"Lock Position",
    /* Menu_ExpandOnHover */       L"Expand on Hover (when rolled up)",
    /* Menu_CloseTab */            L"Close Tab",
    /* Menu_DeleteCorral */        L"Delete Corral",
    /* Menu_RemoveFromCorral */    L"Remove from Corral",
    /* Menu_MoveToTab */           L"Move to Tab",

    // Confirmations
    /* Confirm_CloseTabBody */     L"Close the tab \"{0}\"?\n\nIts icons go back to the desktop. No files are deleted.",
    /* Confirm_CloseVirtualTabBody */ L"Close the tab \"{0}\"?\n\nThe folder it shows and the files in it are not touched.",
    /* Confirm_CloseTabTitle */    L"Close Tab",
    /* Confirm_DeleteCorralBody */ L"Delete this corral?\n\nIts icons go back to the desktop. No files are deleted.",
    /* Confirm_DeleteVirtualCorralBody */ L"Delete this corral?\n\nThe folder it shows and the files in it are not touched.",
    /* Confirm_DeleteCorralTitle */L"Delete Corral",

    // Rename dialog / file rename
    /* Dlg_RenameTab */            L"Rename Tab",
    /* Err_RenameFailedBody */     L"Failed to rename file. The file may be in use or you may not have permission.",
    /* Err_RenameFailedTitle */    L"Rename Error",

    // Appearance dialog
    /* Title_Appearance */         L"Appearance: {0}",
    /* Grp_BackgroundColor */      L"Background Color",
    /* Btn_Change */               L"Change...",
    /* Grp_Opacity */              L"Opacity",
    /* Grp_Header */               L"Header",
    /* Lbl_Height */               L"Height",
    /* Lbl_Font */                 L"Font",
    /* Btn_Choose */               L"Choose...",
    /* Lbl_Color */                L"Color",
    /* Grp_Icons */                L"Icons",
    /* Lbl_Opacity */              L"Opacity",
    /* Lbl_Tint */                 L"Tint",
    /* Lbl_Background */           L"Background",
    /* Lbl_Border */               L"Border",
    /* Lbl_HeaderLabel */          L"Header Label",
    /* Lbl_IconLabel */            L"Icon Label",
    /* Btn_Color */                L"Color...",
    /* Grp_IconSpacing */          L"Icon Spacing",
    /* Lbl_Horiz */                L"Horiz",
    /* Lbl_Vert */                 L"Vert",
    /* Chk_UseAsDefault */         L"Save this style as the default for new corrals",
    /* Chk_ApplyToAll */           L"Apply only what I changed to all corrals",
    /* Chk_CopyStyleToAll */       L"Make all corrals look like this one",
    /* Btn_ResetToDefaults */      L"Reset to Defaults",

    // Details view column headers / sort menu
    /* Col_Name */                 L"Name",
    /* Col_Type */                 L"Type",
    /* Col_Size */                 L"Size",
    /* Col_DateModified */         L"Date modified",

    // Corral header states
    /* Hdr_FolderUnavailable */    L"Folder unavailable\nRight-click to relink",

    // File size units
    /* Unit_Bytes */               L" B",
    /* Unit_KB */                  L" KB",
    /* Unit_MB */                  L" MB",
    /* Unit_GB */                  L" GB",

    // Default names
    /* Name_NewCorral */           L"New Corral",
    /* Name_NewTab */              L"New Tab",
    /* Name_Desktop */             L"Desktop",

    // Registration tool
    /* Reg_NoDesktopWindow */      L"Could not find Explorer's desktop window.\nMake sure Explorer is running.",
    /* Reg_HookLoadFailed */       L"Failed to load DexCorralHook.dll.",
    /* Reg_WakeProcMissing */      L"WakeHookProc not found in DexCorralHook.dll.",
    /* Reg_HookLoadFailedHint */   L"Failed to load DexCorralHook.dll.\nMake sure it's in the same folder as this EXE.",
    /* Reg_RegisterProcMissing */  L"DexCorralRegister not found in DexCorralHook.dll.",
    /* Reg_RegisterSuccess */      L"DexCorral shell extension registered successfully.\n\n"
                                   L"Restart Explorer for changes to take effect:\n"
                                   L"  1. Open Task Manager\n"
                                   L"  2. Find 'Windows Explorer'\n"
                                   L"  3. Right-click > Restart",
    /* Reg_RegisterFailed */       L"Failed to register shell extension.\n\nTry running as Administrator.",
    /* Reg_UnregisterProcMissing */L"DexCorralUnregister not found in DexCorralHook.dll.",
    /* Reg_UnregisterSuccess */    L"DexCorral shell extension unregistered successfully.\n\n"
                                   L"Restart Explorer for changes to take effect.",
    /* Reg_UnregisterFailed */     L"Failed to unregister shell extension.",
    /* Reg_Usage */                L"DexCorral - Desktop Icon Organizer\n\n"
                                   L"Usage:\n"
                                   L"  DexCorral.exe --register     Register shell extension\n"
                                   L"  DexCorral.exe --unregister   Unregister shell extension\n"
                                   L"  DexCorral.exe --startup      Inject into Explorer and start (used by Run key)\n"
                                   L"  DexCorral.exe --silent       Suppress message dialogs\n"
                                   L"  DexCorral.exe --reset-guard  End a crash-guard pause (see the manual)\n"
                                   L"  DexCorral.exe --check-conflicts  Exit code 3 if incompatible software is installed\n"
                                   L"  DexCorral.exe --force        Register on an unsupported Windows version\n\n"
                                   L"Scope (applies to --register and --unregister):\n"
                                   L"  --scope=user      This user only, HKCU, no admin rights needed\n"
                                   L"  --scope=machine   All users, HKLM, must run as Administrator\n"
                                   L"  --cleanup-legacy  Remove registry keys from installs before 1.0.28\n\n"
                                   L"Without --scope the scope is inferred: under Program Files means machine.\n"
                                   L"The two scopes share one CLSID and must not both be installed.\n\n"
                                   L"DexCorral requires Windows 11; Windows 10 is unsupported and untested.\n"
                                   L"After registration, restart Explorer or use --startup to activate.",
    /* Reg_NeedsWin11 */           L"DexCorral requires Windows 11 (build {0} or newer).\n"
                                   L"This system reports build {1}.\n\n"
                                   L"Windows 10 is end of life and DexCorral is neither tested nor "
                                   L"supported on it.\n\n"
                                   L"To register anyway, at your own risk:\n"
                                   L"  DexCorral.exe --register --force\n\n"
                                   L"Please do not file bug reports from unsupported Windows versions.",
    /* Reg_NeedsElevation */       L"A machine-wide (all users) registration writes to HKEY_LOCAL_MACHINE "
                                   L"and needs Administrator rights.\n\n"
                                   L"Run this command from an elevated prompt, or install DexCorral for "
                                   L"your account only:\n"
                                   L"  DexCorral.exe --register --scope=user",
    /* Conflict_Refuse */          L"DexCorral cannot be set up while {0} is installed on this PC.\n\n"
                                   L"Both programs take over the desktop icon view. With Fences running, the "
                                   L"icons DexCorral hides become visible again and are drawn twice.\n\n"
                                   L"Which of the two you use is your choice: uninstall Stardock Fences and run "
                                   L"this again, or keep Fences and leave DexCorral out.\n\n"
                                   L"Stardock and Fences are trademarks of Stardock. DexCorral is not affiliated "
                                   L"with Stardock.",
};

static_assert(sizeof(kEnglish) / sizeof(kEnglish[0]) == (size_t)Str::_Count,
              "kEnglish must have exactly one entry per Str enum value, in enum order");

// German translation — same order as kEnglish. This file must be compiled as
// UTF-8 (/utf-8, set in CMakeLists.txt) for the umlauts to survive.
const wchar_t *const kGerman[] = {
    // Common
    /* App_Name */                 L"DexCorral",
    /* Title_Error */              L"Fehler",
    /* Btn_OK */                   L"OK",
    /* Btn_Cancel */               L"Abbrechen",

    // Tray icon / tray menu
    /* Tray_Tooltip */             L"DexCorral - Doppelklick hier oder auf den Desktop blendet alles aus",
    /* Menu_About */               L"Info",
    /* Menu_UserManual */          L"Benutzerhandbuch",
    /* Menu_OpenConfigFolder */    L"Konfigurationsordner öffnen",
    /* Menu_CreateNewCorral */     L"Neues Corral erstellen",
    /* Menu_NewVirtualCorral */    L"Neues virtuelles Corral",
    /* Menu_ShowDesktopIcons */    L"Desktopsymbole anzeigen",
    /* Menu_QuickHideEverything */ L"Alles schnell ausblenden",
    /* Menu_QuickHideHotkey */     L"Tastenkürzel für Schnell-Ausblenden ({0})",
    /* Menu_CheckUpdatesAuto */    L"Automatisch nach Updates suchen",
    /* Menu_CheckUpdatesNow */     L"Jetzt nach Updates suchen",
    /* Menu_Language */            L"Sprache",

    // Standby: DexCorral deliberately not running
    /* Tray_TooltipPaused */       L"DexCorral ist pausiert - Rechtsklick für Details",
    /* Menu_Resume */              L"DexCorral fortsetzen",
    /* Standby_CrashStatus */      L"Pausiert: Der Explorer ist zweimal abgestürzt",
    /* SafeMode_Title */           L"DexCorral ist pausiert",
    /* SafeMode_Body */            L"DexCorral ist weiterhin pausiert, nachdem der Explorer zweimal hintereinander "
                                   L"abgestürzt ist. Die Symbole liegen auf dem Desktop, die Corrals sind "
                                   L"gespeichert. Zum erneuten Versuch: Rechtsklick auf dieses Symbol, dann "
                                   L"DexCorral fortsetzen.",
    /* Standby_CrashDialogBody */  L"Der Windows-Explorer ist beim Start von DexCorral zweimal innerhalb von zwei "
                                   L"Minuten abgestürzt. DexCorral hat sich deshalb pausiert, statt es erneut zu "
                                   L"versuchen.\n\n"
                                   L"Es geht nichts verloren. Alle Symbole liegen wieder auf dem Desktop, die "
                                   L"Corrals und ihre Einstellungen sind gespeichert.\n\n"
                                   L"DexCorral bleibt pausiert, auch nach einem Neustart, bis Sie mit der rechten "
                                   L"Maustaste auf das Symbol im Infobereich klicken und \"DexCorral fortsetzen\" "
                                   L"wählen.\n\n"
                                   L"Sollte es erneut passieren, melden Sie es bitte. \"Konfigurationsordner "
                                   L"öffnen\" im selben Menü zeigt guard.log und config.json, die für eine Meldung "
                                   L"gebraucht werden.",
    /* Resume_FailedTitle */       L"DexCorral konnte nicht fortgesetzt werden",
    /* Resume_FailedBody */        L"Der Desktop war nicht bereit. Nach dem Ab- und Anmelden startet DexCorral "
                                   L"normal.",

    // Unreadable config.json notice
    /* Config_RestoredTitle */     L"DexCorral hat das Layout aus einer Sicherung geladen",
    /* Config_RestoredBody */      L"config.json konnte nicht gelesen werden, daher wurde die letzte funktionierende "
                                   L"Sicherung geladen. Die unlesbare Datei liegt als config.corrupt.json in "
                                   L"%APPDATA%\\DexCorral.",
    /* Config_ResetTitle */        L"DexCorral konnte seine Konfiguration nicht lesen",
    /* Config_ResetBody */         L"config.json konnte nicht gelesen werden und es gab keine brauchbare Sicherung, "
                                   L"daher startet DexCorral mit einem neuen Layout. Die unlesbare Datei liegt als "
                                   L"config.corrupt.json in %APPDATA%\\DexCorral.",

    // Quick-hide hotkey could not be registered
    /* Hotkey_FailedTitle */       L"Tastenkürzel nicht verfügbar",
    /* Hotkey_FailedBody */        L"{0} ist keine gültige Kombination oder wird bereits von Windows oder einem "
                                   L"anderen Programm verwendet. Eine andere lässt sich als QuickHideHotkey in "
                                   L"config.json festlegen.",

    // About dialog
    /* Title_About */              L"Über DexCorral",
    /* About_Body */               L"DexCorral {0}\n"
                                   L"Ein freier und quelloffener Desktop-Symbol-Organizer für Windows.\n\n"
                                   L"Copyright (C) 2026 Gunter Heiss\n\n"
                                   L"Freie Software unter der GNU General Public License, Version 3 oder später. "
                                   L"Es besteht keinerlei Garantie.",
    /* Btn_Website */              L"Website",
    /* Btn_License */              L"Lizenz",

    // Update check balloons
    /* Update_AvailableTitle */    L"DexCorral-Update verfügbar",
    /* Update_AvailableBody */     L"Version {0} ist verfügbar. Klicken Sie hier, um die Downloadseite zu öffnen.",
    /* Update_UpToDateTitle */     L"DexCorral ist auf dem neuesten Stand",
    /* Update_UpToDateBody */      L"Sie verwenden die neueste Version ({0}).",
    /* Update_FailedTitle */       L"Updateprüfung fehlgeschlagen",
    /* Update_FailedBody */        L"Der Updateserver konnte nicht erreicht werden. Bitte versuchen Sie es später erneut.",

    // Virtual corral folder selection
    /* Dlg_SelectVirtualFolder */  L"Ordner für virtuelles Corral auswählen",
    /* Err_InvalidFolderSelected */L"Ungültiger Ordner ausgewählt.",
    /* Err_NetworkPaths */         L"Netzwerkpfade werden nicht unterstützt.",
    /* Err_NetworkDrives */        L"Netzlaufwerke werden nicht unterstützt.",
    /* Err_FolderNotExist */       L"Der angegebene Ordner existiert nicht.",
    /* Err_PathNotFolder */        L"Der angegebene Pfad ist kein Ordner.",
    /* Err_NetworkPathsLocal */    L"Netzwerkpfade werden nicht unterstützt. Bitte wählen Sie einen lokalen Ordner.",
    /* Err_NetworkDrivesLocal */   L"Netzlaufwerke werden nicht unterstützt. Bitte wählen Sie einen lokalen Ordner.",
    /* Title_InvalidFolder */      L"Ungültiger Ordner",

    // Corral context menu
    /* Menu_AddTab */              L"Tab hinzufügen",
    /* Menu_DetachTab */           L"Tab abtrennen",
    /* Menu_RenameTab */           L"Tab umbenennen",
    /* Menu_Appearance */          L"Darstellung...",
    /* Menu_ChangeFolder */        L"Ordner ändern...",
    /* Menu_View */                L"Ansicht",
    /* Menu_SmallIcons */          L"Kleine Symbole",
    /* Menu_MediumIcons */         L"Mittelgroße Symbole",
    /* Menu_LargeIcons */          L"Große Symbole",
    /* Menu_Details */             L"Details",
    /* Menu_SortBy */              L"Sortieren nach",
    /* Menu_SortAscending */       L"Aufsteigend",
    /* Menu_SortDescending */      L"Absteigend",
    /* Menu_CatchAll */            L"Auffang-Corral (empfängt neue Dateien)",
    /* Menu_AddSpecialIcon */      L"Spezialsymbol hinzufügen",
    /* Menu_ExcludeFromQuickHide */L"Vom Schnell-Ausblenden ausnehmen",
    /* Menu_LockPosition */        L"Position sperren",
    /* Menu_ExpandOnHover */       L"Beim Überfahren aufklappen (wenn eingerollt)",
    /* Menu_CloseTab */            L"Tab schließen",
    /* Menu_DeleteCorral */        L"Corral löschen",
    /* Menu_RemoveFromCorral */    L"Aus dem Corral entfernen",
    /* Menu_MoveToTab */           L"In Tab verschieben",

    // Confirmations
    /* Confirm_CloseTabBody */     L"Den Tab „{0}“ schließen?\n\nSeine Symbole kehren auf den Desktop zurück. Es werden keine Dateien gelöscht.",
    /* Confirm_CloseVirtualTabBody */ L"Den Tab „{0}“ schließen?\n\nDer angezeigte Ordner und seine Dateien bleiben unverändert.",
    /* Confirm_CloseTabTitle */    L"Tab schließen",
    /* Confirm_DeleteCorralBody */ L"Dieses Corral löschen?\n\nSeine Symbole kehren auf den Desktop zurück. Es werden keine Dateien gelöscht.",
    /* Confirm_DeleteVirtualCorralBody */ L"Dieses Corral löschen?\n\nDer angezeigte Ordner und seine Dateien bleiben unverändert.",
    /* Confirm_DeleteCorralTitle */L"Corral löschen",

    // Rename dialog / file rename
    /* Dlg_RenameTab */            L"Tab umbenennen",
    /* Err_RenameFailedBody */     L"Die Datei konnte nicht umbenannt werden. Sie wird möglicherweise verwendet, oder Ihnen fehlt die Berechtigung.",
    /* Err_RenameFailedTitle */    L"Fehler beim Umbenennen",

    // Appearance dialog
    /* Title_Appearance */         L"Darstellung: {0}",
    /* Grp_BackgroundColor */      L"Hintergrundfarbe",
    /* Btn_Change */               L"Ändern...",
    /* Grp_Opacity */              L"Deckkraft",
    /* Grp_Header */               L"Kopfzeile",
    /* Lbl_Height */               L"Höhe",
    /* Lbl_Font */                 L"Schrift",
    /* Btn_Choose */               L"Wählen...",
    /* Lbl_Color */                L"Farbe",
    /* Grp_Icons */                L"Symbole",
    /* Lbl_Opacity */              L"Deckkraft",
    /* Lbl_Tint */                 L"Tönung",
    /* Lbl_Background */           L"Hintergrund",
    /* Lbl_Border */               L"Rahmen",
    /* Lbl_HeaderLabel */          L"Kopfzeilentext",
    /* Lbl_IconLabel */            L"Symboltext",
    /* Btn_Color */                L"Farbe...",
    /* Grp_IconSpacing */          L"Symbolabstand",
    /* Lbl_Horiz */                L"Horiz.",
    /* Lbl_Vert */                 L"Vert.",
    /* Chk_UseAsDefault */         L"Diesen Stil als Standard für neue Corrals speichern",
    /* Chk_ApplyToAll */           L"Nur hier Geändertes auf alle Corrals anwenden",
    /* Chk_CopyStyleToAll */       L"Alle Corrals wie dieses aussehen lassen",
    /* Btn_ResetToDefaults */      L"Auf Standard zurücksetzen",

    // Details view column headers / sort menu
    /* Col_Name */                 L"Name",
    /* Col_Type */                 L"Typ",
    /* Col_Size */                 L"Größe",
    /* Col_DateModified */         L"Änderungsdatum",

    // Corral header states
    /* Hdr_FolderUnavailable */    L"Ordner nicht verfügbar\nRechtsklick zum Neuverknüpfen",

    // File size units
    /* Unit_Bytes */               L" B",
    /* Unit_KB */                  L" KB",
    /* Unit_MB */                  L" MB",
    /* Unit_GB */                  L" GB",

    // Default names
    /* Name_NewCorral */           L"Neues Corral",
    /* Name_NewTab */              L"Neuer Tab",
    /* Name_Desktop */             L"Desktop",

    // Registration tool
    /* Reg_NoDesktopWindow */      L"Das Desktopfenster des Explorers wurde nicht gefunden.\nStellen Sie sicher, dass der Explorer läuft.",
    /* Reg_HookLoadFailed */       L"DexCorralHook.dll konnte nicht geladen werden.",
    /* Reg_WakeProcMissing */      L"WakeHookProc wurde in DexCorralHook.dll nicht gefunden.",
    /* Reg_HookLoadFailedHint */   L"DexCorralHook.dll konnte nicht geladen werden.\nStellen Sie sicher, dass sie im selben Ordner wie diese EXE liegt.",
    /* Reg_RegisterProcMissing */  L"DexCorralRegister wurde in DexCorralHook.dll nicht gefunden.",
    /* Reg_RegisterSuccess */      L"Die DexCorral-Shell-Erweiterung wurde erfolgreich registriert.\n\n"
                                   L"Starten Sie den Explorer neu, damit die Änderungen wirksam werden:\n"
                                   L"  1. Task-Manager öffnen\n"
                                   L"  2. 'Windows-Explorer' suchen\n"
                                   L"  3. Rechtsklick > Neu starten",
    /* Reg_RegisterFailed */       L"Die Shell-Erweiterung konnte nicht registriert werden.\n\nVersuchen Sie es als Administrator.",
    /* Reg_UnregisterProcMissing */L"DexCorralUnregister wurde in DexCorralHook.dll nicht gefunden.",
    /* Reg_UnregisterSuccess */    L"Die DexCorral-Shell-Erweiterung wurde erfolgreich deregistriert.\n\n"
                                   L"Starten Sie den Explorer neu, damit die Änderungen wirksam werden.",
    /* Reg_UnregisterFailed */     L"Die Shell-Erweiterung konnte nicht deregistriert werden.",
    /* Reg_Usage */                L"DexCorral - Desktop-Symbol-Organizer\n\n"
                                   L"Verwendung:\n"
                                   L"  DexCorral.exe --register     Shell-Erweiterung registrieren\n"
                                   L"  DexCorral.exe --unregister   Shell-Erweiterung deregistrieren\n"
                                   L"  DexCorral.exe --startup      In den Explorer injizieren und starten (vom Run-Schlüssel verwendet)\n"
                                   L"  DexCorral.exe --silent       Meldungsdialoge unterdrücken\n"
                                   L"  DexCorral.exe --reset-guard  Eine Absturzschutz-Pause beenden (siehe Handbuch)\n"
                                   L"  DexCorral.exe --check-conflicts  Exitcode 3, wenn unverträgliche Software installiert ist\n"
                                   L"  DexCorral.exe --force        Auf einer nicht unterstützten Windows-Version registrieren\n\n"
                                   L"Gültigkeitsbereich (für --register und --unregister):\n"
                                   L"  --scope=user      Nur dieser Benutzer, HKCU, keine Administratorrechte nötig\n"
                                   L"  --scope=machine   Alle Benutzer, HKLM, muss als Administrator laufen\n"
                                   L"  --cleanup-legacy  Registry-Schlüssel von Installationen vor 1.0.28 entfernen\n\n"
                                   L"Ohne --scope wird der Bereich abgeleitet: unter Programme bedeutet machine.\n"
                                   L"Beide Bereiche teilen sich eine CLSID und dürfen nicht gleichzeitig installiert sein.\n\n"
                                   L"DexCorral benötigt Windows 11; Windows 10 wird nicht unterstützt und ist ungetestet.\n"
                                   L"Nach der Registrierung den Explorer neu starten oder --startup verwenden.",
    /* Reg_NeedsWin11 */           L"DexCorral benötigt Windows 11 (Build {0} oder neuer).\n"
                                   L"Dieses System meldet Build {1}.\n\n"
                                   L"Windows 10 hat das Supportende erreicht; DexCorral ist darauf weder "
                                   L"getestet noch unterstützt.\n\n"
                                   L"Wenn du trotzdem registrieren möchtest, auf eigene Gefahr:\n"
                                   L"  DexCorral.exe --register --force\n\n"
                                   L"Bitte melde keine Fehler von nicht unterstützten Windows-Versionen.",
    /* Reg_NeedsElevation */       L"Eine systemweite Registrierung (alle Benutzer) schreibt nach "
                                   L"HKEY_LOCAL_MACHINE und benötigt Administratorrechte.\n\n"
                                   L"Führe den Befehl in einer Eingabeaufforderung als Administrator aus, "
                                   L"oder installiere DexCorral nur für dein Konto:\n"
                                   L"  DexCorral.exe --register --scope=user",
    /* Conflict_Refuse */          L"DexCorral kann nicht eingerichtet werden, solange {0} auf diesem PC "
                                   L"installiert ist.\n\n"
                                   L"Beide Programme übernehmen die Symbolansicht des Desktops. Läuft Fences, "
                                   L"werden die von DexCorral ausgeblendeten Symbole wieder sichtbar und doppelt "
                                   L"gezeichnet.\n\n"
                                   L"Welches der beiden du nutzt, entscheidest du: Stardock Fences deinstallieren "
                                   L"und dies erneut ausführen, oder Fences behalten und auf DexCorral verzichten.\n\n"
                                   L"Stardock und Fences sind Marken von Stardock. DexCorral steht in keiner "
                                   L"Verbindung zu Stardock.",
};

static_assert(sizeof(kGerman) / sizeof(kGerman[0]) == (size_t)Str::_Count,
              "kGerman must have exactly one entry per Str enum value, in enum order");

// Active table — swapped by SetLanguage(). English until told otherwise.
const wchar_t *const *g_active = kEnglish;

} // namespace

const wchar_t *Tr(Str id)
{
    size_t i = (size_t)id;
    if (i >= (size_t)Str::_Count)
        return L"";
    return g_active[i];
}

void SetLanguage(const std::wstring &langCode)
{
    if (langCode == L"de")
        g_active = kGerman;
    else
        g_active = kEnglish;
}

std::wstring GetInstallerLanguage()
{
    // HKCU first, then HKLM. A machine-wide install writes the language once to
    // HKLM for every user; a per-user install and the in-app language setting
    // both write HKCU, which is why that one wins.
    const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (HKEY root : roots)
    {
        wchar_t buf[16] = {};
        DWORD size = sizeof(buf);
        if (RegGetValueW(root, L"Software\\DexCorral", L"Language",
                         RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
            return buf;
    }
    return L"";
}

void SetInstallerLanguage(const std::wstring &langCode)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\DexCorral", 0, nullptr,
                        0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return; // best effort: the app itself uses config.json, not this key
    RegSetValueExW(key, L"Language", 0, REG_SZ, (const BYTE *)langCode.c_str(),
                   (DWORD)((langCode.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

std::wstring TrFmt(Str id, const std::wstring &arg0)
{
    std::wstring result = Tr(id);
    const std::wstring token = L"{0}";
    size_t pos = 0;
    while ((pos = result.find(token, pos)) != std::wstring::npos)
    {
        result.replace(pos, token.size(), arg0);
        pos += arg0.size();
    }
    return result;
}

std::wstring TrFmt(Str id, const std::wstring &arg0, const std::wstring &arg1)
{
    // Single left-to-right pass so a token appearing inside an argument is
    // never re-expanded by a later replacement.
    const std::wstring src = Tr(id);
    std::wstring result;
    result.reserve(src.size() + arg0.size() + arg1.size());
    for (size_t i = 0; i < src.size();)
    {
        if (src.compare(i, 3, L"{0}") == 0)      { result += arg0; i += 3; }
        else if (src.compare(i, 3, L"{1}") == 0) { result += arg1; i += 3; }
        else                                     { result += src[i]; i++; }
    }
    return result;
}
