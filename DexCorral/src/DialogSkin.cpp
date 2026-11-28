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
 * DialogSkin.cpp - One look for every DexCorral dialog (see DialogSkin.h)
 */

#include "DialogSkin.h"
#include "AccentColor.h"
#include "App.h"
#include <CommCtrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <new>
#include <string>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

namespace DialogSkin
{
int forceTheme = -1;

namespace
{
const wchar_t *PROP_STATE = L"DexSkin";
const wchar_t *PROP_ROLE = L"DexSkinRole";
const wchar_t *PROP_HOT = L"DexSkinHot";
const wchar_t *PROP_DEFAULT = L"DexSkinDefault";
const wchar_t *PROP_POS = L"DexSkinPos"; // Track bar: 1 + the position last painted

enum Role : INT_PTR
{
    ROLE_NONE = 0,
    ROLE_HINT,
    ROLE_PAGETAB,
    ROLE_PAGETAB_SELECTED,
    ROLE_PRIMARY, // The dialog's default button
};

struct State
{
    Palette colors;
    bool dark = false;
    HBRUSH body = nullptr, footer = nullptr, field = nullptr;
    HFONT heading = nullptr;
    int footerTop = -1; // Client pixels; -1 = no footer
};

State *StateOf(HWND hDlg)
{
    return (State *)GetPropW(hDlg, PROP_STATE);
}

Role RoleOf(HWND control)
{
    return (Role)(INT_PTR)GetPropW(control, PROP_ROLE);
}

int Scale(HWND w, int px)
{
    return MulDiv(px, (int)GetDpiForWindow(w), 96);
}

COLORREF Mix(COLORREF a, COLORREF b, int percentOfB)
{
    auto mix = [&](int x, int y) { return (BYTE)((x * (100 - percentOfB) + y * percentOfB) / 100); };
    return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)), mix(GetBValue(a), GetBValue(b)));
}

bool HighContrast()
{
    HIGHCONTRASTW hc = {sizeof(hc)};
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON);
}

/// Draws with `draw` at three times the size and scales the result down into
/// `r`: GDI has no anti-aliasing of its own. `draw` gets the large DC and the scale.
template <typename Draw>
void Smooth(HDC dc, const RECT &r, COLORREF behind, Draw draw)
{
    const int scale = 3;
    const int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0)
        return;
    HDC big = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, w * scale, h * scale);
    if (!big || !bitmap)
    {
        if (bitmap)
            DeleteObject(bitmap);
        if (big)
            DeleteDC(big);
        return;
    }
    HGDIOBJ oldBitmap = SelectObject(big, bitmap);
    RECT all = {0, 0, w * scale, h * scale};
    HBRUSH back = CreateSolidBrush(behind);
    FillRect(big, &all, back);
    DeleteObject(back);
    draw(big, scale);

    const int oldMode = SetStretchBltMode(dc, HALFTONE);
    POINT org;
    SetBrushOrgEx(dc, 0, 0, &org);
    StretchBlt(dc, r.left, r.top, w, h, big, 0, 0, w * scale, h * scale, SRCCOPY);
    SetBrushOrgEx(dc, org.x, org.y, nullptr);
    SetStretchBltMode(dc, oldMode);

    SelectObject(big, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(big);
}

void Shape(HDC dc, COLORREF fill, COLORREF border, int penWidth, bool ellipse, int l, int t, int r, int b, int radius)
{
    HPEN pen = CreatePen(PS_INSIDEFRAME, penWidth, border);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, brush);
    if (ellipse)
        Ellipse(dc, l, t, r, b);
    else
        RoundRect(dc, l, t, r, b, radius * 2, radius * 2);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

/// The band (body or footer) a control lies in decides what is behind it
COLORREF BehindOf(HWND hDlg, HWND control, const State *s)
{
    if (s->footerTop < 0)
        return s->colors.body;
    RECT r;
    GetWindowRect(control, &r);
    MapWindowPoints(nullptr, hDlg, (POINT *)&r, 2);
    return r.top >= s->footerTop ? s->colors.footer : s->colors.body;
}

std::wstring TextOf(HWND w)
{
    wchar_t text[256] = {};
    GetWindowTextW(w, text, 256);
    return text;
}

