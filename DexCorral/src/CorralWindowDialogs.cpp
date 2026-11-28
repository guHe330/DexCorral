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
 * CorralWindowDialogs.cpp - Settings dialogs, inline rename, and file dialogs
 *
 * Implements all dialog user interfaces using in-memory DLGTEMPLATE construction
 * (no .rc resource files). Includes appearance settings, tab/corral management,
 * file properties, folder properties, inline icon renaming, and open/save dialogs
 * for folder selection. Provides live preview of changes via direct config modification.
 */

#include "CorralWindow.h"
#include "DialogTemplate.h"
#include "DialogSkin.h"
#include "Presets.h"
#include "IconUtils.h"
#include <vector>
#include "App.h"
#include "Constants.h"
#include "Strings.h"
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <algorithm>
#include <cstdio>

// ============================================================================
// Icon rename support (inline edit control)
// ============================================================================

void CorralWindow::StartIconRename(int iconIndex)
{
    if (iconIndex < 0 || iconIndex >= (int)icons.size() || isRenamingIcon)
    {
        return;
    }

    // Special shell icons cannot be renamed
    if (icons[iconIndex].isSpecialIcon)
        return;

    isRenamingIcon = true;
    renamingIconIndex = iconIndex;
    renamingIconPath = icons[iconIndex].fullPath;
    originalName = icons[iconIndex].displayName;

    // Get the label rect in client coordinates
    RECT labelRect = GetIconLabelRect(iconIndex);

    // Adjust for scroll position
    labelRect.top -= scrollPosition;
    labelRect.bottom -= scrollPosition;

    // A layered window cannot show child controls, so the edit is a popup of its
    // own, owned by the corral and laid over the label. The corral itself keeps
    // its normal look underneath.
    MapWindowPoints(hwnd, nullptr, (POINT *)&labelRect, 2);
    hEditControl = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"EDIT",
        icons[iconIndex].displayName.c_str(),
        WS_POPUP | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER,
        labelRect.left,
        labelRect.top,
        labelRect.right - labelRect.left,
        labelRect.bottom - labelRect.top,
        hwnd,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);

    if (hEditControl)
    {
        // Set font to match the rendered icon label. The labels use the
        // system icon title font (SPI_GETICONTITLELOGFONT) at this Corral's DPI.
        LOGFONTW iconLogFont = {};
        if (SystemParametersInfoForDpi(SPI_GETICONTITLELOGFONT, sizeof(iconLogFont), &iconLogFont, 0,
                                       GetDpiForWindow(hwnd)))
        {
            iconLogFont.lfQuality = CLEARTYPE_QUALITY;
            hEditFont = CreateFontIndirectW(&iconLogFont);
        }
        else
        {
            hEditFont = CreateFontW(-Dpi(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        }
        SendMessageW(hEditControl, WM_SETFONT, (WPARAM)hEditFont, TRUE);

        // Select all text
        SendMessageW(hEditControl, EM_SETSEL, 0, -1);

        // Subclass the edit control to handle Enter/Escape and loss of focus
        SetWindowSubclass(hEditControl, EditSubclassProc, 0, (DWORD_PTR)this);

        // Show and focus it; the popup comes to the front, the corrals stay behind
        blockRaise++;
        ShowWindow(hEditControl, SW_SHOW);
        SetForegroundWindow(hEditControl);
        SetFocus(hEditControl);
        blockRaise--;
    }
    else
    {
        isRenamingIcon = false;
    }
}

void CorralWindow::EndIconRename(bool save)
{
    if (!isRenamingIcon || !hEditControl)
    {
        return;
    }

    // Re-entrancy guard: the failure message box below and DestroyWindow both
    // take the focus from the edit, and its WM_KILLFOCUS calls back in here.
    isRenamingIcon = false;

    // The icon list can be rebuilt while the edit is open (a folder change
    // reloads a virtual tab, a desktop event reloads a normal one), so the
    // index from StartIconRename may now name a different file. Find the file
    // again by its path; if it is gone, there is nothing to rename.
    renamingIconIndex = -1;
    for (size_t i = 0; i < icons.size(); i++)
    {
        if (!renamingIconPath.empty() && _wcsicmp(icons[i].fullPath.c_str(), renamingIconPath.c_str()) == 0)
        {
            renamingIconIndex = (int)i;
            break;
        }
    }

    if (save && renamingIconIndex >= 0)
    {
        // Get the new name from edit control
        int len = GetWindowTextLengthW(hEditControl);
        if (len > 0)
        {
            std::wstring newName(len + 1, L'\0');
            GetWindowTextW(hEditControl, &newName[0], len + 1);
            newName.resize(len);

            // Trim whitespace
            size_t start = newName.find_first_not_of(L" \t\r\n");
            size_t end = newName.find_last_not_of(L" \t\r\n");
            if (start != std::wstring::npos && end != std::wstring::npos)
            {
                newName = newName.substr(start, end - start + 1);
            }

            // Only rename if name actually changed and is not empty
            if (!newName.empty() && newName != originalName)
            {
                // Get the original file path
                std::wstring oldPath = icons[renamingIconIndex].fullPath;

                // Build new path
                size_t lastSlash = oldPath.find_last_of(L"\\");
                std::wstring newPath;
                // A name with a path separator would move the file to another
                // folder: such a name is refused with the other invalid ones.
                const bool validName = IconUtils::IsValidNewFileName(newName);
                if (lastSlash != std::wstring::npos && validName)
                {
                    // The editable name is the shell display name, which may
                    // omit the extension (.lnk, or any extension when "hide
                    // extensions" is on). Re-append it like Explorer does so
                    // the rename can't silently change the file type.
                    newPath = oldPath.substr(0, lastSlash + 1) +
                              IconUtils::NameWithHiddenExtension(oldPath.substr(lastSlash + 1),
                                                                 icons[renamingIconIndex].displayName, newName);
                }

                // Attempt to rename the file
                if (!newPath.empty() && MoveFileW(oldPath.c_str(), newPath.c_str()))
                {
                    std::string oldFileUtf8 = icons[renamingIconIndex].fileName;
                    std::wstring oldDisplayName = icons[renamingIconIndex].displayName;

                    // Update the icon data
                    icons[renamingIconIndex].fullPath = newPath;
                    icons[renamingIconIndex].wFileName = newPath.substr(lastSlash + 1);
                    icons[renamingIconIndex].fileName = WideToUtf8(icons[renamingIconIndex].wFileName);
                    icons[renamingIconIndex].displayName = DesktopIcons::GetShellDisplayName(newPath);

                    // Update config: match by old value (icons and Files indices
                    // can drift when an entry failed to load)
                    auto &files = GetActiveTab().Files;
                    auto fit = std::find(files.begin(), files.end(), oldFileUtf8);
                    if (fit != files.end())
                    {
                        *fit = icons[renamingIconIndex].fileName;
                    }

                    if (App::GetInstance())
                    {
                        App::GetInstance()->SaveConfig();
                        // The icon's identity (path and display name) changed.
                        // Keep the OLD identity hidden as a transition alias —
                        // the shell updates the desktop item asynchronously, and
                        // until it does, the item still matches the old identity.
                        // Then refresh the hidden list (now containing both) and
                        // re-park. Result: no frame where the icon is unmatched
                        // and flickers onto the desktop.
                        App::GetInstance()->AddTransientHiddenIcon(oldDisplayName, oldPath);
                        App::GetInstance()->UpdateHookHiddenIcons();
                        App::GetInstance()->PositionHiddenIconsUnderCorrals();
                    }
                }
                else
                {
                    const DWORD error = GetLastError();
                    const bool taken = !newPath.empty() && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS);
                    if (!validName)
                        MessageBoxBehind(Tr(Str::Err_RenameInvalidName), Tr(Str::Err_RenameFailedTitle),
                                         MB_OK | MB_ICONWARNING);
                    else if (taken)
                        MessageBoxBehind(TrFmt(Str::Err_RenameNameTaken, newName).c_str(),
                                         Tr(Str::Err_RenameFailedTitle), MB_OK | MB_ICONWARNING);
                    else
                        MessageBoxBehind(Tr(Str::Err_RenameFailedBody), Tr(Str::Err_RenameFailedTitle),
                                         MB_OK | MB_ICONERROR);

                    // A name the user can fix: keep the edit open with what was
                    // typed, as Explorer does. Escape still cancels.
                    if ((!validName || taken) && IsWindow(hEditControl))
                    {
                        isRenamingIcon = true;
                        blockRaise++;
                        SetForegroundWindow(hEditControl);
                        SetFocus(hEditControl);
                        blockRaise--;
                        SendMessageW(hEditControl, EM_SETSEL, 0, -1);
                        return;
                    }
                }
            }
        }
    }

    // Clean up edit control
    // Note: Set state flags BEFORE DestroyWindow to prevent re-entrancy
    // (DestroyWindow triggers WM_KILLFOCUS which would call EndIconRename again)
    HWND editToDestroy = hEditControl;
    hEditControl = nullptr;
    isRenamingIcon = false;
    renamingIconIndex = -1;
    renamingIconPath.clear();
    originalName.clear();

    if (editToDestroy)
    {
        blockRaise++; // Windows hands the activation back to the owning corral
        DestroyWindow(editToDestroy);
        blockRaise--;
    }

    if (hEditFont)
    {
        DeleteObject(hEditFont);
        hEditFont = nullptr;
    }

    PinToBandTop();

    // Redraw to show the updated label
    UpdateLayeredContent();
}

