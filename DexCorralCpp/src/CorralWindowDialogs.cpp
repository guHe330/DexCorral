// CorralWindowDialogs.cpp - Dialog boxes and inline icon rename
#include "CorralWindow.h"
#include "App.h"
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <cstdio>

// ============================================================================
// Icon rename support (inline edit control)
// ============================================================================

void CorralWindow::StartIconRename(int iconIndex) {
    if (iconIndex < 0 || iconIndex >= (int)icons.size() || isRenamingIcon) {
        return;
    }

    // Special shell icons cannot be renamed
    if (icons[iconIndex].isSpecialIcon) return;

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
        nullptr
    );

    if (hEditControl) {
        // Set font to match the label
        HFONT hFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SendMessageW(hEditControl, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Select all text
        SendMessageW(hEditControl, EM_SETSEL, 0, -1);

        // Set focus
        SetFocus(hEditControl);

        // Subclass the edit control to handle Enter/Escape
        SetWindowSubclass(hEditControl, EditSubclassProc, 0, (DWORD_PTR)this);
    }
}

void CorralWindow::EndIconRename(bool save) {
    if (!isRenamingIcon || !hEditControl) {
        return;
    }

    if (save && renamingIconIndex >= 0 && renamingIconIndex < (int)icons.size()) {
        // Get the new name from edit control
        int len = GetWindowTextLengthW(hEditControl);
        if (len > 0) {
            std::wstring newName(len + 1, L'\0');
            GetWindowTextW(hEditControl, &newName[0], len + 1);
            newName.resize(len);

            // Trim whitespace
            size_t start = newName.find_first_not_of(L" \t\r\n");
            size_t end = newName.find_last_not_of(L" \t\r\n");
            if (start != std::wstring::npos && end != std::wstring::npos) {
                newName = newName.substr(start, end - start + 1);
            }

            // Only rename if name actually changed and is not empty
            if (!newName.empty() && newName != originalName) {
                // Get the original file path
                std::wstring oldPath = icons[renamingIconIndex].fullPath;

                // Build new path
                size_t lastSlash = oldPath.find_last_of(L"\\");
                std::wstring newPath;
                if (lastSlash != std::wstring::npos) {
                    newPath = oldPath.substr(0, lastSlash + 1) + newName;

                    // Add .lnk extension if it's a shortcut and not already present
                    bool oldPathIsLnk = oldPath.length() >= 4 && oldPath.substr(oldPath.length() - 4) == L".lnk";
                    bool newNameIsLnk = newName.length() >= 4 && newName.substr(newName.length() - 4) == L".lnk";
                    if (oldPathIsLnk && !newNameIsLnk) {
                        newPath += L".lnk";
                    }
                }

                // Attempt to rename the file
                if (!newPath.empty() && MoveFileW(oldPath.c_str(), newPath.c_str())) {
                    // Update the icon data
                    icons[renamingIconIndex].fullPath = newPath;
                    icons[renamingIconIndex].wFileName = newPath.substr(lastSlash + 1);

                    // Update display name (without .lnk)
                    std::wstring displayName = icons[renamingIconIndex].wFileName;
                    bool displayNameIsLnk = displayName.length() >= 4 && displayName.substr(displayName.length() - 4) == L".lnk";
                    if (displayNameIsLnk) {
                        displayName = displayName.substr(0, displayName.length() - 4);
                    }
                    icons[renamingIconIndex].displayName = displayName;

                    // Update config
                    if (renamingIconIndex < (int)GetActiveTab().Files.size()) {
                        GetActiveTab().Files[renamingIconIndex] = WideToUtf8(icons[renamingIconIndex].wFileName);
                    }

                    if (App::GetInstance()) {
                        App::GetInstance()->SaveConfig();
                    }
                } else {
                    // Rename failed - could show error message
                    MessageBoxW(hwnd, L"Failed to rename file. The file may be in use or you may not have permission.",
                               L"Rename Error", MB_OK | MB_ICONERROR);
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

    if (editToDestroy) {
        DestroyWindow(editToDestroy);
    }

    // Restore layered window style (was disabled for child edit control to work)
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

    // Redraw to show the updated label
    UpdateLayeredContent();
}

LRESULT CALLBACK CorralWindow::EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    CorralWindow* window = (CorralWindow*)dwRefData;

    switch (uMsg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            // Enter - save and end rename
            window->EndIconRename(true);
            return 0;
        } else if (wParam == VK_ESCAPE) {
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

static INT_PTR CALLBACK RenameDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
        wchar_t* initialText = (wchar_t*)lParam;
        SetDlgItemTextW(hDlg, 101, initialText);

        HWND hEdit = GetDlgItem(hDlg, 101);
        SendMessageW(hEdit, EM_SETSEL, 0, -1);
        SetFocus(hEdit);

        HWND hParent = GetParent(hDlg);
        if (hParent) {
            RECT parentRect, dlgRect;
            GetWindowRect(hParent, &parentRect);
            GetWindowRect(hDlg, &dlgRect);
            int dlgWidth = dlgRect.right - dlgRect.left;
            int dlgHeight = dlgRect.bottom - dlgRect.top;

            // Get monitor work area
            HMONITOR hMon = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hMon, &mi);

            int cx = parentRect.left + (parentRect.right - parentRect.left - dlgWidth) / 2;
            int cy = parentRect.top + (parentRect.bottom - parentRect.top - dlgHeight) / 2;

            // Clamp to monitor bounds
            if (cx + dlgWidth > mi.rcWork.right) cx = mi.rcWork.right - dlgWidth;
            if (cx < mi.rcWork.left) cx = mi.rcWork.left;
            if (cy + dlgHeight > mi.rcWork.bottom) cy = mi.rcWork.bottom - dlgHeight;
            if (cy < mi.rcWork.top) cy = mi.rcWork.top;

            SetWindowPos(hDlg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t buffer[256];
            GetDlgItemTextW(hDlg, 101, buffer, 256);
            wchar_t* result = (wchar_t*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
            wcscpy_s(result, 256, buffer);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
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

void CorralWindow::ShowRenameDialog() {
    WORD dlgTemplate[512] = {};
    WORD* p = dlgTemplate;

    DLGTEMPLATE* dlg = (DLGTEMPLATE*)p;
    dlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT;
    dlg->cdit = 3;
    dlg->cx = 180;
    dlg->cy = 55;
    p += sizeof(DLGTEMPLATE) / sizeof(WORD);

    *p++ = 0;
    *p++ = 0;

    const wchar_t* dlgTitle = L"Rename Corral";
    size_t titleLen = wcslen(dlgTitle) + 1;
    memcpy(p, dlgTitle, titleLen * sizeof(wchar_t));
    p += titleLen;

    *p++ = 9;
    const wchar_t* fontName = L"Segoe UI";
    size_t fontLen = wcslen(fontName) + 1;
    memcpy(p, fontName, fontLen * sizeof(wchar_t));
    p += fontLen;

    if ((ULONG_PTR)p % 4) p++;

    DLGITEMTEMPLATE* item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    item->x = 8; item->y = 8; item->cx = 164; item->cy = 14;
    item->id = 101;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0081;
    *p++ = 0; *p++ = 0;

    if ((ULONG_PTR)p % 4) p++;

    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
    item->x = 70; item->y = 30; item->cx = 50; item->cy = 14;
    item->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    const wchar_t* okText = L"OK";
    memcpy(p, okText, 3 * sizeof(wchar_t));
    p += 3; *p++ = 0;

    if ((ULONG_PTR)p % 4) p++;

    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 124; item->y = 30; item->cx = 50; item->cy = 14;
    item->id = IDCANCEL;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    const wchar_t* cancelText = L"Cancel";
    memcpy(p, cancelText, 7 * sizeof(wchar_t));
    p += 7; *p++ = 0;

    wchar_t nameBuffer[256] = {};
    std::wstring currentTitle = Utf8ToWide(GetActiveTab().Title);
    wcsncpy_s(nameBuffer, currentTitle.c_str(), _TRUNCATE);

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE*)dlgTemplate,
        hwnd,
        RenameDlgProc,
        (LPARAM)nameBuffer
    );

    if (result == IDOK && wcslen(nameBuffer) > 0) {
        int sz = WideCharToMultiByte(CP_UTF8, 0, nameBuffer, -1, nullptr, 0, nullptr, nullptr);
        std::string newTitle(sz - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, nameBuffer, -1, &newTitle[0], sz, nullptr, nullptr);

        GetActiveTab().Title = newTitle;
        SetWindowTextW(hwnd, nameBuffer);
        UpdateLayeredContent();

        if (App::GetInstance()) {
            App::GetInstance()->SaveConfig();
        }
    }
}

// ============================================================================
// Appearance dialog
// ============================================================================

struct AppearanceDlgData {
    BYTE alpha;
    COLORREF color;
    HBRUSH hBrush;
    HWND previewWindow;
    std::string* colorHex;
    bool useAsDefault;
    bool applyToAll;
};

static INT_PTR CALLBACK AppearanceDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppearanceDlgData* data = (AppearanceDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_INITDIALOG: {
        data = (AppearanceDlgData*)lParam;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        // Create brush for initial color
        data->hBrush = CreateSolidBrush(data->color);

        // Setup slider
        HWND hSlider = GetDlgItem(hDlg, 102);
        SendMessageW(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessageW(hSlider, TBM_SETPOS, TRUE, data->alpha);

        // Update label
        wchar_t label[32];
        int pct = (data->alpha * 100) / 255;
        swprintf_s(label, L"%d%%", pct);
        SetDlgItemTextW(hDlg, 103, label);

        // Position below parent corral for live preview visibility
        HWND hParent = GetParent(hDlg);
        if (hParent) {
            RECT parentRect, dlgRect;
            GetWindowRect(hParent, &parentRect);
            GetWindowRect(hDlg, &dlgRect);
            int dlgWidth = dlgRect.right - dlgRect.left;
            int dlgHeight = dlgRect.bottom - dlgRect.top;
            int parentWidth = parentRect.right - parentRect.left;

            // Get monitor work area
            HMONITOR hMon = MonitorFromWindow(hParent, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hMon, &mi);

            // Center horizontally, position below the corral window
            int cx = parentRect.left + (parentWidth - dlgWidth) / 2;
            int cy = parentRect.bottom + 5;  // 5px below the corral

            // If dialog would go below screen, try above the corral
            if (cy + dlgHeight > mi.rcWork.bottom) {
                cy = parentRect.top - dlgHeight - 5;  // 5px above the corral
            }

            // If still outside (top), clamp to top of work area
            if (cy < mi.rcWork.top) {
                cy = mi.rcWork.top;
            }

            // Clamp horizontal position to stay within monitor bounds
            if (cx + dlgWidth > mi.rcWork.right) {
                cx = mi.rcWork.right - dlgWidth;
            }
            if (cx < mi.rcWork.left) {
                cx = mi.rcWork.left;
            }

            SetWindowPos(hDlg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        if (GetDlgCtrlID((HWND)lParam) == 105) { // Color swatch
            return (INT_PTR)data->hBrush;
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 106) { // Change Color button
            static COLORREF customColors[16] = {};
            CHOOSECOLORW cc = {};
            cc.lStructSize = sizeof(CHOOSECOLORW);
            cc.hwndOwner = hDlg;
            cc.lpCustColors = customColors;
            cc.rgbResult = data->color;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;

            if (ChooseColorW(&cc)) {
                data->color = cc.rgbResult;

                // Update brush
                if (data->hBrush) DeleteObject(data->hBrush);
                data->hBrush = CreateSolidBrush(data->color);

                // Force redraw of swatch
                InvalidateRect(GetDlgItem(hDlg, 105), nullptr, TRUE);

                // Update live preview
                BYTE r = GetRValue(data->color);
                BYTE g = GetGValue(data->color);
                BYTE b = GetBValue(data->color);
                char hexBuf[16];
                sprintf_s(hexBuf, "#%02X%02X%02X%02X", data->alpha, r, g, b);
                *data->colorHex = hexBuf;
                if (data->previewWindow) InvalidateRect(data->previewWindow, nullptr, FALSE);
            }
            return TRUE;
        }
        if (id == IDOK) {
            // Read checkbox states before closing
            data->useAsDefault = (SendDlgItemMessageW(hDlg, 107, BM_GETCHECK, 0, 0) == BST_CHECKED);
            data->applyToAll = (SendDlgItemMessageW(hDlg, 108, BM_GETCHECK, 0, 0) == BST_CHECKED);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (id == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    case WM_HSCROLL: {
        if ((HWND)lParam == GetDlgItem(hDlg, 102)) {
            data->alpha = (BYTE)SendMessageW((HWND)lParam, TBM_GETPOS, 0, 0);

            wchar_t label[32];
            int pct = (data->alpha * 100) / 255;
            swprintf_s(label, L"%d%%", pct);
            SetDlgItemTextW(hDlg, 103, label);

            // Live preview
            BYTE r = GetRValue(data->color);
            BYTE g = GetGValue(data->color);
            BYTE b = GetBValue(data->color);
            char hexBuf[16];
            sprintf_s(hexBuf, "#%02X%02X%02X%02X", data->alpha, r, g, b);
            *data->colorHex = hexBuf;
            if (data->previewWindow) InvalidateRect(data->previewWindow, nullptr, FALSE);
        }
        return TRUE;
    }
    case WM_DESTROY:
        if (data && data->hBrush) {
            DeleteObject(data->hBrush);
            data->hBrush = nullptr;
        }
        return TRUE;
    }
    return FALSE;
}

void CorralWindow::ShowAppearanceDialog() {
    auto Align = [](WORD* p) { return (ULONG_PTR)p % 4 ? p + 1 : p; };

    // Build dynamic title with tab name
    std::wstring wTitle = Utf8ToWide(GetActiveTab().Title);
    std::wstring dlgTitleStr = L"Appearance: " + wTitle;
    const wchar_t* strDlgTitle = dlgTitleStr.c_str();
    const wchar_t* strFontName = L"Segoe UI";
    const wchar_t* strGrpColor = L"Background Color";
    const wchar_t* strBtnChange = L"Change...";
    const wchar_t* strGrpOpacity = L"Opacity";
    const wchar_t* strTrackbarClass = L"msctls_trackbar32";
    const wchar_t* strLabelPercent = L"100%";
    const wchar_t* strChkDefault = L"Use as default for new corrals";
    const wchar_t* strChkApplyAll = L"Apply to all corrals";
    const wchar_t* strBtnOK = L"OK";
    const wchar_t* strBtnCancel = L"Cancel";

    WORD dlgTemplate[2048] = {};
    WORD* p = dlgTemplate;

    DLGTEMPLATE* dlg = (DLGTEMPLATE*)p;
    dlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | DS_SETFONT;
    dlg->cdit = 10;  // 8 original + 2 checkboxes
    dlg->cx = 200;
    dlg->cy = 130;   // Increased height for checkboxes
    p += sizeof(DLGTEMPLATE) / sizeof(WORD);

    *p++ = 0; *p++ = 0;

    size_t len = wcslen(strDlgTitle) + 1;
    memcpy(p, strDlgTitle, len * sizeof(wchar_t)); p += len;

    *p++ = 9;
    len = wcslen(strFontName) + 1;
    memcpy(p, strFontName, len * sizeof(wchar_t)); p += len;

    // 1. Group Box "Background Color"
    p = Align(p);
    DLGITEMTEMPLATE* item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_GROUPBOX;
    item->x = 5; item->y = 5; item->cx = 190; item->cy = 35;
    item->id = (WORD)-1;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strGrpColor) + 1; memcpy(p, strGrpColor, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 2. Color Swatch (Static with Border) (ID 105)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_BORDER | SS_NOTIFY;
    item->x = 15; item->y = 18; item->cx = 30; item->cy = 14;
    item->id = 105;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0082; *p++ = 0; *p++ = 0;

    // 3. Change Button (ID 106)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 55; item->y = 18; item->cx = 60; item->cy = 14;
    item->id = 106;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strBtnChange) + 1; memcpy(p, strBtnChange, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 4. Group Box "Opacity"
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_GROUPBOX;
    item->x = 5; item->y = 45; item->cx = 190; item->cy = 30;
    item->id = (WORD)-1;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strGrpOpacity) + 1; memcpy(p, strGrpOpacity, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 5. Slider (ID 102) - use TBS_NOTICKS to remove tick marks
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS;
    item->x = 15; item->y = 56; item->cx = 140; item->cy = 15;
    item->id = 102;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    len = wcslen(strTrackbarClass) + 1;
    memcpy(p, strTrackbarClass, len * sizeof(wchar_t));
    p += len;
    *p++ = 0;
    *p++ = 0;

    // 6. Label % (ID 103)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | SS_RIGHT;
    item->x = 160; item->y = 58; item->cx = 25; item->cy = 12;
    item->id = 103;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0082;
    len = wcslen(strLabelPercent) + 1; memcpy(p, strLabelPercent, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 7. Checkbox "Use as default for new corrals" (ID 107)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
    item->x = 10; item->y = 82; item->cx = 180; item->cy = 12;
    item->id = 107;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strChkDefault) + 1; memcpy(p, strChkDefault, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 8. Checkbox "Apply to all corrals" (ID 108)
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
    item->x = 10; item->y = 96; item->cx = 180; item->cy = 12;
    item->id = 108;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strChkApplyAll) + 1; memcpy(p, strChkApplyAll, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 9. OK Button
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
    item->x = 90; item->y = 112; item->cx = 50; item->cy = 14;
    item->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strBtnOK) + 1; memcpy(p, strBtnOK, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // 10. Cancel Button
    p = Align(p);
    item = (DLGITEMTEMPLATE*)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 145; item->y = 112; item->cx = 50; item->cy = 14;
    item->id = IDCANCEL;
    p += sizeof(DLGITEMTEMPLATE) / sizeof(WORD);
    *p++ = 0xFFFF; *p++ = 0x0080;
    len = wcslen(strBtnCancel) + 1; memcpy(p, strBtnCancel, len * sizeof(wchar_t)); p += len; *p++ = 0;

    // Data prep
    AppearanceDlgData dlgData;
    dlgData.alpha = 153;
    dlgData.color = RGB(0,0,0);
    dlgData.previewWindow = hwnd;
    dlgData.colorHex = &GetActiveTab().ColorHex;
    dlgData.hBrush = nullptr;
    dlgData.useAsDefault = false;
    dlgData.applyToAll = false;

    const std::string& colorHexRef = GetActiveTab().ColorHex;
    if (!colorHexRef.empty() && colorHexRef[0] == '#' && colorHexRef.length() >= 9) {
        unsigned int colorValue;
        sscanf_s(colorHexRef.c_str() + 1, "%x", &colorValue);
        dlgData.alpha = (colorValue >> 24) & 0xFF;
        BYTE r = (colorValue >> 16) & 0xFF;
        BYTE g = (colorValue >> 8) & 0xFF;
        BYTE b = colorValue & 0xFF;
        dlgData.color = RGB(r, g, b);
    }

    std::string originalColor = GetActiveTab().ColorHex;

    INT_PTR result = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr),
        (DLGTEMPLATE*)dlgTemplate,
        hwnd,
        AppearanceDlgProc,
        (LPARAM)&dlgData
    );

    if (result != IDOK) {
        GetActiveTab().ColorHex = originalColor;
        UpdateLayeredContent();
    } else {
        App* app = App::GetInstance();
        if (app) {
            // Handle "Use as default for new corrals"
            if (dlgData.useAsDefault) {
                app->SetDefaultColorHex(GetActiveTab().ColorHex);
            }

            // Handle "Apply to all corrals"
            if (dlgData.applyToAll) {
                app->ApplyColorToAllCorrals(GetActiveTab().ColorHex);
            }

            app->SaveConfig();
        }
    }
}