bool ShowsFocus(HWND control)
{
    return !(SendMessageW(control, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS);
}

/// Underline the &-marked access key only once the user has pressed Alt, as Windows does
UINT AccelFlag(HWND control)
{
    return (SendMessageW(control, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) ? DT_HIDEPREFIX : 0;
}

void FocusRing(HDC dc, RECT r, int inset, int radius, const State *s, COLORREF behind)
{
    InflateRect(&r, -inset, -inset);
    HPEN pen = CreatePen(PS_SOLID, 1, s->colors.text);
    HGDIOBJ oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius * 2, radius * 2);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    (void)behind;
}

// ---- Push buttons (owner-drawn) ----

/// Hover tracking, and the two things an owner-drawn button no longer does by
/// itself: tell the dialog manager it is a push button (so Enter presses the
/// focused one), and keep its style when the default button moves.
LRESULT CALLBACK ButtonProc(HWND w, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR)
{
    switch (msg)
    {
    case WM_GETDLGCODE:
        return DLGC_BUTTON | (GetPropW(w, PROP_DEFAULT) ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);
    case BM_SETSTYLE:
        if ((wParam & BS_TYPEMASK) == BS_DEFPUSHBUTTON)
            SetPropW(w, PROP_DEFAULT, (HANDLE)1);
        else
            RemovePropW(w, PROP_DEFAULT);
        InvalidateRect(w, nullptr, FALSE);
        return 0; // The style stays BS_OWNERDRAW
    case WM_MOUSEMOVE:
        if (!GetPropW(w, PROP_HOT))
        {
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, w, 0};
            TrackMouseEvent(&track);
            SetPropW(w, PROP_HOT, (HANDLE)1);
            InvalidateRect(w, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        RemovePropW(w, PROP_HOT);
        InvalidateRect(w, nullptr, FALSE);
        break;
    case WM_NCDESTROY:
        RemovePropW(w, PROP_HOT);
        RemovePropW(w, PROP_DEFAULT);
        RemovePropW(w, PROP_ROLE);
        RemoveWindowSubclass(w, ButtonProc, id);
        break;
    }
    return DefSubclassProc(w, msg, wParam, lParam);
}

void DrawButton(HWND hDlg, const DRAWITEMSTRUCT *item, const State *s)
{
    const Palette &c = s->colors;
    HWND button = item->hwndItem;
    const Role role = RoleOf(button);
    const bool disabled = (item->itemState & ODS_DISABLED) != 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool hot = GetPropW(button, PROP_HOT) != nullptr;
    const bool focus = (item->itemState & ODS_FOCUS) && ShowsFocus(button);
    const COLORREF behind = BehindOf(hDlg, button, s);
    RECT r = item->rcItem;
    HDC dc = item->hDC;
    const std::wstring text = TextOf(button);
    HFONT font = (HFONT)SendMessageW(button, WM_GETFONT, 0, 0);
    SetBkMode(dc, TRANSPARENT);

    if (role == ROLE_PAGETAB || role == ROLE_PAGETAB_SELECTED)
    {
        const bool selected = (role == ROLE_PAGETAB_SELECTED);
        HBRUSH back = CreateSolidBrush(hot && !selected ? Mix(behind, c.text, 6) : behind);
        FillRect(dc, &r, back);
        DeleteObject(back);

        HGDIOBJ oldFont = SelectObject(dc, selected && s->heading ? s->heading : font);
        SetTextColor(dc, selected ? c.text : c.textMuted);
        RECT textRect = r;
        textRect.bottom -= Scale(button, 4);
        DrawTextW(dc, text.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | AccelFlag(button));
        if (selected)
        {
            RECT size = {};
            DrawTextW(dc, text.c_str(), -1, &size, DT_SINGLELINE | DT_CALCRECT); // Width without the '&'
            const int width = std::min((int)size.right, (int)(r.right - r.left) - Scale(button, 8));
            const int mid = (r.left + r.right) / 2;
            RECT line = {mid - width / 2, r.bottom - Scale(button, 3), mid + width / 2, r.bottom};
            FillRoundRect(dc, line, Scale(button, 2), c.accent, c.accent, behind);
        }
        SelectObject(dc, oldFont);
        if (focus)
            FocusRing(dc, r, Scale(button, 1), Scale(button, 4), s, behind);
        return;
    }

    COLORREF fill, border, textColor;
    if (role == ROLE_PRIMARY)
    {
        fill = pressed ? Mix(c.accent, behind, 25) : hot ? Mix(c.accent, behind, 12) : c.accent;
        border = fill;
        textColor = c.onAccent;
        if (disabled)
        {
            fill = border = Mix(behind, c.text, 20);
            textColor = c.textDisabled;
        }
    }
    else
    {
        fill = pressed ? c.buttonPressed : hot ? c.buttonHover : c.button;
        border = c.buttonBorder;
        textColor = disabled ? c.textDisabled : c.text;
    }
    FillRoundRect(dc, r, Scale(button, 4), fill, border, behind);

    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, textColor);
    DrawTextW(dc, text.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | AccelFlag(button));
    SelectObject(dc, oldFont);
    if (focus)
        FocusRing(dc, r, Scale(button, 2), Scale(button, 3), s, behind);
}

// ---- Check boxes and track bars (custom draw) ----

LRESULT DrawCheckBox(HWND hDlg, const NMCUSTOMDRAW *draw, const State *s)
{
    if (draw->dwDrawStage != CDDS_PREPAINT)
        return CDRF_DODEFAULT;
    const Palette &c = s->colors;
    HWND box = draw->hdr.hwndFrom;
    HDC dc = draw->hdc;
    RECT r;
    GetClientRect(box, &r);
    const COLORREF behind = BehindOf(hDlg, box, s);
    const bool checked = SendMessageW(box, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const bool disabled = !IsWindowEnabled(box);
    const bool hot = (draw->uItemState & CDIS_HOT) != 0;

    HBRUSH back = CreateSolidBrush(behind);
    FillRect(dc, &r, back);
    DeleteObject(back);

    const int size = Scale(box, 16);
    RECT square = {r.left, (r.top + r.bottom - size) / 2, r.left + size, (r.top + r.bottom - size) / 2 + size};
    COLORREF fill = checked ? c.accent : (hot ? c.buttonHover : c.field);
    COLORREF edge = checked ? c.accent : Mix(behind, c.text, 55);
    if (disabled)
    {
        fill = checked ? Mix(behind, c.text, 30) : behind;
        edge = Mix(behind, c.text, 30);
    }
    Smooth(dc, square, behind, [&](HDC big, int k)
    {
        const int n = size * k;
        Shape(big, fill, edge, k, false, 0, 0, n, n, 3 * k);
        if (checked)
        {
            HPEN pen = CreatePen(PS_SOLID, 2 * k, disabled ? behind : c.onAccent);
            HGDIOBJ oldPen = SelectObject(big, pen);
            MoveToEx(big, n * 26 / 100, n * 52 / 100, nullptr);
            LineTo(big, n * 43 / 100, n * 69 / 100);
            LineTo(big, n * 75 / 100, n * 33 / 100);
            SelectObject(big, oldPen);
            DeleteObject(pen);
        }
    });

    HGDIOBJ oldFont = SelectObject(dc, (HFONT)SendMessageW(box, WM_GETFONT, 0, 0));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? c.textDisabled : c.text);
    RECT textRect = {square.right + Scale(box, 7), r.top, r.right, r.bottom};
    const std::wstring text = TextOf(box);
    DrawTextW(dc, text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | AccelFlag(box));
    if ((draw->uItemState & CDIS_FOCUS) && ShowsFocus(box))
    {
        RECT measure = textRect;
        DrawTextW(dc, text.c_str(), -1, &measure, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT);
        measure.top = r.top;
        measure.bottom = r.bottom;
        InflateRect(&measure, Scale(box, 2), 0);
        DrawFocusRect(dc, &measure);
    }
    SelectObject(dc, oldFont);
    return CDRF_SKIPDEFAULT;
}

LRESULT DrawTrackBar(HWND hDlg, const NMCUSTOMDRAW *draw, const State *s)
{
    if (draw->dwDrawStage == CDDS_PREPAINT)
    {
        // The knob is drawn wider than the bar's own thumb and the filled part
        // of the channel moves with it, but the bar repaints only around its
        // thumb: after a move, have all of it painted once more
        HWND moved = draw->hdr.hwndFrom;
        const INT_PTR pos = SendMessageW(moved, TBM_GETPOS, 0, 0) + 1;
        if ((INT_PTR)GetPropW(moved, PROP_POS) != pos)
        {
            SetPropW(moved, PROP_POS, (HANDLE)pos);
            InvalidateRect(moved, nullptr, TRUE);
        }
        return CDRF_NOTIFYITEMDRAW;
    }
    if (draw->dwDrawStage != CDDS_ITEMPREPAINT)
        return CDRF_DODEFAULT;

    const Palette &c = s->colors;
    HWND bar = draw->hdr.hwndFrom;
    const COLORREF behind = BehindOf(hDlg, bar, s);
    const bool disabled = !IsWindowEnabled(bar);
    const COLORREF accent = disabled ? Mix(behind, c.text, 30) : c.accent;
    RECT thumb;
    SendMessageW(bar, TBM_GETTHUMBRECT, 0, (LPARAM)&thumb);

    if (draw->dwItemSpec == TBCD_CHANNEL)
    {
        const int thick = Scale(bar, 4);
        const int mid = (draw->rc.top + draw->rc.bottom) / 2;
        RECT track = {draw->rc.left, mid - thick / 2, draw->rc.right, mid - thick / 2 + thick};
        FillRoundRect(draw->hdc, track, thick / 2, Mix(behind, c.text, 45), Mix(behind, c.text, 45), behind);
        RECT filled = track;
        filled.right = std::max((LONG)(track.left + thick), std::min(track.right, (thumb.left + thumb.right) / 2));
        FillRoundRect(draw->hdc, filled, thick / 2, accent, accent, behind);
        return CDRF_SKIPDEFAULT;
    }
    if (draw->dwItemSpec == TBCD_THUMB)
    {
        // A round knob, wider than the bar's own narrow thumb; HandleMessage
        // repaints the whole bar on every move so nothing of it is left behind
        const int d = std::min(Scale(bar, 18), (int)(thumb.bottom - thumb.top));
        const int cx = (thumb.left + thumb.right) / 2, cy = (thumb.top + thumb.bottom) / 2;
        RECT knob = {cx - d / 2, cy - d / 2, cx - d / 2 + d, cy - d / 2 + d};
        const bool pressed = (draw->uItemState & CDIS_SELECTED) != 0;
        Smooth(draw->hdc, knob, behind, [&](HDC big, int k)
        {
            const int n = d * k;
            Shape(big, s->dark ? RGB(0x45, 0x45, 0x45) : RGB(0xFF, 0xFF, 0xFF), Mix(behind, c.text, 30), k, true, 0, 0, n, n, 0);
            const int inner = n * (pressed ? 38 : 28) / 100;
            Shape(big, accent, accent, 1, true, inner, inner, n - inner, n - inner, 0);
        });
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_SKIPDEFAULT; // No tick marks
}

/// A control that knows no dark mode and paints black on white (the hot key
/// field). It paints only to its own window, so it is left to do that and the
/// result is recoloured in place: white becomes the field colour, black the text.
LRESULT CALLBACK DarkFieldProc(HWND w, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR)
{
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(w, DarkFieldProc, id);
    // Only what the control is about to paint: what it painted earlier is recoloured already
    RECT dirty = {};
    const bool painting = (msg == WM_PAINT) && GetUpdateRect(w, &dirty, FALSE);
    const LRESULT result = DefSubclassProc(w, msg, wParam, lParam);
    if (!painting)
        return result;

    const int width = dirty.right - dirty.left, height = dirty.bottom - dirty.top;
    if (width <= 0 || height <= 0)
        return result;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    DWORD *pixels = nullptr;
    HDC dc = GetDC(w);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&pixels, nullptr, 0);
    if (dc && mem && bitmap && pixels)
    {
        HGDIOBJ old = SelectObject(mem, bitmap);
        BitBlt(mem, 0, 0, width, height, dc, dirty.left, dirty.top, SRCCOPY);
        GdiFlush();
        const Palette c = CurrentPalette();
        for (int i = 0; i < width * height; i++)
        {
            // How light the pixel is decides how much field and how much text it becomes
            const DWORD p = pixels[i];
            const int light = (((p >> 16) & 0xFF) + ((p >> 8) & 0xFF) + (p & 0xFF)) / 3;
            const COLORREF mixed = Mix(c.text, c.field, light * 100 / 255);
            pixels[i] = (GetRValue(mixed) << 16) | (GetGValue(mixed) << 8) | GetBValue(mixed);
        }
        BitBlt(dc, dirty.left, dirty.top, width, height, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
    }
    if (bitmap)
        DeleteObject(bitmap);
    if (mem)
        DeleteDC(mem);
    if (dc)
        ReleaseDC(w, dc);
    return result;
}

BOOL CALLBACK SkinChild(HWND child, LPARAM lParam)
{
    const State *s = (const State *)lParam;
    wchar_t cls[32] = {};
    GetClassNameW(child, cls, 32);
    if (_wcsicmp(cls, L"Button") == 0)
    {
        const LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
        const LONG_PTR type = style & BS_TYPEMASK;
        if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON)
        {
            if (type == BS_DEFPUSHBUTTON)
            {
                SetPropW(child, PROP_ROLE, (HANDLE)ROLE_PRIMARY);
                SetPropW(child, PROP_DEFAULT, (HANDLE)1);
            }
            SetWindowLongPtrW(child, GWL_STYLE, (style & ~(LONG_PTR)BS_TYPEMASK) | BS_OWNERDRAW);
            SetWindowSubclass(child, ButtonProc, 1, 0);
        }
    }
    else if (s->dark && (_wcsicmp(cls, L"ComboBox") == 0 || _wcsicmp(cls, L"Edit") == 0))
    {
        SetWindowTheme(child, L"DarkMode_CFD", nullptr);
    }
    else if (s->dark && _wcsicmp(cls, L"msctls_hotkey32") == 0)
    {
        SetWindowSubclass(child, DarkFieldProc, 2, 0);
    }
    return TRUE;
}
} // namespace