LRESULT CALLBACK CorralWindow::EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    CorralWindow *window = (CorralWindow *)dwRefData;

    switch (uMsg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN)
        {
            // Enter - save and end rename
            window->EndIconRename(true);
            return 0;
        }
        else if (wParam == VK_ESCAPE)
        {
            // Escape - cancel rename
            window->EndIconRename(false);
            return 0;
        }
        break;

    case WM_KILLFOCUS:
        // Lost focus - save rename
        window->EndIconRename(true);
        return 0;

    case WM_NCDESTROY:
        // Remove subclass before destruction
        RemoveWindowSubclass(hwnd, EditSubclassProc, uIdSubclass);
        break;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Rename dialog
// ============================================================================

// Where the footer band of the rename dialog begins (dialog units)
static const int RENAME_FOOTER_TOP = 32;

struct NamePrompt
{
    wchar_t *name; // 256 characters: the text to start with and, on OK, the trimmed result
    int maxLength; // Characters the edit accepts
};

/// What the name edit holds, without blanks around it
static std::wstring NamePromptText(HWND hDlg)
{
    wchar_t buffer[256] = {};
    GetDlgItemTextW(hDlg, 101, buffer, 256);
    const std::wstring text = buffer;
    const size_t first = text.find_first_not_of(L" \t");
    if (first == std::wstring::npos)
        return L"";
    return text.substr(first, text.find_last_not_of(L" \t") - first + 1);
}

static INT_PTR CALLBACK RenameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
        const NamePrompt *prompt = (const NamePrompt *)lParam;
        HWND hEdit = GetDlgItem(hDlg, 101);
        SendMessageW(hEdit, EM_LIMITTEXT, prompt->maxLength, 0);
        SetDlgItemTextW(hDlg, 101, prompt->name);
        EnableWindow(GetDlgItem(hDlg, IDOK), !NamePromptText(hDlg).empty()); // A blank name is not a name

        SendMessageW(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);
        DialogSkin::Attach(hDlg, RENAME_FOOTER_TOP);

        HWND hParent = GetParent(hDlg);
        if (hParent)
        {
            RECT parentRect, dlgRect;
            GetWindowRect(hParent, &parentRect);
            GetWindowRect(hDlg, &dlgRect);
            int dlgWidth = dlgRect.right - dlgRect.left;
            int dlgHeight = dlgRect.bottom - dlgRect.top;

            // Get monitor work area
            HMONITOR hMon = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfo(hMon, &mi);

            int cx = parentRect.left + (parentRect.right - parentRect.left - dlgWidth) / 2;
            int cy = parentRect.top + (parentRect.bottom - parentRect.top - dlgHeight) / 2;

            // Clamp to monitor bounds
            if (cx + dlgWidth > mi.rcWork.right)
                cx = mi.rcWork.right - dlgWidth;
            if (cx < mi.rcWork.left)
                cx = mi.rcWork.left;
            if (cy + dlgHeight > mi.rcWork.bottom)
                cy = mi.rcWork.bottom - dlgHeight;
            if (cy < mi.rcWork.top)
                cy = mi.rcWork.top;

            SetWindowPos(hDlg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 101 && HIWORD(wParam) == EN_CHANGE)
            EnableWindow(GetDlgItem(hDlg, IDOK), !NamePromptText(hDlg).empty());
        if (LOWORD(wParam) == IDOK)
        {
            const std::wstring text = NamePromptText(hDlg);
            if (text.empty())
                return TRUE; // OK is disabled then; this covers Enter
            NamePrompt *prompt = (NamePrompt *)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            wcscpy_s(prompt->name, 256, text.c_str());
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    INT_PTR skinned;
    if (DialogSkin::HandleMessage(hDlg, msg, wParam, lParam, skinned))
        return skinned;
    return FALSE;
}

void CorralWindow::PromptRename()
{
    ShowRenameDialog();
}

/// Asks for a name in the small edit dialog. `name` (256 characters) holds
/// the text to start with and, on true, what the user entered without blanks
/// around it, never empty. The edit takes at most `maxLength` characters.
static bool PromptForName(HWND owner, const wchar_t *title, wchar_t *name, int maxLength = 255)
{
    NamePrompt prompt = {name, maxLength};
    WORD dlgTemplate[512] = {};
    WORD *p = BeginDlgTemplate(dlgTemplate,
                               DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT,
                               3, 180, RENAME_FOOTER_TOP + 28, title);
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 10, 10, 160, 14, 101, 0xFFFF,
                   DLG_CLASS_EDIT, nullptr);
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 62, RENAME_FOOTER_TOP + 7, 50, 14, IDOK,
                   0xFFFF, DLG_CLASS_BUTTON, Tr(Str::Btn_OK));
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 118, RENAME_FOOTER_TOP + 7, 50, 14, IDCANCEL,
                   0xFFFF, DLG_CLASS_BUTTON, Tr(Str::Btn_Cancel));
    return DialogBoxIndirectParamW(GetModuleHandleW(nullptr), (DLGTEMPLATE *)dlgTemplate, owner, RenameDlgProc,
                                   (LPARAM)&prompt) == IDOK;
}

