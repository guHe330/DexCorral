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
    originalName = icons[iconIndex].displayName;

    // Get the label rect in client coordinates
    RECT labelRect = GetIconLabelRect(iconIndex);

    // Adjust for scroll position
    labelRect.top -= scrollPosition;
    labelRect.bottom -= scrollPosition;

    // Temporarily disable layered window style so child controls render properly
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);

    // Create edit control as a normal child window
    hEditControl = CreateWindowExW(
        0,
        L"EDIT",
        icons[iconIndex].displayName.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_CENTER,
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
        // system icon title font (DPI-aware, SPI_GETICONTITLELOGFONT); a
        // hardcoded -11px font would render much smaller on high-DPI displays.
        LOGFONTW iconLogFont = {};
        if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(iconLogFont), &iconLogFont, 0))
        {
            iconLogFont.lfQuality = CLEARTYPE_QUALITY;
            hEditFont = CreateFontIndirectW(&iconLogFont);
        }
        else
        {
            hEditFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        }
        SendMessageW(hEditControl, WM_SETFONT, (WPARAM)hEditFont, TRUE);

        // Select all text
        SendMessageW(hEditControl, EM_SETSEL, 0, -1);

        // Set focus
        SetFocus(hEditControl);

        // Subclass the edit control to handle Enter/Escape
        SetWindowSubclass(hEditControl, EditSubclassProc, 0, (DWORD_PTR)this);
    }
}