bool IsDark()
{
    if (forceTheme >= 0)
        return forceTheme == 1;
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return light == 0;
}

Palette CurrentPalette()
{
    Palette c;
    const unsigned accent = App::AccentRgb();
    c.accent = RGB((accent >> 16) & 0xFF, (accent >> 8) & 0xFF, accent & 0xFF);
    c.onAccent = AccentColor::TextOn(accent) == "#000000" ? RGB(0, 0, 0) : RGB(255, 255, 255);
    if (IsDark())
    {
        c.body = RGB(0x2B, 0x2B, 0x2B);
        c.footer = RGB(0x20, 0x20, 0x20);
        c.line = RGB(0x1A, 0x1A, 0x1A);
        c.text = RGB(0xFF, 0xFF, 0xFF);
        c.textMuted = RGB(0xC5, 0xC5, 0xC5);
        c.textDisabled = RGB(0x7A, 0x7A, 0x7A);
        c.button = RGB(0x37, 0x37, 0x37);
        c.buttonHover = RGB(0x40, 0x40, 0x40);
        c.buttonPressed = RGB(0x30, 0x30, 0x30);
        c.buttonBorder = RGB(0x4C, 0x4C, 0x4C);
        c.field = RGB(0x1F, 0x1F, 0x1F);
    }
    else
    {
        c.body = RGB(0xFF, 0xFF, 0xFF);
        c.footer = RGB(0xF3, 0xF3, 0xF3);
        c.line = RGB(0xE5, 0xE5, 0xE5);
        c.text = RGB(0x1B, 0x1B, 0x1B);
        c.textMuted = RGB(0x5F, 0x5F, 0x5F);
        c.textDisabled = RGB(0xA0, 0xA0, 0xA0);
        c.button = RGB(0xFD, 0xFD, 0xFD);
        c.buttonHover = RGB(0xF4, 0xF4, 0xF4);
        c.buttonPressed = RGB(0xEA, 0xEA, 0xEA);
        c.buttonBorder = RGB(0xCC, 0xCC, 0xCC);
        c.field = RGB(0xFF, 0xFF, 0xFF);
    }
    return c;
}