void CorralWindow::ShowRenameDialog()
{
    wchar_t nameBuffer[256] = {};
    std::wstring currentTitle = Utf8ToWide(GetActiveTab().Title);
    wcsncpy_s(nameBuffer, currentTitle.c_str(), _TRUNCATE);

    blockRaise++; // The prompt is owned by this corral: see MessageBoxBehind
    const INT_PTR result = PromptForName(hwnd, Tr(Str::Dlg_RenameTab), nameBuffer) ? IDOK : IDCANCEL;
    blockRaise--;
    PinToBandTop();

    if (result == IDOK)
    {
        int sz = WideCharToMultiByte(CP_UTF8, 0, nameBuffer, -1, nullptr, 0, nullptr, nullptr);
        std::string newTitle(sz - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, nameBuffer, -1, &newTitle[0], sz, nullptr, nullptr);

        GetActiveTab().Title = newTitle;
        SetWindowTextW(hwnd, nameBuffer);
        UpdateLayeredContent();

        if (App::GetInstance())
        {
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// Appearance dialog
// ============================================================================

struct AppearanceDlgData
{
    // Background color & opacity
    BYTE alpha;
    COLORREF color;
    HBRUSH hBrush;
    HWND previewWindow;
    std::string *colorHex;
    bool colorChanged;

    // Border opacity (background group)
    int borderOpacity;
    bool borderOpacityChanged;

    // Header settings
    int titleBarHeight;
    bool titleBarHeightChanged;
    int headerOpacity;
    bool headerOpacityChanged;
    std::wstring fontName;
    int fontSize;
    bool fontChanged;
    COLORREF fontColor;
    HBRUSH fontColorBrush;
    bool fontColorChanged;
    int fontOpacity;
    bool fontOpacityChanged;

    // Icon settings
    int iconOpacity;
    bool iconOpacityChanged;
    int labelOpacity;
    bool labelOpacityChanged;
    COLORREF tintColor;
    HBRUSH tintColorBrush;
    int tintStrength;
    bool tintChanged;
    int iconSpacingX;
    bool iconSpacingXChanged;
    int iconSpacingY;
    bool iconSpacingYChanged;

    // Own presets were saved or deleted: the config is written whichever way the dialog ends
    bool presetsChanged;
    // A preset was loaded: OK gives the look to every tab of the Corral, not only the active one
    bool presetLoaded;

    // Checkboxes
    bool useAsDefault;
    bool applyChangesToAll;    // Apply only changed settings to all corrals
    bool applyEverythingToAll; // Copy full appearance to all corrals

    // Back-references for live preview
    CorralWindowConfig *corralConfig;
    CorralTabConfig *activeTabConfig;  // Active tab — font settings live here
    CorralWindow *corralWindow; // For triggering layout recalculation
};

static void AppearanceUpdateLivePreview(AppearanceDlgData *data, bool relayout = false)
{
    if (relayout && data->corralWindow)
    {
        data->corralWindow->RecalculateLayout();
        return; // RecalculateLayout already calls UpdateLayeredContent
    }
    if (data->previewWindow)
        InvalidateRect(data->previewWindow, nullptr, FALSE);
}

// Control id of the Reset to Defaults button
static const WORD IDC_APPEARANCE_RESET = 134;

/// Moves a slider and runs the dialog's own WM_HSCROLL handling for it, so the
/// label, the changed flag and the live preview follow exactly as for a drag.
static void AppearanceSetSlider(HWND hDlg, int sliderId, int pos)
{
    HWND hSlider = GetDlgItem(hDlg, sliderId);
    SendMessageW(hSlider, TBM_SETPOS, TRUE, pos);
    SendMessageW(hDlg, WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, 0), (LPARAM)hSlider);
}

/// "#RRGGBB" or "#AARRGGBB" -> 0xAARRGGBB (alpha 0 for the short form); `fallback` if malformed
static unsigned int ParseHexColor(const std::string &hex, unsigned int fallback)
{
    unsigned int value = fallback;
    if (hex.size() >= 7 && hex[0] == '#')
        sscanf_s(hex.c_str() + 1, "%x", &value);
    return value;
}

static COLORREF RgbOf(unsigned int argb)
{
    return RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
}

// "Windows accent color" (ID 135): while it is on, the tab is drawn in the accent
// colour with black or white header text, so the two colour buttons have no effect
static void AppearanceSyncAccentControls(HWND hDlg, const AppearanceDlgData *data)
{
    const bool accent = data->activeTabConfig && data->activeTabConfig->UseAccentColor;
    CheckDlgButton(hDlg, 135, accent ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(hDlg, 106), !accent);
    EnableWindow(GetDlgItem(hDlg, 115), !accent);
}

// The preset row: list, "Save as...", "Delete"
static const WORD IDC_APPEARANCE_PRESET = 170;
static const WORD IDC_APPEARANCE_PRESET_SAVE = 171;
static const WORD IDC_APPEARANCE_PRESET_DELETE = 172;

/// Puts a whole look into the dialog: every control, the live preview and
/// the "changed" flags follow, as if the user had set each value by hand
static void AppearanceLoadLook(HWND hDlg, AppearanceDlgData *data, const AppearanceSettings &look)
{
    // Colours and font first. They have no slider to carry them, and the
    // background slider below rebuilds the colour string from data->color.
    unsigned int background = ParseHexColor(look.ColorHex, 0x99000000);
    data->color = RgbOf(background);
    data->colorChanged = true;
    if (data->hBrush)
        DeleteObject(data->hBrush);
    data->hBrush = CreateSolidBrush(data->color);
    InvalidateRect(GetDlgItem(hDlg, 105), nullptr, TRUE);
    if (data->activeTabConfig)
        data->activeTabConfig->UseAccentColor = look.UseAccentColor;
    AppearanceSyncAccentControls(hDlg, data);

    data->fontName = CorralWindow::Utf8ToWide(look.HeaderFontName);
    data->fontSize = look.HeaderFontSize;
    data->fontChanged = true;
    SetDlgItemTextW(hDlg, 112, data->fontName.c_str());

    data->fontColor = RgbOf(ParseHexColor(look.HeaderFontColor, 0xFFFFFF));
    data->fontColorChanged = true;
    if (data->fontColorBrush)
        DeleteObject(data->fontColorBrush);
    data->fontColorBrush = CreateSolidBrush(data->fontColor);
    InvalidateRect(GetDlgItem(hDlg, 114), nullptr, TRUE);

    data->tintColor = RgbOf(ParseHexColor(look.IconTintColor, 0x000000));
    data->tintChanged = true;
    if (data->tintColorBrush)
        DeleteObject(data->tintColorBrush);
    data->tintColorBrush = CreateSolidBrush(data->tintColor);
    InvalidateRect(GetDlgItem(hDlg, 118), nullptr, TRUE);

    if (data->activeTabConfig)
    {
        data->activeTabConfig->HeaderFontName = look.HeaderFontName;
        data->activeTabConfig->HeaderFontSize = look.HeaderFontSize;
        data->activeTabConfig->HeaderFontColor = look.HeaderFontColor;
    }
    if (data->corralConfig)
        data->corralConfig->IconTintColor = look.IconTintColor;

    // Everything with a slider goes through the slider's own handler
    AppearanceSetSlider(hDlg, 102, (background >> 24) & 0xFF);
    AppearanceSetSlider(hDlg, 122, look.BorderOpacity);
    AppearanceSetSlider(hDlg, 124, look.HeaderOpacity);
    AppearanceSetSlider(hDlg, 126, look.HeaderFontOpacity);
    AppearanceSetSlider(hDlg, 116, look.IconOpacity);
    AppearanceSetSlider(hDlg, 128, look.IconLabelOpacity);
    AppearanceSetSlider(hDlg, 120, look.IconTintStrength);
    AppearanceSetSlider(hDlg, 130, look.IconSpacingXPercent);
    AppearanceSetSlider(hDlg, 132, look.IconSpacingYPercent);
    AppearanceSetSlider(hDlg, 110, look.TitleBarHeight); // last: it relayouts, picking up the font too
}

/// The look the dialog shows right now
static AppearanceSettings AppearanceCurrentLook(const AppearanceDlgData *data)
{
    AppearanceSettings look;
    char hex[16];
    sprintf_s(hex, "#%02X%02X%02X%02X", data->alpha, GetRValue(data->color), GetGValue(data->color), GetBValue(data->color));
    look.ColorHex = hex;
    look.UseAccentColor = data->activeTabConfig && data->activeTabConfig->UseAccentColor;
    look.TitleBarHeight = data->titleBarHeight;
    look.HeaderOpacity = data->headerOpacity;
    look.BorderOpacity = data->borderOpacity;
    look.HeaderFontName = CorralWindow::WideToUtf8(data->fontName);
    look.HeaderFontSize = data->fontSize;
    sprintf_s(hex, "#%02X%02X%02X", GetRValue(data->fontColor), GetGValue(data->fontColor), GetBValue(data->fontColor));
    look.HeaderFontColor = hex;
    look.HeaderFontOpacity = data->fontOpacity;
    look.IconOpacity = data->iconOpacity;
    look.IconLabelOpacity = data->labelOpacity;
    sprintf_s(hex, "#%02X%02X%02X", GetRValue(data->tintColor), GetGValue(data->tintColor), GetBValue(data->tintColor));
    look.IconTintColor = hex;
    look.IconTintStrength = data->tintStrength;
    look.IconSpacingXPercent = data->iconSpacingX;
    look.IconSpacingYPercent = data->iconSpacingY;
    return look;
}

static std::string AppearanceOwnPresetName(HWND hDlg);

/// Only an own preset can be deleted
static void AppearanceSyncPresetButtons(HWND hDlg)
{
    HWND remove = GetDlgItem(hDlg, IDC_APPEARANCE_PRESET_DELETE);
    const bool own = !AppearanceOwnPresetName(hDlg).empty();
    // A disabled control must not keep the focus, or the keyboard is stuck in the dialog
    if (!own && GetFocus() == remove)
        SendMessageW(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDC_APPEARANCE_PRESET), TRUE);
    EnableWindow(remove, own);
}

/// Fills the preset list: the prompt, the shipped looks, then the user's own.
/// Selects the own preset named `select`, else the prompt.
static void AppearanceFillPresets(HWND hDlg, const std::string &select)
{
    HWND list = GetDlgItem(hDlg, IDC_APPEARANCE_PRESET);
    SendMessageW(list, CB_RESETCONTENT, 0, 0);
    SendMessageW(list, CB_ADDSTRING, 0, (LPARAM)Tr(Str::Preset_Load));
    for (const auto &preset : Presets::Shipped())
        SendMessageW(list, CB_ADDSTRING, 0, (LPARAM)Tr(preset.name));
    int selected = 0;
    if (App *app = App::GetInstance())
    {
        for (const auto &preset : app->OwnPresets())
        {
            const LRESULT at = SendMessageW(list, CB_ADDSTRING, 0, (LPARAM)CorralWindow::Utf8ToWide(preset.Name).c_str());
            if (preset.Name == select && at >= 0)
                selected = (int)at;
        }
    }
    SendMessageW(list, CB_SETCURSEL, selected, 0);
    AppearanceSyncPresetButtons(hDlg);
}

/// The look of the list entry that is selected; false for the prompt
static bool AppearancePresetAt(HWND hDlg, AppearanceSettings &look)
{
    const auto shipped = Presets::Shipped();
    const int sel = (int)SendDlgItemMessageW(hDlg, IDC_APPEARANCE_PRESET, CB_GETCURSEL, 0, 0);
    if (sel >= 1 && sel <= (int)shipped.size())
    {
        look = shipped[sel - 1].look;
        return true;
    }
    App *app = App::GetInstance();
    const int own = sel - 1 - (int)shipped.size();
    if (app && own >= 0 && own < (int)app->OwnPresets().size())
    {
        look = app->OwnPresets()[own].Look;
        return true;
    }
    return false;
}

/// The name of the own preset that is selected; empty for the prompt and the shipped looks
static std::string AppearanceOwnPresetName(HWND hDlg)
{
    App *app = App::GetInstance();
    const int own = (int)SendDlgItemMessageW(hDlg, IDC_APPEARANCE_PRESET, CB_GETCURSEL, 0, 0) - 1 -
                    (int)Presets::Shipped().size();
    return (app && own >= 0 && own < (int)app->OwnPresets().size()) ? app->OwnPresets()[own].Name : std::string();
}


// Layout of the Appearance dialog, in dialog units
static const short APPEARANCE_DLG_W = 246;
static const short APPEARANCE_PRESET_TOP = 10;                         // The preset row
static const short APPEARANCE_STRIP_TOP = APPEARANCE_PRESET_TOP + 24;  // The page strip
static const short APPEARANCE_PAGE_TOP = APPEARANCE_STRIP_TOP + 26;    // First row of a page
static const short APPEARANCE_FOOTER_TOP = APPEARANCE_PAGE_TOP + 6 * 18 + 2 + 3 * 14 + 4;

// The page strip: three buttons with consecutive ids
static const WORD IDC_APPEARANCE_PAGE = 140;
static const int APPEARANCE_PAGES = 3;

/// Shows the controls of one page and hides those of the others
static void AppearanceShowPage(HWND hDlg, int page)
{
    static const std::vector<int> controls[APPEARANCE_PAGES] = {
        {150, 105, 106, 135, 151, 102, 103, 152, 122, 123},
        {153, 110, 111, 154, 124, 125, 155, 112, 113, 156, 114, 115, 157, 126, 127},
        {158, 116, 117, 159, 128, 129, 160, 118, 119, 161, 120, 121, 162, 130, 131, 163, 132, 133},
    };
    for (int i = 0; i < APPEARANCE_PAGES; i++)
    {
        for (int id : controls[i])
            ShowWindow(GetDlgItem(hDlg, id), i == page ? SW_SHOW : SW_HIDE);
        DialogSkin::MakePageTab(hDlg, IDC_APPEARANCE_PAGE + i, i == page);
    }
}

static INT_PTR CALLBACK AppearanceDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AppearanceDlgData *data = (AppearanceDlgData *)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        data = (AppearanceDlgData *)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        // Create brushes
        data->hBrush = CreateSolidBrush(data->color);
        data->fontColorBrush = CreateSolidBrush(data->fontColor);
        data->tintColorBrush = CreateSolidBrush(data->tintColor);
        AppearanceSyncAccentControls(hDlg, data);

        // --- Opacity group: six sliders, in the order they appear ---

        // Background opacity (ID 102)
        HWND hSlider = GetDlgItem(hDlg, 102);
        SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageW(hSlider, TBM_SETPOS, TRUE, data->alpha);
        wchar_t label[32];
        swprintf_s(label, L"%d%%", (data->alpha * 100) / 255);
        SetDlgItemTextW(hDlg, 103, label);

        // Border opacity (ID 122)
        HWND hBorderSlider = GetDlgItem(hDlg, 122);
        SendMessageW(hBorderSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageW(hBorderSlider, TBM_SETPOS, TRUE, data->borderOpacity);
        swprintf_s(label, L"%d%%", (data->borderOpacity * 100) / 255);
        SetDlgItemTextW(hDlg, 123, label);

        // Header opacity (ID 124). Floored at HEADER_OPACITY_MIN: the header is
        // the corral's grab handle and must stay findable.
        HWND hHeaderOpacitySlider = GetDlgItem(hDlg, 124);
        SendMessageW(hHeaderOpacitySlider, TBM_SETRANGE, TRUE, MAKELPARAM(HEADER_OPACITY_MIN, 255));
        SendMessageW(hHeaderOpacitySlider, TBM_SETPOS, TRUE, data->headerOpacity);
        swprintf_s(label, L"%d%%", (data->headerOpacity * 100) / 255);
        SetDlgItemTextW(hDlg, 125, label);

        // Header label opacity (ID 126). Unlike the header itself this may go to
        // 0 — the header stays draggable without its title. Per-tab, unlike the
        // rest of this group, because it belongs with the other font settings.
        HWND hFontOpacitySlider = GetDlgItem(hDlg, 126);
        SendMessageW(hFontOpacitySlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageW(hFontOpacitySlider, TBM_SETPOS, TRUE, data->fontOpacity);
        swprintf_s(label, L"%d%%", (data->fontOpacity * 100) / 255);
        SetDlgItemTextW(hDlg, 127, label);

        // Icon opacity (ID 116)
        HWND hIconSlider = GetDlgItem(hDlg, 116);
        SendMessageW(hIconSlider, TBM_SETRANGE, TRUE, MAKELPARAM(5, 255));
        SendMessageW(hIconSlider, TBM_SETPOS, TRUE, data->iconOpacity);
        swprintf_s(label, L"%d%%", (data->iconOpacity * 100) / 255);
        SetDlgItemTextW(hDlg, 117, label);

        // Icon label opacity (ID 128)
        HWND hLabelSlider = GetDlgItem(hDlg, 128);
        SendMessageW(hLabelSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageW(hLabelSlider, TBM_SETPOS, TRUE, data->labelOpacity);
        swprintf_s(label, L"%d%%", (data->labelOpacity * 100) / 255);
        SetDlgItemTextW(hDlg, 129, label);

        // --- Header group ---

        // Header height (ID 110)
        HWND hHeightSlider = GetDlgItem(hDlg, 110);
        SendMessageW(hHeightSlider, TBM_SETRANGE, TRUE, MAKELPARAM(20, 64));
        SendMessageW(hHeightSlider, TBM_SETPOS, TRUE, data->titleBarHeight);
        swprintf_s(label, L"%dpx", data->titleBarHeight);
        SetDlgItemTextW(hDlg, 111, label);

        // Font name display (ID 112)
        SetDlgItemTextW(hDlg, 112, data->fontName.c_str());

        // --- Icons group ---

        // Tint strength (ID 120)
        HWND hTintSlider = GetDlgItem(hDlg, 120);
        SendMessageW(hTintSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageW(hTintSlider, TBM_SETPOS, TRUE, data->tintStrength);
        swprintf_s(label, L"%d%%", (data->tintStrength * 100) / 255);
        SetDlgItemTextW(hDlg, 121, label);

        // --- Icon Spacing group ---

        // Setup horizontal spacing slider (ID 130)
        HWND hSpacingXSlider = GetDlgItem(hDlg, 130);
        SendMessageW(hSpacingXSlider, TBM_SETRANGE, TRUE, MAKELPARAM(50, 200));
        SendMessageW(hSpacingXSlider, TBM_SETPOS, TRUE, data->iconSpacingX);
        swprintf_s(label, L"%d%%", data->iconSpacingX);
        SetDlgItemTextW(hDlg, 131, label);

        // Setup vertical spacing slider (ID 132)
        HWND hSpacingYSlider = GetDlgItem(hDlg, 132);
        SendMessageW(hSpacingYSlider, TBM_SETRANGE, TRUE, MAKELPARAM(50, 200));
        SendMessageW(hSpacingYSlider, TBM_SETPOS, TRUE, data->iconSpacingY);
        swprintf_s(label, L"%d%%", data->iconSpacingY);
        SetDlgItemTextW(hDlg, 133, label);

        DialogSkin::Attach(hDlg, APPEARANCE_FOOTER_TOP);
        AppearanceShowPage(hDlg, 0);
        AppearanceFillPresets(hDlg, "");

        // Position below parent corral
        HWND hParent = GetParent(hDlg);
        if (hParent)
        {
            RECT parentRect, dlgRect;
            GetWindowRect(hParent, &parentRect);
            GetWindowRect(hDlg, &dlgRect);
            int dlgWidth = dlgRect.right - dlgRect.left;
            int dlgHeight = dlgRect.bottom - dlgRect.top;
            int parentWidth = parentRect.right - parentRect.left;

            HMONITOR hMon = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfo(hMon, &mi);

            int cx = parentRect.left + (parentWidth - dlgWidth) / 2;
            int cy = parentRect.bottom + 5;
            if (cy + dlgHeight > mi.rcWork.bottom)
                cy = parentRect.top - dlgHeight - 5;
            if (cy < mi.rcWork.top)
                cy = mi.rcWork.top;
            if (cx + dlgWidth > mi.rcWork.right)
                cx = mi.rcWork.right - dlgWidth;
            if (cx < mi.rcWork.left)
                cx = mi.rcWork.left;

            SetWindowPos(hDlg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return TRUE;
    }
    case WM_CTLCOLORSTATIC:
    {
        int ctrlId = GetDlgCtrlID((HWND)lParam);
        if (ctrlId == 105 && data)
            return (INT_PTR)data->hBrush;
        if (ctrlId == 114 && data)
            return (INT_PTR)data->fontColorBrush;
        if (ctrlId == 118 && data)
            return (INT_PTR)data->tintColorBrush;
        break;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == 106)
        { // Change Background Color
            static COLORREF customColors[16] = {};
            CHOOSECOLORW cc = {};
            cc.lStructSize = sizeof(CHOOSECOLORW);
            cc.hwndOwner = hDlg;
            cc.lpCustColors = customColors;
            cc.rgbResult = data->color;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColorW(&cc))
            {
                data->color = cc.rgbResult;
                data->colorChanged = true;
                if (data->hBrush)
                    DeleteObject(data->hBrush);
                data->hBrush = CreateSolidBrush(data->color);
                InvalidateRect(GetDlgItem(hDlg, 105), nullptr, TRUE);

                BYTE r = GetRValue(data->color);
                BYTE g = GetGValue(data->color);
                BYTE b = GetBValue(data->color);
                char hexBuf[16];
                sprintf_s(hexBuf, "#%02X%02X%02X%02X", data->alpha, r, g, b);
                *data->colorHex = hexBuf;
                AppearanceUpdateLivePreview(data);
            }
            return TRUE;
        }

        if (id >= IDC_APPEARANCE_PAGE && id < IDC_APPEARANCE_PAGE + APPEARANCE_PAGES)
        {
            AppearanceShowPage(hDlg, id - IDC_APPEARANCE_PAGE);
            return TRUE;
        }

        if (id == 135)
        { // Windows accent color
            if (data->activeTabConfig)
            {
                data->activeTabConfig->UseAccentColor = IsDlgButtonChecked(hDlg, 135) == BST_CHECKED;
                data->colorChanged = true; // Travels with the colour to "apply to all"
                AppearanceSyncAccentControls(hDlg, data);
                AppearanceUpdateLivePreview(data);
            }
            return TRUE;
        }

        if (id == 113)
        { // Choose Font
            // Convert point size to pixel height for LOGFONT
            HDC hdc = GetDC(hDlg);
            int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hDlg, hdc);
            LOGFONTW lf = {};
            lf.lfHeight = -MulDiv(data->fontSize, dpi, 72);
            lf.lfWeight = FW_SEMIBOLD;
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcsncpy_s(lf.lfFaceName, data->fontName.c_str(), _TRUNCATE);

            CHOOSEFONTW cf = {};
            cf.lStructSize = sizeof(CHOOSEFONTW);
            cf.hwndOwner = hDlg;
            cf.lpLogFont = &lf;
            // No style list: the header is always drawn semibold and upright
            cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL | CF_NOSTYLESEL;

            if (ChooseFontW(&cf))
            {
                data->fontName = lf.lfFaceName;
                data->fontSize = (cf.iPointSize + 5) / 10; // iPointSize is in 1/10 points; only whole points are kept
                data->fontChanged = true;
                SetDlgItemTextW(hDlg, 112, data->fontName.c_str());

                // Live preview
                if (data->activeTabConfig)
                {
                    data->activeTabConfig->HeaderFontName.resize(WideCharToMultiByte(CP_UTF8, 0, data->fontName.c_str(), -1, nullptr, 0, nullptr, nullptr) - 1);
                    WideCharToMultiByte(CP_UTF8, 0, data->fontName.c_str(), -1, &data->activeTabConfig->HeaderFontName[0], (int)data->activeTabConfig->HeaderFontName.size() + 1, nullptr, nullptr);
                    data->activeTabConfig->HeaderFontSize = data->fontSize;
                    AppearanceUpdateLivePreview(data);
                }
            }
            return TRUE;
        }

        if (id == 115)
        { // Change Font Color
            static COLORREF customFontColors[16] = {};
            CHOOSECOLORW cc = {};
            cc.lStructSize = sizeof(CHOOSECOLORW);
            cc.hwndOwner = hDlg;
            cc.lpCustColors = customFontColors;
            cc.rgbResult = data->fontColor;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColorW(&cc))
            {
                data->fontColor = cc.rgbResult;
                data->fontColorChanged = true;
                if (data->fontColorBrush)
                    DeleteObject(data->fontColorBrush);
                data->fontColorBrush = CreateSolidBrush(data->fontColor);
                InvalidateRect(GetDlgItem(hDlg, 114), nullptr, TRUE);

                // Live preview
                if (data->activeTabConfig)
                {
                    char hexBuf[16];
                    sprintf_s(hexBuf, "#%02X%02X%02X", GetRValue(data->fontColor), GetGValue(data->fontColor), GetBValue(data->fontColor));
                    data->activeTabConfig->HeaderFontColor = hexBuf;
                    AppearanceUpdateLivePreview(data);
                }
            }
            return TRUE;
        }

        if (id == 119)
        { // Change Tint Color
            static COLORREF customTintColors[16] = {};
            CHOOSECOLORW cc = {};
            cc.lStructSize = sizeof(CHOOSECOLORW);
            cc.hwndOwner = hDlg;
            cc.lpCustColors = customTintColors;
            cc.rgbResult = data->tintColor;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColorW(&cc))
            {
                data->tintColor = cc.rgbResult;
                data->tintChanged = true;
                if (data->tintColorBrush)
                    DeleteObject(data->tintColorBrush);
                data->tintColorBrush = CreateSolidBrush(data->tintColor);
                InvalidateRect(GetDlgItem(hDlg, 118), nullptr, TRUE);

                if (data->corralConfig)
                {
                    char hexBuf[16];
                    sprintf_s(hexBuf, "#%02X%02X%02X", GetRValue(data->tintColor), GetGValue(data->tintColor), GetBValue(data->tintColor));
                    data->corralConfig->IconTintColor = hexBuf;
                    AppearanceUpdateLivePreview(data);
                }
            }
            return TRUE;
        }

        if (id == IDC_APPEARANCE_RESET)
        { // Reset to Defaults: every control goes to the style a new corral would get
            if (App *app = App::GetInstance())
            {
                AppearanceLoadLook(hDlg, data, app->GetDefaultAppearance());
                data->presetLoaded = true; // A new Corral's tabs share one look, so OK gives it to every tab
            }
            return TRUE;
        }

        if (id == IDC_APPEARANCE_PRESET && HIWORD(wParam) == CBN_SELCHANGE)
        {
            AppearanceSettings look;
            if (AppearancePresetAt(hDlg, look))
            {
                AppearanceLoadLook(hDlg, data, look);
                data->presetLoaded = true;
            }
            AppearanceSyncPresetButtons(hDlg);
            return TRUE;
        }

        if (id == IDC_APPEARANCE_PRESET_SAVE)
        {
            App *app = App::GetInstance();
            if (!app)
                return TRUE;
            // Offer the name of the own preset that is selected, for saving over it
            wchar_t name[256] = {};
            const std::string selected = AppearanceOwnPresetName(hDlg);
            wcsncpy_s(name, CorralWindow::Utf8ToWide(selected).c_str(), _TRUNCATE);
            if (!PromptForName(hDlg, Tr(Str::Title_SavePreset), name, (int)Presets::MAX_NAME))
                return TRUE;

            const std::string clean = Presets::CleanName(CorralWindow::WideToUtf8(name));
            auto &own = app->OwnPresets();
            const bool exists = std::any_of(own.begin(), own.end(), [&](const LookPreset &p) { return p.Name == clean; });
            if (exists && MessageBoxW(hDlg, TrFmt(Str::Msg_PresetReplace, CorralWindow::Utf8ToWide(clean)).c_str(),
                                      Tr(Str::App_Name), MB_YESNO | MB_ICONQUESTION) != IDYES)
                return TRUE;
            switch (Presets::Upsert(own, clean, AppearanceCurrentLook(data)))
            {
            case Presets::UpsertResult::Full:
                MessageBoxW(hDlg, TrFmt(Str::Msg_PresetsFull, std::to_wstring(Presets::MAX_OWN)).c_str(),
                            Tr(Str::App_Name), MB_OK | MB_ICONINFORMATION);
                break;
            case Presets::UpsertResult::BadName:
                break; // Nothing entered
            default:
                data->presetsChanged = true;
                AppearanceFillPresets(hDlg, clean);
            }
            return TRUE;
        }

        if (id == IDC_APPEARANCE_PRESET_DELETE)
        {
            App *app = App::GetInstance();
            const std::string name = AppearanceOwnPresetName(hDlg);
            if (app && !name.empty() &&
                MessageBoxW(hDlg, TrFmt(Str::Msg_PresetDelete, CorralWindow::Utf8ToWide(name)).c_str(), Tr(Str::App_Name),
                            MB_YESNO | MB_ICONQUESTION) == IDYES &&
                Presets::Remove(app->OwnPresets(), name))
            {
                data->presetsChanged = true;
                AppearanceFillPresets(hDlg, "");
            }
            return TRUE;
        }

        // Mutual exclusion: checking one "apply to all" unchecks the other
        if (id == 108 && HIWORD(wParam) == BN_CLICKED)
        {
            if (SendDlgItemMessageW(hDlg, 108, BM_GETCHECK, 0, 0) == BST_CHECKED)
                SendDlgItemMessageW(hDlg, 109, BM_SETCHECK, BST_UNCHECKED, 0);
            return TRUE;
        }
        if (id == 109 && HIWORD(wParam) == BN_CLICKED)
        {
            if (SendDlgItemMessageW(hDlg, 109, BM_GETCHECK, 0, 0) == BST_CHECKED)
                SendDlgItemMessageW(hDlg, 108, BM_SETCHECK, BST_UNCHECKED, 0);
            return TRUE;
        }

        if (id == IDOK)
        {
            data->useAsDefault = (SendDlgItemMessageW(hDlg, 107, BM_GETCHECK, 0, 0) == BST_CHECKED);
            data->applyChangesToAll = (SendDlgItemMessageW(hDlg, 108, BM_GETCHECK, 0, 0) == BST_CHECKED);
            data->applyEverythingToAll = (SendDlgItemMessageW(hDlg, 109, BM_GETCHECK, 0, 0) == BST_CHECKED);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (id == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    case WM_HSCROLL:
    {
        HWND hCtrl = (HWND)lParam;
        int pos = (int)SendMessageW(hCtrl, TBM_GETPOS, 0, 0);
        wchar_t label[32];

        if (hCtrl == GetDlgItem(hDlg, 102))
        { // Background opacity
            data->alpha = (BYTE)pos;
            data->colorChanged = true;
            swprintf_s(label, L"%d%%", (pos * 100) / 255);
            SetDlgItemTextW(hDlg, 103, label);

            BYTE r = GetRValue(data->color);
            BYTE g = GetGValue(data->color);
            BYTE b = GetBValue(data->color);
            char hexBuf[16];
            sprintf_s(hexBuf, "#%02X%02X%02X%02X", data->alpha, r, g, b);
            *data->colorHex = hexBuf;
            AppearanceUpdateLivePreview(data);
        }
        else if (hCtrl == GetDlgItem(hDlg, 122))
        { // Border opacity
            data->borderOpacity = pos;
            data->borderOpacityChanged = true;
            swprintf_s(label, L"%d%%", (pos * 100) / 255);
            SetDlgItemTextW(hDlg, 123, label);

            if (data->corralConfig)
            {
                data->corralConfig->BorderOpacity = pos;
                if (data->corralWindow)
                    data->corralWindow->SetCurrentBorderOpacity(pos);
                AppearanceUpdateLivePreview(data);
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 124))
        { // Header opacity
            data->headerOpacity = pos;
            data->headerOpacityChanged = true;
            swprintf_s(label, L"%d%%", (pos * 100) / 255);
            SetDlgItemTextW(hDlg, 125, label);

            if (data->corralConfig)
            {
                data->corralConfig->HeaderOpacity = pos;
                if (data->corralWindow)
                    data->corralWindow->SetCurrentHeaderOpacity(pos);
                AppearanceUpdateLivePreview(data);
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 126))
        { // Header label opacity (per-tab, like the other font settings)
            data->fontOpacity = pos;
            data->fontOpacityChanged = true;
            swprintf_s(label, L"%d%%", (pos * 100) / 255);
            SetDlgItemTextW(hDlg, 127, label);

            if (data->activeTabConfig)
            {
                data->activeTabConfig->HeaderFontOpacity = pos;
                if (data->corralWindow)
                    data->corralWindow->SetCurrentTextHover(0); // preview the slider, not the hover fade
                AppearanceUpdateLivePreview(data);
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 116))
        { // Icon opacity
            data->iconOpacity = pos;
            data->iconOpacityChanged = true;
            swprintf_s(label, L"%d%%", (pos * 100) / 255);
            SetDlgItemTextW(hDlg, 117, label);

            if (data->corralConfig)
            {
                data->corralConfig->IconOpacity = pos;
                // Update currentOpacity so SourceConstantAlpha reflects the change
                if (data->corralWindow)
                {
                    data->corralWindow->SetCurrentOpacity(pos);
                }
                AppearanceUpdateLivePreview(data);
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 128))
        { // Icon label opacity
            data->labelOpacity = pos;
            data->labelOpacityChanged = true;
            swprintf_s(label, L"%d%%", (pos * 100) / 255);
            SetDlgItemTextW(hDlg, 129, label);

            if (data->corralConfig)
            {
                data->corralConfig->IconLabelOpacity = pos;
                if (data->corralWindow)
                    data->corralWindow->SetCurrentTextHover(0); // preview the slider, not the hover fade
                AppearanceUpdateLivePreview(data);
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 110))
        { // Header height
            data->titleBarHeight = pos;
            data->titleBarHeightChanged = true;
            swprintf_s(label, L"%dpx", pos);
            SetDlgItemTextW(hDlg, 111, label);

            if (data->corralConfig)
            {
                data->corralConfig->TitleBarHeight = pos;
                AppearanceUpdateLivePreview(data, true); // relayout needed
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 120))
        { // Tint strength
            data->tintStrength = pos;
            data->tintChanged = true;
            swprintf_s(label, L"%d%%", (pos * 100) / 255);
            SetDlgItemTextW(hDlg, 121, label);

            if (data->corralConfig)
            {
                data->corralConfig->IconTintStrength = pos;
                if (data->corralWindow)
                {
                    data->corralWindow->SetCurrentTintStrength(pos);
                }
                AppearanceUpdateLivePreview(data);
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 130))
        { // Horizontal spacing
            data->iconSpacingX = pos;
            data->iconSpacingXChanged = true;
            swprintf_s(label, L"%d%%", pos);
            SetDlgItemTextW(hDlg, 131, label);

            if (data->corralConfig)
            {
                data->corralConfig->IconSpacingXPercent = pos;
                AppearanceUpdateLivePreview(data, true); // relayout needed
            }
        }
        else if (hCtrl == GetDlgItem(hDlg, 132))
        { // Vertical spacing
            data->iconSpacingY = pos;
            data->iconSpacingYChanged = true;
            swprintf_s(label, L"%d%%", pos);
            SetDlgItemTextW(hDlg, 133, label);

            if (data->corralConfig)
            {
                data->corralConfig->IconSpacingYPercent = pos;
                AppearanceUpdateLivePreview(data, true); // relayout needed
            }
        }
        return TRUE;
    }
    case WM_DESTROY:
        if (data)
        {
            if (data->hBrush)
            {
                DeleteObject(data->hBrush);
                data->hBrush = nullptr;
            }
            if (data->fontColorBrush)
            {
                DeleteObject(data->fontColorBrush);
                data->fontColorBrush = nullptr;
            }
            if (data->tintColorBrush)
            {
                DeleteObject(data->tintColorBrush);
                data->tintColorBrush = nullptr;
            }
        }
        return TRUE;
    }
    INT_PTR skinned;
    if (DialogSkin::HandleMessage(hDlg, msg, wParam, lParam, skinned))
        return skinned;
    return FALSE;
}

// Helper to add a trackbar (custom class name)
static WORD *AddTrackbar(WORD *p, DWORD style, short x, short y, short cx, short cy, WORD id)
{
    if ((ULONG_PTR)p % 4)
        p++;

    DLGITEMTEMPLATE *item = (DLGITEMTEMPLATE *)p;
    item->style = style;
    item->x = x;
    item->y = y;
    item->cx = cx;
    item->cy = cy;
    item->id = id;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    const wchar_t *cls = L"msctls_trackbar32";
    size_t len = wcslen(cls) + 1;
    memcpy(p, cls, len * sizeof(wchar_t));
    p += len;
    *p++ = 0; // no text
    *p++ = 0; // no creation data
    return p;
}

void CorralWindow::ShowAppearanceDialog()
{
    // Build dynamic title
    std::wstring wTitle = Utf8ToWide(GetActiveTab().Title);
    std::wstring dlgTitleStr = TrFmt(Str::Title_Appearance, wTitle);

    // Three pages share the dialog: the controls of the pages that are not
    // shown are hidden (see AppearanceShowPage). Layout in dialog units.
    WORD dlgTemplate[2 * DIALOG_TEMPLATE_BUFFER_SIZE / sizeof(WORD)] = {};
    WORD *p = BeginDlgTemplate(dlgTemplate, DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT,
                               0, APPEARANCE_DLG_W, APPEARANCE_FOOTER_TOP + 28, dlgTitleStr.c_str());
    WORD itemCount = 0;

    // WS_GROUP on every label: the arrow keys stay within the page strip and within each row
    const DWORD labelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | WS_GROUP;
    const DWORD buttonStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    const DWORD checkStyle = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
    const DWORD swatchStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY;
    const short LABEL_X = 14, LABEL_W = 72, FIELD_X = 90, SLIDER_W = 114, VALUE_X = 208, VALUE_W = 28;
    const short ROW = 18;
    auto rowY = [&](int row) { return (short)(APPEARANCE_PAGE_TOP + row * ROW); };
    auto text = [&](short x, short y, short w, WORD id, const wchar_t *caption, DWORD style)
    {
        p = AddDlgItem(p, style, x, y, w, 10, id, 0xFFFF, DLG_CLASS_STATIC, caption);
        itemCount++;
    };
    auto button = [&](short x, short y, short w, WORD id, const wchar_t *caption, DWORD style)
    {
        p = AddDlgItem(p, style, x, y, w, 14, id, 0xFFFF, DLG_CLASS_BUTTON, caption);
        itemCount++;
    };
    auto swatch = [&](int row, WORD id)
    {
        p = AddDlgItem(p, swatchStyle, FIELD_X, (short)(rowY(row) - 1), 26, 12, id, 0xFFFF, DLG_CLASS_STATIC, nullptr);
        itemCount++;
    };
    // One slider row: label, slider, value
    auto slider = [&](int row, WORD labelId, const wchar_t *caption, WORD sliderId, WORD valueId)
    {
        text(LABEL_X, rowY(row), LABEL_W, labelId, caption, labelStyle);
        p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, FIELD_X, (short)(rowY(row) - 3),
                        SLIDER_W, 15, sliderId);
        itemCount++;
        text(VALUE_X, rowY(row), VALUE_W, valueId, L"", WS_CHILD | WS_VISIBLE | SS_RIGHT);
    };

    // Preset row
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 10, APPEARANCE_PRESET_TOP, 118,
                   160, IDC_APPEARANCE_PRESET, 0xFFFF, DLG_CLASS_COMBOBOX, nullptr);
    itemCount++;
    button(132, (short)(APPEARANCE_PRESET_TOP - 1), 56, IDC_APPEARANCE_PRESET_SAVE, Tr(Str::Btn_SavePreset), buttonStyle);
    button(192, (short)(APPEARANCE_PRESET_TOP - 1), 44, IDC_APPEARANCE_PRESET_DELETE, Tr(Str::Btn_DeletePreset), buttonStyle);

    // Page strip
    button(10, APPEARANCE_STRIP_TOP, 62, IDC_APPEARANCE_PAGE, Tr(Str::Lbl_Background), buttonStyle | WS_GROUP);
    button(74, APPEARANCE_STRIP_TOP, 62, IDC_APPEARANCE_PAGE + 1, Tr(Str::Grp_Header), buttonStyle);
    button(138, APPEARANCE_STRIP_TOP, 62, IDC_APPEARANCE_PAGE + 2, Tr(Str::Grp_Icons), buttonStyle);

    // Page 1: background. Colour and accent are per tab, the two opacities per Corral.
    text(LABEL_X, rowY(0), LABEL_W, 150, Tr(Str::Lbl_Color), labelStyle);
    swatch(0, 105);
    button(FIELD_X + 32, (short)(rowY(0) - 2), 58, 106, Tr(Str::Btn_Change), buttonStyle);
    button(FIELD_X, (short)(rowY(1) - 2), 146, 135, Tr(Str::Chk_AccentColor), checkStyle);
    slider(2, 151, Tr(Str::Grp_Opacity), 102, 103);
    slider(3, 152, Tr(Str::Lbl_Border), 122, 123);

    // Page 2: header. Height and opacity are per Corral, the font settings per tab.
    slider(0, 153, Tr(Str::Lbl_Height), 110, 111);
    slider(1, 154, Tr(Str::Grp_Opacity), 124, 125);
    text(LABEL_X, rowY(2), LABEL_W, 155, Tr(Str::Lbl_Font), labelStyle);
    text(FIELD_X, rowY(2), 84, 112, L"", labelStyle | SS_ENDELLIPSIS);
    button(FIELD_X + 88, (short)(rowY(2) - 2), 58, 113, Tr(Str::Btn_Choose), buttonStyle);
    text(LABEL_X, rowY(3), LABEL_W, 156, Tr(Str::Lbl_Color), labelStyle);
    swatch(3, 114);
    button(FIELD_X + 32, (short)(rowY(3) - 2), 58, 115, Tr(Str::Btn_Change), buttonStyle);
    slider(4, 157, Tr(Str::Lbl_HeaderLabel), 126, 127);

    // Page 3: icons, all per Corral
    slider(0, 158, Tr(Str::Grp_Opacity), 116, 117);
    slider(1, 159, Tr(Str::Lbl_IconLabel), 128, 129);
    text(LABEL_X, rowY(2), LABEL_W, 160, Tr(Str::Lbl_Tint), labelStyle);
    swatch(2, 118);
    button(FIELD_X + 32, (short)(rowY(2) - 2), 58, 119, Tr(Str::Btn_Change), buttonStyle);
    slider(3, 161, Tr(Str::Lbl_TintStrength), 120, 121);
    slider(4, 162, Tr(Str::Lbl_Horiz), 130, 131);
    slider(5, 163, Tr(Str::Lbl_Vert), 132, 133);

    // On every page: what OK does beyond this Corral
    const short CHECK_TOP = rowY(6) + 2;
    button(LABEL_X, CHECK_TOP, 222, 107, Tr(Str::Chk_UseAsDefault), checkStyle | WS_GROUP);
    button(LABEL_X, (short)(CHECK_TOP + 14), 222, 108, Tr(Str::Chk_ApplyToAll), checkStyle);
    button(LABEL_X, (short)(CHECK_TOP + 28), 222, 109, Tr(Str::Chk_CopyStyleToAll), checkStyle);

    // Footer: Reset to Defaults set apart on the left
    const short BTN_Y = APPEARANCE_FOOTER_TOP + 7;
    button(10, BTN_Y, 104, IDC_APPEARANCE_RESET, Tr(Str::Btn_ResetToDefaults), buttonStyle | WS_GROUP);
    button(APPEARANCE_DLG_W - 10 - 54 - 6 - 54, BTN_Y, 54, IDOK, Tr(Str::Btn_OK), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP);
    button(APPEARANCE_DLG_W - 10 - 54, BTN_Y, 54, IDCANCEL, Tr(Str::Btn_Cancel), buttonStyle);

    ((DLGTEMPLATE *)dlgTemplate)->cdit = itemCount;

    // Data prep
    AppearanceDlgData dlgData = {};
    dlgData.alpha = 153;
    dlgData.color = RGB(0, 0, 0);
    dlgData.previewWindow = hwnd;
    dlgData.colorHex = &GetActiveTab().ColorHex;
    dlgData.corralConfig = &config;
    dlgData.activeTabConfig = &GetActiveTab();
    dlgData.corralWindow = this;

    // Parse background color
    const std::string &colorHexRef = GetActiveTab().ColorHex;
    if (!colorHexRef.empty() && colorHexRef[0] == '#' && colorHexRef.length() >= 9)
    {
        unsigned int colorValue;
        sscanf_s(colorHexRef.c_str() + 1, "%x", &colorValue);
        dlgData.alpha = (colorValue >> 24) & 0xFF;
        BYTE r = (colorValue >> 16) & 0xFF;
        BYTE g = (colorValue >> 8) & 0xFF;
        BYTE b = colorValue & 0xFF;
        dlgData.color = RGB(r, g, b);
    }

    dlgData.borderOpacity = ChromeAlpha::ClampBorderOpacity(config.BorderOpacity);

    // Header settings — height and opacity are per-corral, font is per-tab (active tab)
    dlgData.titleBarHeight = config.TitleBarHeight;
    dlgData.headerOpacity = ChromeAlpha::ClampHeaderOpacity(config.HeaderOpacity);
    dlgData.fontName = Utf8ToWide(GetActiveTab().HeaderFontName);
    dlgData.fontSize = GetActiveTab().HeaderFontSize;

    // Parse font color from active tab
    dlgData.fontColor = RGB(255, 255, 255);
    const std::string &fcHex = GetActiveTab().HeaderFontColor;
    if (!fcHex.empty() && fcHex[0] == '#' && fcHex.length() >= 7)
    {
        unsigned int fcVal;
        sscanf_s(fcHex.c_str() + 1, "%x", &fcVal);
        dlgData.fontColor = RGB((fcVal >> 16) & 0xFF, (fcVal >> 8) & 0xFF, fcVal & 0xFF);
    }

    dlgData.fontOpacity = ChromeAlpha::ClampTextOpacity(GetActiveTab().HeaderFontOpacity);
    dlgData.iconOpacity = config.IconOpacity;
    dlgData.labelOpacity = ChromeAlpha::ClampTextOpacity(config.IconLabelOpacity);

    // Parse tint color
    dlgData.tintColor = RGB(0, 0, 0);
    const std::string &tintHex = config.IconTintColor;
    if (!tintHex.empty() && tintHex[0] == '#' && tintHex.length() >= 7)
    {
        unsigned int tintVal;
        sscanf_s(tintHex.c_str() + 1, "%x", &tintVal);
        dlgData.tintColor = RGB((tintVal >> 16) & 0xFF, (tintVal >> 8) & 0xFF, tintVal & 0xFF);
    }
    dlgData.tintStrength = config.IconTintStrength;
    dlgData.iconSpacingX = config.IconSpacingXPercent;
    dlgData.iconSpacingY = config.IconSpacingYPercent;

    // Save originals for cancel
    std::string originalColor = GetActiveTab().ColorHex;
    bool originalUseAccent = GetActiveTab().UseAccentColor;
    int originalTitleBarHeight = config.TitleBarHeight;
    int originalHeaderOpacity = config.HeaderOpacity;
    int originalBorderOpacity = config.BorderOpacity;
    std::string originalFontName = GetActiveTab().HeaderFontName;
    int originalFontSize = GetActiveTab().HeaderFontSize;
    std::string originalFontColor = GetActiveTab().HeaderFontColor;
    int originalFontOpacity = GetActiveTab().HeaderFontOpacity;
    int originalIconOpacity = config.IconOpacity;
    int originalLabelOpacity = config.IconLabelOpacity;
    std::string originalTintColor = config.IconTintColor;
    int originalTintStrength = config.IconTintStrength;
    int originalSpacingX = config.IconSpacingXPercent;
    int originalSpacingY = config.IconSpacingYPercent;

    blockRaise++; // Owned by this corral: see MessageBoxBehind
    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE *)dlgTemplate,
        hwnd,
        AppearanceDlgProc,
        (LPARAM)&dlgData);
    blockRaise--;
    PinToBandTop();

    if (result != IDOK)
    {
        // Restore all settings on cancel
        GetActiveTab().ColorHex = originalColor;
        GetActiveTab().UseAccentColor = originalUseAccent;
        config.TitleBarHeight = originalTitleBarHeight;
        config.HeaderOpacity = originalHeaderOpacity;
        config.BorderOpacity = originalBorderOpacity;
        SetCurrentHeaderOpacity(originalHeaderOpacity);
        SetCurrentBorderOpacity(originalBorderOpacity);
        GetActiveTab().HeaderFontName = originalFontName;
        GetActiveTab().HeaderFontSize = originalFontSize;
        GetActiveTab().HeaderFontColor = originalFontColor;
        GetActiveTab().HeaderFontOpacity = originalFontOpacity;
        config.IconOpacity = originalIconOpacity;
        SetCurrentOpacity(originalIconOpacity); // The slider and a loaded preset set it for the preview
        config.IconLabelOpacity = originalLabelOpacity;
        config.IconTintColor = originalTintColor;
        config.IconTintStrength = originalTintStrength;
        currentTintStrength = originalTintStrength;
        config.IconSpacingXPercent = originalSpacingX;
        config.IconSpacingYPercent = originalSpacingY;
        RecalculateLayout(); // Not only CalculateIconLayout: the spacing preview changed the cached pixel spacing
        // A preset saved or deleted in the dialog stays so; the look is back as it was
        if (dlgData.presetsChanged && App::GetInstance())
            App::GetInstance()->SaveConfig();
    }
    else
    {
        // Apply final values — height/opacity to corral, font to active tab only
        config.TitleBarHeight = dlgData.titleBarHeight;
        config.HeaderOpacity = ChromeAlpha::ClampHeaderOpacity(dlgData.headerOpacity);
        config.BorderOpacity = ChromeAlpha::ClampBorderOpacity(dlgData.borderOpacity);
        SetCurrentHeaderOpacity(config.HeaderOpacity);
        SetCurrentBorderOpacity(config.BorderOpacity);
        GetActiveTab().HeaderFontName = WideToUtf8(dlgData.fontName);
        GetActiveTab().HeaderFontSize = dlgData.fontSize;
        char fcBuf[16];
        sprintf_s(fcBuf, "#%02X%02X%02X", GetRValue(dlgData.fontColor), GetGValue(dlgData.fontColor), GetBValue(dlgData.fontColor));
        GetActiveTab().HeaderFontColor = fcBuf;
        GetActiveTab().HeaderFontOpacity = ChromeAlpha::ClampTextOpacity(dlgData.fontOpacity);
        config.IconOpacity = dlgData.iconOpacity;
        config.IconLabelOpacity = ChromeAlpha::ClampTextOpacity(dlgData.labelOpacity);

        char tintBuf[16];
        sprintf_s(tintBuf, "#%02X%02X%02X", GetRValue(dlgData.tintColor), GetGValue(dlgData.tintColor), GetBValue(dlgData.tintColor));
        config.IconTintColor = tintBuf;
        config.IconTintStrength = dlgData.tintStrength;
        currentTintStrength = dlgData.tintStrength;
        config.IconSpacingXPercent = dlgData.iconSpacingX;
        config.IconSpacingYPercent = dlgData.iconSpacingY;
        if (dlgData.presetLoaded)
            Presets::ApplyLook(config, AppearanceCurrentLook(&dlgData)); // The other tabs too

        CalculateIconLayout();
        UpdateLayeredContent();

        App *app = App::GetInstance();
        if (app)
        {
            // One snapshot of the look the user just approved; what happens to
            // it depends on the checkboxes.
            AppearanceSettings look;
            look.ColorHex = GetActiveTab().ColorHex;
            look.UseAccentColor = GetActiveTab().UseAccentColor;
            look.TitleBarHeight = config.TitleBarHeight;
            look.HeaderOpacity = config.HeaderOpacity;
            look.BorderOpacity = config.BorderOpacity;
            look.HeaderFontName = GetActiveTab().HeaderFontName;
            look.HeaderFontSize = GetActiveTab().HeaderFontSize;
            look.HeaderFontColor = GetActiveTab().HeaderFontColor;
            look.HeaderFontOpacity = GetActiveTab().HeaderFontOpacity;
            look.IconOpacity = config.IconOpacity;
            look.IconLabelOpacity = config.IconLabelOpacity;
            look.IconTintColor = config.IconTintColor;
            look.IconTintStrength = config.IconTintStrength;
            look.IconSpacingXPercent = config.IconSpacingXPercent;
            look.IconSpacingYPercent = config.IconSpacingYPercent;

            if (dlgData.useAsDefault)
            {
                app->SetDefaultColorHex(look.ColorHex);
                app->SetDefaultAppearance(look);
            }

            if (dlgData.applyChangesToAll)
            {
                // Apply only the settings the user actually changed
                AppearanceApplyFlags changed;
                changed.Color = dlgData.colorChanged;
                changed.TitleBarHeight = dlgData.titleBarHeightChanged;
                changed.HeaderOpacity = dlgData.headerOpacityChanged;
                changed.BorderOpacity = dlgData.borderOpacityChanged;
                changed.Font = dlgData.fontChanged;
                changed.FontColor = dlgData.fontColorChanged;
                changed.FontOpacity = dlgData.fontOpacityChanged;
                changed.IconOpacity = dlgData.iconOpacityChanged;
                changed.IconLabelOpacity = dlgData.labelOpacityChanged;
                changed.Tint = dlgData.tintChanged;
                changed.Spacing = dlgData.iconSpacingXChanged || dlgData.iconSpacingYChanged;
                app->ApplyAppearanceToAllCorrals(look, changed);
            }
            else if (dlgData.applyEverythingToAll)
            {
                // Copy full appearance to all corrals
                app->ApplyAppearanceToAllCorrals(look, AppearanceApplyFlags::All());
            }

            app->SaveConfig();
        }
    }
}
