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
    /* Tray_Tooltip */             L"DexCorral - Double-click desktop to hide icons",
    /* Menu_About */               L"About",
    /* Menu_CreateNewCorral */     L"Create New Corral",
    /* Menu_NewVirtualCorral */    L"New Virtual Corral",
    /* Menu_ShowDesktopIcons */    L"Show Desktop Icons",
    /* Menu_QuickHideEverything */ L"Quick-Hide Everything",
    /* Menu_CheckUpdatesAuto */    L"Check for Updates Automatically",
    /* Menu_CheckUpdatesNow */     L"Check for Updates Now",

    // Safe-mode notice
    /* SafeMode_Title */           L"DexCorral started in safe mode",
    /* SafeMode_Body */            L"Explorer restarted repeatedly while the desktop hook was active, so the "
                                   L"hook is disabled for this session. Desktop icons stay visible. The hook "
                                   L"will be re-enabled on the next start.",

    // About dialog
    /* Title_About */              L"About DexCorral",
    /* About_Body */               L"DexCorral - a free and open source Windows desktop icon organizer\n\n"
                                   L"Version: {0}\n\n"
                                   L"Copyright (C) 2026 Gunter Heiss\n\n"
                                   L"This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.\n\n"
                                   L"This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.\n\n"
                                   L"You should have received a copy of the GNU General Public License along with this program.  If not, see https://www.gnu.org/licenses/\n\n"
                                   L"Website: https://dexcorral.com\n"
                                   L"GitHub: https://github.com/guHe330/DexCorral",

    // Shortcut-arrow toggle
    /* Arrow_HideConfirm */        L"This will hide shortcut arrows on desktop icons.\n\nExplorer will restart to apply the change. Continue?",
    /* Arrow_RestoreConfirm */     L"This will restore shortcut arrows on desktop icons.\n\nExplorer will restart to apply the change. Continue?",
    /* Arrow_ChangeFailed */       L"Failed to change shortcut arrow setting.\n\nThis may require administrator privileges.",

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
    /* Menu_CloseTab */            L"Close Tab",
    /* Menu_DeleteCorral */        L"Delete Corral",
    /* Menu_RemoveFromCorral */    L"Remove from Corral",

    // Confirmations
    /* Confirm_CloseTabBody */     L"Close this tab?",
    /* Confirm_CloseTabTitle */    L"Confirm Close",
    /* Confirm_DeleteCorralBody */ L"Delete this corral?",
    /* Confirm_DeleteCorralTitle */L"Confirm Delete",

    // Rename dialog / file rename
    /* Dlg_RenameCorral */         L"Rename Corral",
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
    /* Chk_UseAsDefault */         L"Use as default for new corrals",
    /* Chk_ApplyToAll */           L"Apply changes to all corrals",
    /* Chk_CopyStyleToAll */       L"Copy full style to all corrals",

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
    /* Reg_RegisterProcMissing */  L"DllRegisterServer not found in DexCorralHook.dll.",
    /* Reg_RegisterSuccess */      L"DexCorral shell extension registered successfully.\n\n"
                                   L"Restart Explorer for changes to take effect:\n"
                                   L"  1. Open Task Manager\n"
                                   L"  2. Find 'Windows Explorer'\n"
                                   L"  3. Right-click > Restart",
    /* Reg_RegisterFailed */       L"Failed to register shell extension.\n\nTry running as Administrator.",
    /* Reg_UnregisterProcMissing */L"DllUnregisterServer not found in DexCorralHook.dll.",
    /* Reg_UnregisterSuccess */    L"DexCorral shell extension unregistered successfully.\n\n"
                                   L"Restart Explorer for changes to take effect.",
    /* Reg_UnregisterFailed */     L"Failed to unregister shell extension.",
    /* Reg_Usage */                L"DexCorral - Desktop Icon Organizer\n\n"
                                   L"Usage:\n"
                                   L"  DexCorral.exe --register     Register shell extension\n"
                                   L"  DexCorral.exe --unregister   Unregister shell extension\n"
                                   L"  DexCorral.exe --startup      Inject into Explorer and start (used by Run key)\n"
                                   L"  DexCorral.exe --silent       Suppress message dialogs\n"
                                   L"  DexCorral.exe --force        Register on an unsupported Windows version\n\n"
                                   L"DexCorral requires Windows 11; Windows 10 is unsupported and untested.\n"
                                   L"After registration, restart Explorer or use --startup to activate.",
    /* Reg_NeedsWin11 */           L"DexCorral requires Windows 11 (build {0} or newer).\n"
                                   L"This system reports build {1}.\n\n"
                                   L"Windows 10 is end of life and DexCorral is neither tested nor "
                                   L"supported on it.\n\n"
                                   L"To register anyway, at your own risk:\n"
                                   L"  DexCorral.exe --register --force\n\n"
                                   L"Please do not file bug reports from unsupported Windows versions.",
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
    /* Tray_Tooltip */             L"DexCorral - Doppelklick auf den Desktop blendet Symbole aus",
    /* Menu_About */               L"Info",
    /* Menu_CreateNewCorral */     L"Neues Corral erstellen",
    /* Menu_NewVirtualCorral */    L"Neues virtuelles Corral",
    /* Menu_ShowDesktopIcons */    L"Desktopsymbole anzeigen",
    /* Menu_QuickHideEverything */ L"Alles schnell ausblenden",
    /* Menu_CheckUpdatesAuto */    L"Automatisch nach Updates suchen",
    /* Menu_CheckUpdatesNow */     L"Jetzt nach Updates suchen",

    // Safe-mode notice
    /* SafeMode_Title */           L"DexCorral im abgesicherten Modus gestartet",
    /* SafeMode_Body */            L"Der Explorer wurde wiederholt neu gestartet, während der Desktop-Hook aktiv "
                                   L"war; der Hook ist daher für diese Sitzung deaktiviert. Desktopsymbole bleiben "
                                   L"sichtbar. Beim nächsten Start wird der Hook wieder aktiviert.",

    // About dialog
    /* Title_About */              L"Über DexCorral",
    /* About_Body */               L"DexCorral - ein freier und quelloffener Desktop-Symbol-Organizer für Windows\n\n"
                                   L"Version: {0}\n\n"
                                   L"Copyright (C) 2026 Gunter Heiss\n\n"
                                   L"Dieses Programm ist freie Software: Sie können es unter den Bedingungen der GNU General Public License, wie von der Free Software Foundation veröffentlicht, weitergeben und/oder modifizieren, entweder gemäß Version 3 der Lizenz oder (nach Ihrer Wahl) jeder späteren Version.\n\n"
                                   L"Die Veröffentlichung dieses Programms erfolgt in der Hoffnung, dass es Ihnen von Nutzen sein wird, aber OHNE IRGENDEINE GARANTIE, sogar ohne die implizite Garantie der MARKTREIFE oder der VERWENDBARKEIT FÜR EINEN BESTIMMTEN ZWECK. Details finden Sie in der GNU General Public License.\n\n"
                                   L"Sie sollten ein Exemplar der GNU General Public License zusammen mit diesem Programm erhalten haben. Falls nicht, siehe https://www.gnu.org/licenses/\n\n"
                                   L"Website: https://dexcorral.com\n"
                                   L"GitHub: https://github.com/guHe330/DexCorral",

    // Shortcut-arrow toggle
    /* Arrow_HideConfirm */        L"Die Verknüpfungspfeile auf Desktopsymbolen werden ausgeblendet.\n\nDer Explorer wird neu gestartet, um die Änderung zu übernehmen. Fortfahren?",
    /* Arrow_RestoreConfirm */     L"Die Verknüpfungspfeile auf Desktopsymbolen werden wiederhergestellt.\n\nDer Explorer wird neu gestartet, um die Änderung zu übernehmen. Fortfahren?",
    /* Arrow_ChangeFailed */       L"Die Einstellung für Verknüpfungspfeile konnte nicht geändert werden.\n\nMöglicherweise sind Administratorrechte erforderlich.",

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
    /* Menu_CloseTab */            L"Tab schließen",
    /* Menu_DeleteCorral */        L"Corral löschen",
    /* Menu_RemoveFromCorral */    L"Aus dem Corral entfernen",

    // Confirmations
    /* Confirm_CloseTabBody */     L"Diesen Tab schließen?",
    /* Confirm_CloseTabTitle */    L"Schließen bestätigen",
    /* Confirm_DeleteCorralBody */ L"Dieses Corral löschen?",
    /* Confirm_DeleteCorralTitle */L"Löschen bestätigen",

    // Rename dialog / file rename
    /* Dlg_RenameCorral */         L"Corral umbenennen",
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
    /* Chk_UseAsDefault */         L"Als Standard für neue Corrals verwenden",
    /* Chk_ApplyToAll */           L"Änderungen auf alle Corrals anwenden",
    /* Chk_CopyStyleToAll */       L"Kompletten Stil auf alle Corrals kopieren",

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
    /* Reg_RegisterProcMissing */  L"DllRegisterServer wurde in DexCorralHook.dll nicht gefunden.",
    /* Reg_RegisterSuccess */      L"Die DexCorral-Shell-Erweiterung wurde erfolgreich registriert.\n\n"
                                   L"Starten Sie den Explorer neu, damit die Änderungen wirksam werden:\n"
                                   L"  1. Task-Manager öffnen\n"
                                   L"  2. 'Windows-Explorer' suchen\n"
                                   L"  3. Rechtsklick > Neu starten",
    /* Reg_RegisterFailed */       L"Die Shell-Erweiterung konnte nicht registriert werden.\n\nVersuchen Sie es als Administrator.",
    /* Reg_UnregisterProcMissing */L"DllUnregisterServer wurde in DexCorralHook.dll nicht gefunden.",
    /* Reg_UnregisterSuccess */    L"Die DexCorral-Shell-Erweiterung wurde erfolgreich deregistriert.\n\n"
                                   L"Starten Sie den Explorer neu, damit die Änderungen wirksam werden.",
    /* Reg_UnregisterFailed */     L"Die Shell-Erweiterung konnte nicht deregistriert werden.",
    /* Reg_Usage */                L"DexCorral - Desktop-Symbol-Organizer\n\n"
                                   L"Verwendung:\n"
                                   L"  DexCorral.exe --register     Shell-Erweiterung registrieren\n"
                                   L"  DexCorral.exe --unregister   Shell-Erweiterung deregistrieren\n"
                                   L"  DexCorral.exe --startup      In den Explorer injizieren und starten (vom Run-Schlüssel verwendet)\n"
                                   L"  DexCorral.exe --silent       Meldungsdialoge unterdrücken\n"
                                   L"  DexCorral.exe --force        Auf einer nicht unterstützten Windows-Version registrieren\n\n"
                                   L"DexCorral benötigt Windows 11; Windows 10 wird nicht unterstützt und ist ungetestet.\n"
                                   L"Nach der Registrierung den Explorer neu starten oder --startup verwenden.",
    /* Reg_NeedsWin11 */           L"DexCorral benötigt Windows 11 (Build {0} oder neuer).\n"
                                   L"Dieses System meldet Build {1}.\n\n"
                                   L"Windows 10 hat das Supportende erreicht; DexCorral ist darauf weder "
                                   L"getestet noch unterstützt.\n\n"
                                   L"Wenn du trotzdem registrieren möchtest, auf eigene Gefahr:\n"
                                   L"  DexCorral.exe --register --force\n\n"
                                   L"Bitte melde keine Fehler von nicht unterstützten Windows-Versionen.",
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
    wchar_t buf[16] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\DexCorral", L"Language",
                     RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
        return buf;
    return L"";
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