void FillRoundRect(HDC dc, const RECT &r, int radius, COLORREF fill, COLORREF border, COLORREF behind)
{
    const int w = r.right - r.left, h = r.bottom - r.top;
    Smooth(dc, r, behind, [&](HDC big, int k) { Shape(big, fill, border, k, false, 0, 0, w * k, h * k, radius * k); });
}

void Attach(HWND hDlg, int footerTopDlu)
{
    if (HighContrast() || StateOf(hDlg))
        return;

    State *s = new (std::nothrow) State;
    if (!s)
        return;
    s->dark = IsDark();
    s->colors = CurrentPalette();
    s->body = CreateSolidBrush(s->colors.body);
    s->footer = CreateSolidBrush(s->colors.footer);
    s->field = CreateSolidBrush(s->colors.field);
    if (footerTopDlu >= 0)
    {
        RECT r = {0, footerTopDlu, 0, 0};
        MapDialogRect(hDlg, &r);
        s->footerTop = r.top;
    }

    LOGFONTW lf = {};
    if (HFONT font = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0))
    {
        GetObjectW(font, sizeof(lf), &lf);
        lf.lfWeight = FW_SEMIBOLD;
        s->heading = CreateFontIndirectW(&lf);
    }
    if (!SetPropW(hDlg, PROP_STATE, s))
    {
        DeleteObject(s->body);
        DeleteObject(s->footer);
        DeleteObject(s->field);
        if (s->heading)
            DeleteObject(s->heading);
        delete s;
        return;
    }

    if (s->dark)
    {
        BOOL on = TRUE;
        DwmSetWindowAttribute(hDlg, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &on, sizeof(on));
    }
    EnumChildWindows(hDlg, SkinChild, (LPARAM)s);
}