void CorralWindow::EndIconRename(bool save)
{
    if (!isRenamingIcon || !hEditControl)
    {
        return;
    }

    if (save && renamingIconIndex >= 0 && renamingIconIndex < (int)icons.size())
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
                if (lastSlash != std::wstring::npos)
                {
                    newPath = oldPath.substr(0, lastSlash + 1) + newName;

                    // The editable name is the shell display name, which may
                    // omit the extension (.lnk, or any extension when "hide
                    // extensions" is on). Re-append it like Explorer does so
                    // the rename can't silently change the file type.
                    std::wstring oldFileName = oldPath.substr(lastSlash + 1);
                    const std::wstring &shownName = icons[renamingIconIndex].displayName;
                    if (oldFileName.length() > shownName.length() &&
                        _wcsnicmp(oldFileName.c_str(), shownName.c_str(), shownName.length()) == 0 &&
                        oldFileName[shownName.length()] == L'.')
                    {
                        std::wstring hiddenExt = oldFileName.substr(shownName.length());
                        bool newNameHasExt = newName.length() >= hiddenExt.length() &&
                            _wcsicmp(newName.c_str() + newName.length() - hiddenExt.length(), hiddenExt.c_str()) == 0;
                        if (!newNameHasExt)
                        {
                            newPath += hiddenExt;
                        }
                    }
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
                    // Rename failed - could show error message
                    MessageBoxW(hwnd, Tr(Str::Err_RenameFailedBody),
                                Tr(Str::Err_RenameFailedTitle), MB_OK | MB_ICONERROR);
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
    originalName.clear();

    if (editToDestroy)
    {
        DestroyWindow(editToDestroy);
    }

    if (hEditFont)
    {
        DeleteObject(hEditFont);
        hEditFont = nullptr;
    }

    // Restore layered window style (was disabled for child edit control to work)
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
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

static INT_PTR CALLBACK RenameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
        wchar_t *initialText = (wchar_t *)lParam;
        SetDlgItemTextW(hDlg, 101, initialText);

        HWND hEdit = GetDlgItem(hDlg, 101);
        SendMessageW(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);

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
        if (LOWORD(wParam) == IDOK)
        {
            wchar_t buffer[256];
            GetDlgItemTextW(hDlg, 101, buffer, 256);
            wchar_t *result = (wchar_t *)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            wcscpy_s(result, 256, buffer);
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
    return FALSE;
}

void CorralWindow::ShowRenameDialog()
{
    WORD dlgTemplate[512] = {};
    WORD *p = dlgTemplate;

    DLGTEMPLATE *dlg = (DLGTEMPLATE *)p;
    dlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT;
    dlg->cdit = 3;
    dlg->cx = 180;
    dlg->cy = 55;
    p += sizeof(DLGTEMPLATE) / sizeof(WORD);

    *p++ = 0;
    *p++ = 0;

    const wchar_t *dlgTitle = Tr(Str::Dlg_RenameCorral);
    size_t titleLen = wcslen(dlgTitle) + 1;
    memcpy(p, dlgTitle, titleLen * sizeof(wchar_t));
    p += titleLen;

    *p++ = 9;
    const wchar_t *fontName = L"Segoe UI";
    size_t fontLen = wcslen(fontName) + 1;
    memcpy(p, fontName, fontLen * sizeof(wchar_t));
    p += fontLen;

    if ((ULONG_PTR)p % 4)
        p++;

    DLGITEMTEMPLATE *item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    item->x = 8;
    item->y = 8;
    item->cx = 164;
    item->cy = 14;
    item->id = 101;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF;
    *p++ = 0x0081;
    *p++ = 0;
    *p++ = 0;

    if ((ULONG_PTR)p % 4)
        p++;

    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
    item->x = 70;
    item->y = 30;
    item->cx = 50;
    item->cy = 14;
    item->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF;
    *p++ = 0x0080;
    const wchar_t *okText = Tr(Str::Btn_OK);
    size_t okLen = wcslen(okText) + 1;
    memcpy(p, okText, okLen * sizeof(wchar_t));
    p += okLen;
    *p++ = 0;

    if ((ULONG_PTR)p % 4)
        p++;

    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 124;
    item->y = 30;
    item->cx = 50;
    item->cy = 14;
    item->id = IDCANCEL;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF;
    *p++ = 0x0080;
    const wchar_t *cancelText = Tr(Str::Btn_Cancel);
    size_t cancelLen = wcslen(cancelText) + 1;
    memcpy(p, cancelText, cancelLen * sizeof(wchar_t));
    p += cancelLen;
    *p++ = 0;

    wchar_t nameBuffer[256] = {};
    std::wstring currentTitle = Utf8ToWide(GetActiveTab().Title);
    wcsncpy_s(nameBuffer, currentTitle.c_str(), _TRUNCATE);

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE *)dlgTemplate,
        hwnd,
        RenameDlgProc,
        (LPARAM)nameBuffer);
    PinToBandTop();

    if (result == IDOK && wcslen(nameBuffer) > 0)
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
            cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL;

            if (ChooseFontW(&cf))
            {
                data->fontName = lf.lfFaceName;
                data->fontSize = cf.iPointSize / 10; // iPointSize is in 1/10 points
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
    return FALSE;
}

// Helper to add a dialog item to the in-memory template
static WORD *AddDlgItem(WORD *p, DWORD style, short x, short y, short cx, short cy, WORD id,
                        WORD classHi, WORD classLo, const wchar_t *text)
{
    // Align to DWORD boundary
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
    *p++ = 0xFFFF;
    *p++ = classLo;
    if (text)
    {
        size_t len = wcslen(text) + 1;
        memcpy(p, text, len * sizeof(wchar_t));
        p += len;
    }
    else
    {
        *p++ = 0;
    }
    *p++ = 0; // no creation data
    return p;
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

    // Layout Y positions (in dialog units)
    // Background Color group: y=5, h=35
    // Opacity group (every opacity: fill, border, header, header label, icons, icon label): y=45, h=115
    // Header group (height + font face/size + colour): y=165, h=58
    // Icons group (tint): y=228, h=30
    // Icon Spacing group: y=263, h=40
    // Checkboxes: y=309, y=323, y=337
    // Buttons: y=354

    const int DLG_WIDTH = 220;
    const int DLG_HEIGHT = 371;
    const int ITEM_COUNT = 50;

    WORD dlgTemplate[DIALOG_TEMPLATE_BUFFER_SIZE / sizeof(WORD)] = {};
    WORD *p = dlgTemplate;

    DLGTEMPLATE *dlg = (DLGTEMPLATE *)p;
    dlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT;
    dlg->cdit = ITEM_COUNT;
    dlg->cx = DLG_WIDTH;
    dlg->cy = DLG_HEIGHT;
    p += sizeof(DLGTEMPLATE) / sizeof(WORD);

    *p++ = 0;
    *p++ = 0; // menu, class

    size_t len = wcslen(dlgTitleStr.c_str()) + 1;
    memcpy(p, dlgTitleStr.c_str(), len * sizeof(wchar_t));
    p += len;

    *p++ = 9; // font size
    const wchar_t *strFont = L"Segoe UI";
    len = wcslen(strFont) + 1;
    memcpy(p, strFont, len * sizeof(wchar_t));
    p += len;

    // === Background Color group ===
    // 1. Group box
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 5, 5, 210, 35, (WORD)-1, 0xFFFF, 0x0080, Tr(Str::Grp_BackgroundColor));
    // 2. Color swatch (ID 105)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY, 15, 18, 30, 14, 105, 0xFFFF, 0x0082, nullptr);
    // 3. Change button (ID 106)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 55, 18, 60, 14, 106, 0xFFFF, 0x0080, Tr(Str::Btn_Change));

    // === Opacity group — every opacity slider lives here, each label directly
    // under the thing it belongs to. All are per-corral except "Header Label",
    // which is per-tab like the other header font settings.
    // 4. Group box
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 5, 45, 210, 115, (WORD)-1, 0xFFFF, 0x0080, Tr(Str::Grp_Opacity));
    // 5. "Background" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 58, 52, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Background));
    // 6. Background opacity slider (ID 102)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 70, 56, 105, 15, 102);
    // 7. Background opacity label (ID 103)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 58, 25, 12, 103, 0xFFFF, 0x0082, L"100%");
    // 8. "Border" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 75, 52, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Border));
    // 9. Border opacity slider (ID 122)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 70, 73, 105, 15, 122);
    // 10. Border opacity label (ID 123)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 75, 25, 12, 123, 0xFFFF, 0x0082, L"100%");
    // 11. "Header" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 92, 52, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Grp_Header));
    // 12. Header opacity slider (ID 124)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 70, 90, 105, 15, 124);
    // 13. Header opacity label (ID 125)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 92, 25, 12, 125, 0xFFFF, 0x0082, L"94%");
    // 14. "Header Label" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 109, 52, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_HeaderLabel));
    // 15. Header text opacity slider (ID 126)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 70, 107, 105, 15, 126);
    // 16. Header text opacity label (ID 127)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 109, 25, 12, 127, 0xFFFF, 0x0082, L"100%");
    // 17. "Icons" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 126, 52, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Grp_Icons));
    // 18. Icon opacity slider (ID 116)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 70, 124, 105, 15, 116);
    // 19. Icon opacity label (ID 117)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 126, 25, 12, 117, 0xFFFF, 0x0082, L"100%");
    // 20. "Icon Label" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 143, 52, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_IconLabel));
    // 21. Icon label opacity slider (ID 128)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 70, 141, 105, 15, 128);
    // 22. Icon label opacity label (ID 129)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 143, 25, 12, 129, 0xFFFF, 0x0082, L"100%");

    // === Header group (height is per-corral; font face/size/colour are per-tab) ===
    // 23. Group box
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 5, 165, 210, 58, (WORD)-1, 0xFFFF, 0x0080, Tr(Str::Grp_Header));
    // 24. "Height" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 178, 28, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Height));
    // 25. Height slider (ID 110)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 45, 176, 130, 15, 110);
    // 26. Height label (ID 111)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 178, 25, 10, 111, 0xFFFF, 0x0082, L"32px");
    // 27. "Font" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 195, 22, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Font));
    // 28. Font name display (ID 112)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SUNKEN, 45, 194, 100, 12, 112, 0xFFFF, 0x0082, L"Segoe UI");
    // 29. Choose Font button (ID 113)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 150, 193, 55, 14, 113, 0xFFFF, 0x0080, Tr(Str::Btn_Choose));
    // 30. "Color" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 212, 24, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Color));
    // 31. Font color swatch (ID 114)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY, 45, 210, 30, 14, 114, 0xFFFF, 0x0082, nullptr);
    // 32. Font color Change button (ID 115)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 85, 210, 60, 14, 115, 0xFFFF, 0x0080, Tr(Str::Btn_Change));

    // === Icons group (tint; every icon opacity lives in the Opacity group) ===
    // 33. Group box
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 5, 228, 210, 30, (WORD)-1, 0xFFFF, 0x0080, Tr(Str::Grp_Icons));
    // 34. "Tint" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 241, 16, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Tint));
    // 35. Tint color swatch (ID 118)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY, 35, 239, 20, 14, 118, 0xFFFF, 0x0082, nullptr);
    // 36. Tint color Change button (ID 119)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 60, 239, 40, 14, 119, 0xFFFF, 0x0080, Tr(Str::Btn_Color));
    // 37. Tint strength slider (ID 120)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 105, 239, 70, 15, 120);
    // 38. Tint strength label (ID 121)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 241, 25, 12, 121, 0xFFFF, 0x0082, L"0%");

    // === Icon Spacing group ===
    // 39. Group box
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 5, 263, 210, 40, (WORD)-1, 0xFFFF, 0x0080, Tr(Str::Grp_IconSpacing));
    // 40. "Horiz" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 276, 22, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Horiz));
    // 41. Horizontal spacing slider (ID 130)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 40, 274, 135, 15, 130);
    // 42. Horizontal spacing label (ID 131)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 276, 25, 10, 131, 0xFFFF, 0x0082, L"100%");
    // 43. "Vert" label
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_LEFT, 15, 290, 22, 10, (WORD)-1, 0xFFFF, 0x0082, Tr(Str::Lbl_Vert));
    // 44. Vertical spacing slider (ID 132)
    p = AddTrackbar(p, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 40, 288, 135, 15, 132);
    // 45. Vertical spacing label (ID 133)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | SS_RIGHT, 180, 290, 25, 10, 133, 0xFFFF, 0x0082, L"100%");

    // === Checkboxes ===
    // 46. Checkbox "Use as default" (ID 107)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 10, 309, 200, 12, 107, 0xFFFF, 0x0080, Tr(Str::Chk_UseAsDefault));
    // 47. Checkbox "Apply changed settings to all corrals" (ID 108)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 10, 323, 200, 12, 108, 0xFFFF, 0x0080, Tr(Str::Chk_ApplyToAll));
    // 48. Checkbox "Apply full appearance to all corrals" (ID 109)
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 10, 337, 200, 12, 109, 0xFFFF, 0x0080, Tr(Str::Chk_CopyStyleToAll));

    // === Buttons ===
    // 49-50.
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 110, 354, 50, 14, IDOK, 0xFFFF, 0x0080, Tr(Str::Btn_OK));
    p = AddDlgItem(p, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 165, 354, 50, 14, IDCANCEL, 0xFFFF, 0x0080, Tr(Str::Btn_Cancel));

    // Fix item count in the template header
    ((DLGTEMPLATE *)dlgTemplate)->cdit = ITEM_COUNT;

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

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE *)dlgTemplate,
        hwnd,
        AppearanceDlgProc,
        (LPARAM)&dlgData);
    PinToBandTop();

    if (result != IDOK)
    {
        // Restore all settings on cancel
        GetActiveTab().ColorHex = originalColor;
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
        config.IconLabelOpacity = originalLabelOpacity;
        config.IconTintColor = originalTintColor;
        config.IconTintStrength = originalTintStrength;
        currentTintStrength = originalTintStrength;
        config.IconSpacingXPercent = originalSpacingX;
        config.IconSpacingYPercent = originalSpacingY;
        CalculateIconLayout();
        UpdateLayeredContent();
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

        CalculateIconLayout();
        UpdateLayeredContent();

        App *app = App::GetInstance();
        if (app)
        {
            // One snapshot of the look the user just approved; what happens to
            // it depends on the checkboxes.
            AppearanceSettings look;
            look.ColorHex = GetActiveTab().ColorHex;
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