void MakeHint(HWND hDlg, int id)
{
    HWND control = GetDlgItem(hDlg, id);
    if (StateOf(hDlg) && control)
        SetPropW(control, PROP_ROLE, (HANDLE)ROLE_HINT);
}

void MakePageTab(HWND hDlg, int id, bool selected)
{
    HWND control = GetDlgItem(hDlg, id);
    if (!StateOf(hDlg) || !control)
        return;
    SetPropW(control, PROP_ROLE, (HANDLE)(selected ? ROLE_PAGETAB_SELECTED : ROLE_PAGETAB));
    InvalidateRect(control, nullptr, FALSE);
}

bool HandleMessage(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam, INT_PTR &result)
{
    State *s = StateOf(hDlg);
    if (!s)
        return false;
    const Palette &c = s->colors;

    // Most messages answer through DWLP_MSGRESULT; the colour messages return their brush
    auto answer = [&](LRESULT value)
    {
        SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, value);
        result = TRUE;
        return true;
    };

    switch (msg)
    {
    case WM_ERASEBKGND:
    {
        HDC dc = (HDC)wParam;
        RECT r;
        GetClientRect(hDlg, &r);
        RECT body = r;
        if (s->footerTop >= 0)
        {
            body.bottom = s->footerTop;
            RECT footer = {r.left, s->footerTop, r.right, r.bottom};
            FillRect(dc, &footer, s->footer);
            RECT line = {r.left, s->footerTop, r.right, s->footerTop + 1};
            HBRUSH brush = CreateSolidBrush(c.line);
            FillRect(dc, &line, brush);
            DeleteObject(brush);
        }
        FillRect(dc, &body, s->body);
        return answer(1);
    }
    case WM_CTLCOLORDLG:
        result = (INT_PTR)s->body;
        return true;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HWND control = (HWND)lParam;
        const bool inFooter = BehindOf(hDlg, control, s) == c.footer && s->footerTop >= 0;
        const Role role = RoleOf(control);
        SetTextColor((HDC)wParam, !IsWindowEnabled(control) ? c.textDisabled : role == ROLE_HINT ? c.textMuted : c.text);
        SetBkColor((HDC)wParam, inFooter ? c.footer : c.body);
        result = (INT_PTR)(inFooter ? s->footer : s->body);
        return true;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wParam, c.text);
        SetBkColor((HDC)wParam, c.field);
        result = (INT_PTR)s->field;
        return true;
    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT *item = (const DRAWITEMSTRUCT *)lParam;
        if (item && item->CtlType == ODT_BUTTON)
        {
            DrawButton(hDlg, item, s);
            return answer(TRUE);
        }
        break;
    }
    case WM_NOTIFY:
    {
        const NMHDR *header = (const NMHDR *)lParam;
        if (header && header->code == NM_CUSTOMDRAW)
        {
            wchar_t cls[32] = {};
            GetClassNameW(header->hwndFrom, cls, 32);
            if (_wcsicmp(cls, L"msctls_trackbar32") == 0)
                return answer(DrawTrackBar(hDlg, (const NMCUSTOMDRAW *)lParam, s));
            if (_wcsicmp(cls, L"Button") == 0)
            {
                const LONG_PTR type = GetWindowLongPtrW(header->hwndFrom, GWL_STYLE) & BS_TYPEMASK;
                if (type == BS_AUTOCHECKBOX || type == BS_CHECKBOX)
                    return answer(DrawCheckBox(hDlg, (const NMCUSTOMDRAW *)lParam, s));
            }
        }
        break;
    }
    case WM_NCDESTROY:
        RemovePropW(hDlg, PROP_STATE);
        DeleteObject(s->body);
        DeleteObject(s->footer);
        DeleteObject(s->field);
        if (s->heading)
            DeleteObject(s->heading);
        delete s;
        break;
    }
    return false;
}
} // namespace DialogSkin
