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
 * CorralWindowRender.cpp - Per-pixel alpha rendering
 *
 * Implements layered window rendering using DIB sections with per-pixel alpha blending.
 * Manages color compositing, icon rendering, and hover/selection highlights.
 *
 * All text goes through a scratch TextLayer rather than being drawn into the back
 * buffer directly: neither text engine can be asked for an alpha (GDI writes none
 * at all, DrawThemeTextEx has no opacity option), so the header titles and icon
 * labels are drawn separately and faded as they are composited.
 */

#include "CorralWindow.h"
#include "Constants.h"
#include "TextLayer.h"
#include "App.h"
#include "Strings.h"
#include <windowsx.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <cstdio>
#include <algorithm>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace
{
    /**
     * Padlock glyph, rasterised analytically rather than drawn from a font.
     *
     * GDI text writes no alpha, so every string costs a TextLayer scratch-DIB round
     * trip (see TextLayer.h). The lock is two primitives — a rounded-rect body and
     * the upper half of an annulus for the shackle — so it can be sampled straight
     * into the back buffer at whatever alpha the header is using, with no font
     * availability or hinting risk at any DPI.
     */
    struct LockGlyph
    {
        double gw, gh, bodyTop, radius, cx, ro, ri;

        LockGlyph(int w, int h)
        {
            gw = w;
            gh = h;
            bodyTop = gh * 0.42;
            radius = gw * 0.22;
            cx = gw * 0.5;
            ro = gw * 0.36;
            double thickness = gw * 0.15;
            if (thickness < 1.0)
                thickness = 1.0;
            ri = ro - thickness;
            if (ri < 0.0)
                ri = 0.0;
        }

        bool Inside(double x, double y) const
        {
            // Body: rounded rect spanning the lower part of the glyph box
            if (y >= bodyTop)
            {
                const double l = radius, r = gw - radius, t = bodyTop + radius, b = gh - radius;
                const double dx = (x < l) ? l - x : ((x > r) ? x - r : 0.0);
                const double dy = (y < t) ? t - y : ((y > b) ? y - b : 0.0);
                if (x >= 0.0 && x <= gw && y <= gh && dx * dx + dy * dy <= radius * radius)
                    return true;
            }
            // Shackle: upper half of an annulus centred on the body's top edge
            if (y <= bodyTop)
            {
                const double dx = x - cx, dy = y - bodyTop;
                const double d2 = dx * dx + dy * dy;
                if (d2 <= ro * ro && d2 >= ri * ri)
                    return true;
            }
            return false;
        }

        /// 4x4 supersampled coverage of one pixel, 0..255.
        BYTE Coverage(int px, int py) const
        {
            const int n = 4;
            int hits = 0;
            for (int sy = 0; sy < n; sy++)
                for (int sx = 0; sx < n; sx++)
                    if (Inside(px + (sx + 0.5) / n, py + (sy + 0.5) / n))
                        hits++;
            return (BYTE)((hits * 255) / (n * n));
        }
    };
}

void CorralWindow::UpdateLayeredContent()
{
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0)
        return;

    // Create a 32-bit DIB for per-pixel alpha
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    void *bits = nullptr;
    HBITMAP memBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!memBitmap || !bits)
    {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
    DWORD *pixels = (DWORD *)bits;

    // Get overlay color and alpha from active tab
    BYTE bgAlpha = 153; // Default ~60% opacity
    BYTE bgR = 0, bgG = 0, bgB = 0;

    const std::string &colorHex = GetActiveTab().ColorHex;
    if (!colorHex.empty() && colorHex[0] == '#' && colorHex.length() >= 9)
    {
        unsigned int colorValue;
        sscanf_s(colorHex.c_str() + 1, "%x", &colorValue);
        bgAlpha = (colorValue >> 24) & 0xFF;
        bgR = (colorValue >> 16) & 0xFF;
        bgG = (colorValue >> 8) & 0xFF;
        bgB = colorValue & 0xFF;
    }

    // Premultiply colors (required for per-pixel alpha blending)
    BYTE pmR = (BYTE)((bgR * bgAlpha) / 255);
    BYTE pmG = (BYTE)((bgG * bgAlpha) / 255);
    BYTE pmB = (BYTE)((bgB * bgAlpha) / 255);
    DWORD bgPixel = (bgAlpha << 24) | (pmR << 16) | (pmG << 8) | pmB;

    // Fill background with semi-transparent color
    for (int i = 0; i < w * h; i++)
    {
        pixels[i] = bgPixel;
    }

    // Header alpha: the user's setting, hover-animated, floored while rolled up
    // (rolled up the header is the whole corral — see ChromeAlpha.h).
    const int headerAlpha = ChromeAlpha::EffectiveHeaderAlpha(currentHeaderOpacity, config.IsRolledUp);
    // Inactive tabs are derived from it, never configured separately, so the
    // active tab stays distinguishable wherever the slider sits.
    const int inactiveAlpha = ChromeAlpha::InactiveTabAlpha(headerAlpha);
    const int inactiveRgbPercent = ChromeAlpha::InactiveTabRgbScalePercent(headerAlpha);

    // Compute per-tab pixel colors (each tab uses its own ColorHex)
    std::vector<DWORD> tabPixels(config.Tabs.size());
    for (int i = 0; i < (int)config.Tabs.size(); i++)
    {
        BYTE tabR = bgR, tabG = bgG, tabB = bgB;
        const std::string &tabColorHex = config.Tabs[i].ColorHex;
        if (!tabColorHex.empty() && tabColorHex[0] == '#' && tabColorHex.length() >= 9)
        {
            unsigned int tabColorValue;
            sscanf_s(tabColorHex.c_str() + 1, "%x", &tabColorValue);
            tabR = (tabColorValue >> 16) & 0xFF;
            tabG = (tabColorValue >> 8) & 0xFF;
            tabB = tabColorValue & 0xFF;
        }

        if (i == config.ActiveTabIndex)
        {
            // Active tab: the configured header opacity, full colour
            tabPixels[i] = ChromeAlpha::MakePremultiplied((BYTE)headerAlpha, tabR, tabG, tabB);
        }
        else
        {
            // Inactive tab: derived alpha plus a colour darkening that deepens as
            // the header fades, so the two stay apart even near the opacity floor
            tabPixels[i] = ChromeAlpha::MakePremultiplied(
                (BYTE)inactiveAlpha,
                ChromeAlpha::ScaleChannel(tabR, inactiveRgbPercent),
                ChromeAlpha::ScaleChannel(tabG, inactiveRgbPercent),
                ChromeAlpha::ScaleChannel(tabB, inactiveRgbPercent));
        }
    }

    // Draw tab backgrounds
    for (int i = 0; i < (int)config.Tabs.size(); i++)
    {
        RECT tabRect = GetTabRect(i);
        DWORD pixel = tabPixels[i];

        for (int y = tabRect.top; y < tabRect.bottom && y < h; y++)
        {
            for (int x = tabRect.left; x < tabRect.right && x < w; x++)
            {
                if (x >= 0 && y >= 0)
                {
                    pixels[y * w + x] = pixel;
                }
            }
        }

        // Draw tab separator (vertical line between tabs)
        if (i > 0)
        {
            DWORD sepPixel = TAB_SEPARATOR_COLOR;
            for (int y = 2; y < GetTitleBarHeight() - 2 && y < h; y++)
            {
                if (tabRect.left >= 0 && tabRect.left < w)
                {
                    pixels[y * w + tabRect.left] = sepPixel;
                }
            }
        }
    }

    // Draw border (1px line at the configured, hover-animated opacity).
    // Composited over what is already there rather than overwriting it, so a
    // partly transparent border blends with the corral fill instead of punching
    // a hole in it, and a border at 0 leaves no trace at all.
    const int borderAlpha = ChromeAlpha::ClampBorderOpacity(currentBorderOpacity);
    if (borderAlpha > 0)
    {
        DWORD borderPixel = ChromeAlpha::MakePremultiplied((BYTE)borderAlpha,
                                                           CORRAL_BORDER_R, CORRAL_BORDER_G, CORRAL_BORDER_B);
        // Top edge
        for (int x = 0; x < w; x++)
            pixels[x] = ChromeAlpha::PremultipliedOver(borderPixel, pixels[x]);
        // Bottom edge
        for (int x = 0; x < w; x++)
            pixels[(h - 1) * w + x] = ChromeAlpha::PremultipliedOver(borderPixel, pixels[(h - 1) * w + x]);
        // Left edge
        for (int y = 0; y < h; y++)
            pixels[y * w] = ChromeAlpha::PremultipliedOver(borderPixel, pixels[y * w]);
        // Right edge
        for (int y = 0; y < h; y++)
            pixels[y * w + (w - 1)] = ChromeAlpha::PremultipliedOver(borderPixel, pixels[y * w + (w - 1)]);
    }

    // Now use GDI to draw content (text, icons) on top

    SetBkMode(memDC, TRANSPARENT);

    // Per-tab header font colours, parsed once — the title composite and the
    // reorder grip below both need them.
    std::vector<BYTE> tabFontR(config.Tabs.size(), 255);
    std::vector<BYTE> tabFontG(config.Tabs.size(), 255);
    std::vector<BYTE> tabFontB(config.Tabs.size(), 255);
    for (int i = 0; i < (int)config.Tabs.size(); i++)
    {
        const std::string &fontColorHex = config.Tabs[i].HeaderFontColor;
        if (!fontColorHex.empty() && fontColorHex[0] == '#' && fontColorHex.length() >= 7)
        {
            unsigned int fontColorVal;
            sscanf_s(fontColorHex.c_str() + 1, "%x", &fontColorVal);
            tabFontR[i] = (fontColorVal >> 16) & 0xFF;
            tabFontG[i] = (fontColorVal >> 8) & 0xFF;
            tabFontB[i] = fontColorVal & 0xFF;
        }
    }

    /**
     * Tab titles.
     *
     * GDI writes no alpha at all, so the titles cannot go straight into the
     * back buffer: they are drawn black-on-white into a coverage layer, and
     * that coverage becomes the alpha they are composited back with. Two things
     * fall out of this that drawing in place could not do — the per-tab text
     * opacity is just a factor on the coverage, and a black header font finally
     * renders (in place, "no alpha and no colour" is indistinguishable from a
     * pixel GDI merely cleared, so black titles were swallowed by the fix-up
     * that restored the tab background underneath them).
     *
     * The font is created with ANTIALIASED_QUALITY rather than ClearType:
     * subpixel antialiasing encodes coverage per colour channel, which a single
     * alpha channel cannot carry, and it was never meaningful on a layered
     * window composited over an unknown wallpaper anyway. This matches how the
     * icon labels have always been drawn.
     */
    // Reserved before the titles are laid out: the padlock sits over the last tab,
    // so that tab has to give up the width or a long title runs under it.
    const RECT lockRect = GetLockGlyphRect();
    const int lockReserve = (lockRect.right > lockRect.left) ? (lockRect.right - lockRect.left) + Dpi(4) : 0;

    const int titleLayerH = (GetTitleBarHeight() < h) ? GetTitleBarHeight() : h;
    if (titleLayerH > 0 && !config.Tabs.empty())
    {
        // Each tab's own opacity, brought towards full by however far the hover
        // fade has run (0 while unhovered, so this is the configured value).
        std::vector<int> tabTextAlpha(config.Tabs.size());
        for (int i = 0; i < (int)config.Tabs.size(); i++)
            tabTextAlpha[i] = ChromeAlpha::HoverBlendTextOpacity(config.Tabs[i].HeaderFontOpacity,
                                                                 currentTextHover);

        // Which tab owns each column, so the composite knows the text's colour
        // and opacity. Titles are drawn with DT_END_ELLIPSIS inside their own
        // tab rect, so they never cross into a neighbour's columns.
        std::vector<int> tabAtX(w, -1);
        for (int i = 0; i < (int)config.Tabs.size(); i++)
        {
            RECT tr = GetTabRect(i);
            for (int x = tr.left; x < tr.right && x < w; x++)
                if (x >= 0)
                    tabAtX[x] = i;
        }

        TextLayer titles(screenDC, w, titleLayerH, TEXT_COVERAGE_LAYER_FILL);
        if (titles.IsValid())
        {
            SetTextColor(titles.DC(), RGB(0, 0, 0));

            HFONT oldFont = nullptr;
            HFONT lastFont = nullptr;

            for (int i = 0; i < (int)config.Tabs.size(); i++)
            {
                const CorralTabConfig &tab = config.Tabs[i];

                // Skip a fully transparent title: no point spending a font and a
                // DrawTextW on something that composites to nothing.
                if (tabTextAlpha[i] == 0)
                    continue;

                std::wstring fontNameW = Utf8ToWide(tab.HeaderFontName);
                int fontHeight = -MulDiv(tab.HeaderFontSize, GetDpiForWindow(hwnd), 72);
                HFONT titleFont = CreateFontW(fontHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, fontNameW.c_str());
                HFONT prevFont = (HFONT)SelectObject(titles.DC(), titleFont);
                if (oldFont == nullptr)
                    oldFont = prevFont; // Save the very first original font to restore later

                std::wstring wtitle = Utf8ToWide(tab.Title);

                // Add symbol prefix
                if (tab.IsVirtual)
                {
                    wtitle = L"\U0001F4C1 " + wtitle; // Folder emoji
                }
                else if (tab.IsCatchAll)
                {
                    wtitle = L"\u2B07 " + wtitle; // Down arrow (escaped: this source is not compiled as UTF-8)
                }

                RECT tabRect = GetTabRect(i);
                tabRect.left += 6;
                tabRect.right -= 6;
                if (i == (int)config.Tabs.size() - 1)
                    tabRect.right -= lockReserve;
                tabRect.top += 2;

                DrawTextW(titles.DC(), wtitle.c_str(), (int)wtitle.length(), &tabRect,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

                if (lastFont)
                    DeleteObject(lastFont);
                lastFont = titleFont;
            }

            if (oldFont)
                SelectObject(titles.DC(), oldFont);
            if (lastFont)
                DeleteObject(lastFont);

            // Composite the coverage back over the tab backgrounds, each tab in
            // its own colour and at its own opacity.
            const DWORD *cov = titles.Pixels();
            for (int y = 0; y < titleLayerH; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    // Greyscale coverage: the layer started white and the glyphs
                    // were drawn black, so darkness is coverage.
                    BYTE coverage = (BYTE)(255 - ((cov[y * w + x] >> 8) & 0xFF));
                    if (coverage == 0)
                        continue;

                    int ti = tabAtX[x];
                    if (ti < 0)
                        continue;

                    BYTE a = ChromeAlpha::TextCoverageAlpha(coverage, tabTextAlpha[ti]);
                    if (a == 0)
                        continue;

                    DWORD glyph = ChromeAlpha::MakePremultiplied(a, tabFontR[ti], tabFontG[ti], tabFontB[ti]);
                    pixels[y * w + x] = ChromeAlpha::PremultipliedOver(glyph, pixels[y * w + x]);
                }
            }
        }
    }

    // ---- Tab reorder grip ("Griff") — shown only on the hovered/dragged tab ----
    // Two columns of three dots, the classic drag-handle, in the tab's header
    // font colour. Deliberately not subject to the header text opacity: the grip
    // is an affordance, not decoration, and only appears on hover anyway.
    if (config.Tabs.size() > 1)
    {
        for (int i = 0; i < (int)config.Tabs.size(); i++)
        {
            bool show = (i == hoveredTab) || (isDraggingTab && i == draggedTabIndex);
            if (!show)
                continue;

            BYTE gr = tabFontR[i], gg = tabFontG[i], gb = tabFontB[i];
            BYTE ga = (isDraggingTab && i == draggedTabIndex) ? 255 : 200;
            DWORD dot = (ga << 24) | ((BYTE)(gr * ga / 255) << 16) |
                        ((BYTE)(gg * ga / 255) << 8) | (BYTE)(gb * ga / 255);

            RECT grip = GetTabGripRect(i);
            int dotSize = Dpi(2);
            int gap = Dpi(2);
            const int cols = 2, rows = 3;
            int blockW = cols * dotSize + (cols - 1) * gap;
            int blockH = rows * dotSize + (rows - 1) * gap;
            int startX = grip.left + ((grip.right - grip.left) - blockW) / 2;
            int startY = (GetTitleBarHeight() - blockH) / 2;

            for (int cx = 0; cx < cols; cx++)
                for (int cy = 0; cy < rows; cy++)
                {
                    int px0 = startX + cx * (dotSize + gap);
                    int py0 = startY + cy * (dotSize + gap);
                    for (int dy = 0; dy < dotSize; dy++)
                        for (int dx = 0; dx < dotSize; dx++)
                        {
                            int px = px0 + dx, py = py0 + dy;
                            if (px >= 0 && px < w && py >= 0 && py < h)
                                pixels[py * w + px] = dot;
                        }
                }
        }
    }

    // ---- Lock Position badge ----
    // Composited over the title bar in the last tab's header colour and opacity, so
    // it reads as part of the header rather than an overlay pasted on top of it.
    if (lockRect.right > lockRect.left && !config.Tabs.empty())
    {
        const int lw = lockRect.right - lockRect.left;
        const int lh = lockRect.bottom - lockRect.top;
        const int ti = (int)config.Tabs.size() - 1;
        const int lockAlpha = ChromeAlpha::HoverBlendTextOpacity(config.Tabs[ti].HeaderFontOpacity,
                                                                 currentTextHover);
        if (lockAlpha > 0)
        {
            LockGlyph glyph(lw, lh);
            for (int gy = 0; gy < lh; gy++)
            {
                const int py = lockRect.top + gy;
                if (py < 0 || py >= h)
                    continue;
                for (int gx = 0; gx < lw; gx++)
                {
                    const int px = lockRect.left + gx;
                    if (px < 0 || px >= w)
                        continue;
                    BYTE coverage = glyph.Coverage(gx, gy);
                    if (coverage == 0)
                        continue;
                    BYTE a = ChromeAlpha::TextCoverageAlpha(coverage, lockAlpha);
                    if (a == 0)
                        continue;
                    DWORD lockPixel = ChromeAlpha::MakePremultiplied(a, tabFontR[ti], tabFontG[ti], tabFontB[ti]);
                    pixels[py * w + px] = ChromeAlpha::PremultipliedOver(lockPixel, pixels[py * w + px]);
                }
            }
        }
    }

    // ---- Virtual corral: nav "up" button, details column header, unavailable message ----
    if (IsNavBackVisible() || GetDetailsHeaderHeight() > 0 || virtualFolderMissing)
    {
        HTHEME hHdrTheme = OpenThemeData(hwnd, L"TextStyle");
        auto drawHeaderText = [&](const wchar_t *text, int len, DWORD flags, RECT r, COLORREF color)
        {
            if (!hHdrTheme)
                return;
            DTTOPTS o = {};
            o.dwSize = sizeof(o);
            o.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR | DTT_GLOWSIZE;
            o.crText = RGB(0, 0, 0);
            o.iGlowSize = 6;
            RECT sr = r;
            OffsetRect(&sr, 1, 1);
            DrawThemeTextEx(hHdrTheme, memDC, 0, 0, text, len, flags, &sr, &o);
            o.crText = color;
            o.iGlowSize = 3;
            DrawThemeTextEx(hHdrTheme, memDC, 0, 0, text, len, flags, &r, &o);
        };

        // Glyphs (declared via codepoints; the source is not compiled as UTF-8)
        static const wchar_t kUpArrow[] = {0x2191, 0};   // up arrow (nav up)
        static const wchar_t kSortAsc[] = {0x25B2, 0};   // black up triangle
        static const wchar_t kSortDesc[] = {0x25BC, 0};  // black down triangle

        // Nav "up" button (folder-up arrow) — only when navigated below the root
        if (IsNavBackVisible())
        {
            RECT nb = GetNavBackButtonRect();
            drawHeaderText(kUpArrow, 1, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                           nb, RGB(255, 255, 255));
        }

        // Details column header band
        int headerH = GetDetailsHeaderHeight();
        if (headerH > 0 && (!config.IsRolledUp || isHoverExpanded))
        {
            int headerTop = GetTitleBarHeight(); // flush under the title bar (no gap)
            int headerBottom = headerTop + headerH;

            // Band background (semi-opaque dark, premultiplied)
            const BYTE hdrA = 200, hdrPm = (BYTE)((40 * 200) / 255);
            DWORD hdrBg = (hdrA << 24) | (hdrPm << 16) | (hdrPm << 8) | hdrPm;
            for (int y = headerTop; y < headerBottom && y < h; y++)
                if (y >= 0)
                    for (int x = 0; x < w; x++)
                        pixels[y * w + x] = hdrBg;

            auto cols = GetDetailsColumns();
            int sortCol = GetActiveTab().DetailsSortColumn;
            bool asc = GetActiveTab().DetailsSortAscending;
            DWORD sep = (160 << 24) | (50 << 16) | (50 << 8) | 50;
            for (int i = 0; i < (int)cols.size(); i++)
            {
                RECT lr = {cols[i].left, headerTop, cols[i].right - Dpi(6), headerBottom};
                if (lr.right > lr.left)
                    drawHeaderText(cols[i].label, -1,
                                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
                                   lr, RGB(230, 230, 230));
                if (i == sortCol)
                {
                    RECT gr = {cols[i].left, headerTop, cols[i].right - Dpi(2), headerBottom};
                    drawHeaderText(asc ? kSortAsc : kSortDesc, 1,
                                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                                   gr, RGB(180, 200, 255));
                }
                // Column separator at the right edge
                int sx = cols[i].right;
                for (int y = headerTop + 2; y < headerBottom - 2 && y < h; y++)
                    if (sx >= 0 && sx < w && y >= 0)
                        pixels[y * w + sx] = sep;
            }
        }

        // "Folder unavailable" message when the linked root is gone
        if (virtualFolderMissing && (!config.IsRolledUp || isHoverExpanded))
        {
            RECT mr = {Dpi(12), GetIconAreaTop() + Dpi(8), w - Dpi(12), h - Dpi(8)};
            if (mr.bottom > mr.top)
                drawHeaderText(Tr(Str::Hdr_FolderUnavailable), -1,
                               DT_CENTER | DT_TOP | DT_WORDBREAK | DT_NOPREFIX,
                               mr, RGB(255, 210, 210));
        }

        if (hHdrTheme)
            CloseThemeData(hHdrTheme);
    }

    // Draw icons (skip when rolled up, but show when hover-expanded)
    // Icon opacity applied per-icon only (not whole-window via SourceConstantAlpha)
    BYTE iconAlpha = (BYTE)currentOpacity;

    // Parse tint color and strength
    BYTE tintR = 0, tintG = 0, tintB = 0;
    int tintStrength = currentTintStrength;
    const std::string &tintHex = config.IconTintColor;
    if (!tintHex.empty() && tintHex[0] == '#' && tintHex.length() >= 7)
    {
        unsigned int tintVal;
        sscanf_s(tintHex.c_str() + 1, "%x", &tintVal);
        tintR = (tintVal >> 16) & 0xFF;
        tintG = (tintVal >> 8) & 0xFF;
        tintB = tintVal & 0xFF;
    }
    int tintInv = 255 - tintStrength;

    if (!icons.empty() && (!config.IsRolledUp || isHoverExpanded))
    {
        // Use the system icon title font (DPI-aware, matches desktop icon labels)
        LOGFONTW iconLogFont = {};
        HFONT iconFont;
        if (SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(iconLogFont), &iconLogFont, 0))
        {
            iconLogFont.lfQuality = CLEARTYPE_QUALITY;
            iconFont = CreateFontIndirectW(&iconLogFont);
        }
        else
        {
            iconFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        }
        HFONT oldIconFont = (HFONT)SelectObject(memDC, iconFont);

        // Visible area for clipping (icon area only)
        int visibleTop = GetIconAreaTop();
        int visibleBottom = h;

        /**
         * Icon labels are drawn into their own layer.
         *
         * DrawThemeTextEx writes correct premultiplied alpha but offers no way
         * to ask for less than full opacity — DTTOPTS carries a colour and a
         * glow size, no alpha — so the labels go into a transparent scratch
         * layer that is faded as it is composited. Premultiplied pixels scale by
         * plain multiplication, so at full opacity this reproduces exactly what
         * drawing in place produced, shadow pass included (source-over is
         * associative: fg over shadow over buffer either way).
         *
         * The layer also contains DrawThemeTextEx's habit of ignoring GDI clip
         * regions: the composite bounds do the clipping instead.
         */
        // Brought towards full by the hover fade, in step with the icons.
        const int labelOpacity = ChromeAlpha::HoverBlendTextOpacity(config.IconLabelOpacity,
                                                                    currentTextHover);
        const int labelLayerH = (visibleBottom > visibleTop) ? (visibleBottom - visibleTop) : 0;
        TextLayer labels(screenDC, w, (labelOpacity > 0) ? labelLayerH : 0, TEXT_ALPHA_LAYER_FILL);
        // If the layer could not be allocated, labels fall back to being drawn
        // straight into the back buffer — fully opaque, but visible.
        const bool useLabelLayer = labels.IsValid();
        HDC labelDC = useLabelLayer ? labels.DC() : memDC;
        const int labelOriginY = useLabelLayer ? visibleTop : 0;
        HFONT oldLayerFont = useLabelLayer ? (HFONT)SelectObject(labelDC, iconFont) : nullptr;

        // Create temp DIB for icon rendering (allows proper tint/opacity on modern icons)
        // Modern icons have proper alpha channels - DrawIconEx composites them correctly,
        // but then we can't post-process them. Drawing to a temp buffer first lets us
        // apply tint and opacity before compositing onto the main buffer.
        bool useTempIcon = (iconAlpha < 255 || tintStrength > 0);
        HDC iconTempDC = nullptr;
        HBITMAP iconTempBmp = nullptr;
        HBITMAP iconTempOldBmp = nullptr;
        DWORD *iconTempPixels = nullptr;
        const int ICON_TEMP_SIZE = (iconSize > ICON_TEMP_SIZE_THRESHOLD) ? iconSize : ICON_TEMP_SIZE_THRESHOLD;
        if (useTempIcon)
        {
            BITMAPINFO iconBmi = {};
            iconBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            iconBmi.bmiHeader.biWidth = ICON_TEMP_SIZE;
            iconBmi.bmiHeader.biHeight = -ICON_TEMP_SIZE;
            iconBmi.bmiHeader.biPlanes = 1;
            iconBmi.bmiHeader.biBitCount = 32;
            iconBmi.bmiHeader.biCompression = BI_RGB;
            void *tmpBits = nullptr;
            iconTempDC = CreateCompatibleDC(screenDC);
            iconTempBmp = CreateDIBSection(screenDC, &iconBmi, DIB_RGB_COLORS, &tmpBits, nullptr, 0);
            if (iconTempBmp)
            {
                iconTempOldBmp = (HBITMAP)SelectObject(iconTempDC, iconTempBmp);
                iconTempPixels = (DWORD *)tmpBits;
            }
            else
            {
                DeleteDC(iconTempDC);
                iconTempDC = nullptr;
                useTempIcon = false;
            }
        }

        // Set GDI clipping region to prevent icons from drawing into header
        HRGN clipRegion = CreateRectRgn(0, visibleTop, w, h);
        SelectClipRgn(memDC, clipRegion);

        bool isDetailsView = (GetActiveTab().GetViewMode() == ViewMode::Details);
        std::vector<DetailsColumn> detailCols;
        if (isDetailsView)
            detailCols = GetDetailsColumns();

        // Open theme once for all text rendering (two-pass: shadow + foreground)
        HTHEME hTextTheme = OpenThemeData(hwnd, L"TextStyle");
        auto drawShadowedText = [&](const wchar_t *text, int len, DWORD flags, RECT rect, COLORREF color)
        {
            if (!hTextTheme || labelOpacity == 0)
                return;
            OffsetRect(&rect, 0, -labelOriginY); // window coords -> layer coords
            DTTOPTS dtOpts = {};
            dtOpts.dwSize = sizeof(dtOpts);
            dtOpts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR | DTT_GLOWSIZE;
            // Shadow pass: black text with wide glow for contrast on light backgrounds
            dtOpts.crText = RGB(0, 0, 0);
            dtOpts.iGlowSize = 8;
            RECT shadowRect = rect;
            OffsetRect(&shadowRect, 1, 1);
            DrawThemeTextEx(hTextTheme, labelDC, 0, 0, text, len, flags, &shadowRect, &dtOpts);
            // Foreground pass: actual color with tight glow
            dtOpts.crText = color;
            dtOpts.iGlowSize = 4;
            DrawThemeTextEx(hTextTheme, labelDC, 0, 0, text, len, flags, &rect, &dtOpts);
        };

        for (int i = 0; i < (int)icons.size(); i++)
        {
            const auto &icon = icons[i];

            // Apply scroll offset to icon positions
            int drawTop = icon.rect.top - scrollPosition;
            int drawBottom = icon.rect.bottom - scrollPosition;
            int iconDrawTop = icon.iconRect.top - scrollPosition;

            // Skip if completely outside visible area
            if (drawBottom < visibleTop || drawTop >= visibleBottom)
                continue;

            // Selection highlight (only for grid views, details view handled separately)
            if (IsSelected(i) && !isDraggingIcon && !isDetailsView)
            {
                BYTE selAlpha = SELECTION_ALPHA;
                BYTE selR = SELECTION_R, selG = SELECTION_G, selB = SELECTION_B;
                BYTE selPmR = (BYTE)((selR * selAlpha) / 255);
                BYTE selPmG = (BYTE)((selG * selAlpha) / 255);
                BYTE selPmB = (BYTE)((selB * selAlpha) / 255);
                BYTE invSelAlpha = 255 - selAlpha;

                for (int y = drawTop; y < drawBottom && y < h; y++)
                {
                    if (y < visibleTop)
                        continue; // Clip to visible area
                    for (int x = icon.rect.left; x < icon.rect.right && x < w; x++)
                    {
                        if (x >= 0 && y >= 0)
                        {
                            DWORD dst = pixels[y * w + x];
                            BYTE dstA = (dst >> 24) & 0xFF;
                            BYTE dstR = (dst >> 16) & 0xFF;
                            BYTE dstG = (dst >> 8) & 0xFF;
                            BYTE dstB = dst & 0xFF;
                            BYTE outA = selAlpha + (BYTE)((dstA * invSelAlpha) / 255);
                            BYTE outR = selPmR + (BYTE)((dstR * invSelAlpha) / 255);
                            BYTE outG = selPmG + (BYTE)((dstG * invSelAlpha) / 255);
                            BYTE outB = selPmB + (BYTE)((dstB * invSelAlpha) / 255);
                            pixels[y * w + x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
                        }
                    }
                }
            }

            // Hover highlight (only for grid views, not selected, not dragging)
            if (i == hoveredIcon && !IsSelected(i) && !isDraggingIcon && !isDetailsView)
            {
                BYTE hovAlpha = HOVER_ALPHA;
                BYTE hovPmR = (BYTE)((HOVER_R * hovAlpha) / 255);
                BYTE hovPmG = (BYTE)((HOVER_G * hovAlpha) / 255);
                BYTE hovPmB = (BYTE)((HOVER_B * hovAlpha) / 255);
                BYTE invHovAlpha = 255 - hovAlpha;

                for (int y = drawTop; y < drawBottom && y < h; y++)
                {
                    if (y < visibleTop)
                        continue;
                    for (int x = icon.rect.left; x < icon.rect.right && x < w; x++)
                    {
                        if (x >= 0 && y >= 0)
                        {
                            DWORD dst = pixels[y * w + x];
                            BYTE dstA = (dst >> 24) & 0xFF;
                            BYTE dstR = (dst >> 16) & 0xFF;
                            BYTE dstG = (dst >> 8) & 0xFF;
                            BYTE dstB = dst & 0xFF;
                            BYTE outA = hovAlpha + (BYTE)((dstA * invHovAlpha) / 255);
                            BYTE outR = hovPmR + (BYTE)((dstR * invHovAlpha) / 255);
                            BYTE outG = hovPmG + (BYTE)((dstG * invHovAlpha) / 255);
                            BYTE outB = hovPmB + (BYTE)((dstB * invHovAlpha) / 255);
                            pixels[y * w + x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
                        }
                    }
                }
            }

            // Drop target indicator
            if (isDraggingIcon && i == dropTargetIndex && i != draggedIconIndex)
            {
                DWORD dropPixel = (255 << 24) | (100 << 16) | (200 << 8) | 255;
                int left = icon.rect.left - 2;
                int top = drawTop - 2;
                int right = icon.rect.right + 2;
                int bottom = drawBottom + 2;
                // Draw rectangle border (with clipping)
                for (int x = left; x < right && x < w; x++)
                {
                    if (x >= 0 && top >= visibleTop && top < h)
                        pixels[top * w + x] = dropPixel;
                    if (x >= 0 && bottom - 1 >= visibleTop && bottom - 1 < h)
                        pixels[(bottom - 1) * w + x] = dropPixel;
                }
                for (int y = top; y < bottom && y < h; y++)
                {
                    if (y < visibleTop)
                        continue;
                    if (left >= 0 && y >= 0)
                        pixels[y * w + left] = dropPixel;
                    if (right - 1 >= 0 && right - 1 < w && y >= 0)
                        pixels[y * w + (right - 1)] = dropPixel;
                }
            }

            if (isDetailsView)
            {
                // Details view: icon + name + type + size + date + sync status

                // Draw selection highlight for this row (before drawing content)
                if (IsSelected(i) && !isDraggingIcon)
                {
                    BYTE selAlpha = 200;
                    BYTE selR = 60, selG = 120, selB = 200;
                    BYTE selPmR = (BYTE)((selR * selAlpha) / 255);
                    BYTE selPmG = (BYTE)((selG * selAlpha) / 255);
                    BYTE selPmB = (BYTE)((selB * selAlpha) / 255);
                    BYTE invSelAlpha = 255 - selAlpha;

                    for (int y = drawTop; y < drawBottom && y < h; y++)
                    {
                        if (y < visibleTop)
                            continue;
                        for (int x = icon.rect.left; x < icon.rect.right && x < w; x++)
                        {
                            if (x >= 0 && y >= 0)
                            {
                                DWORD dst = pixels[y * w + x];
                                BYTE dstA = (dst >> 24) & 0xFF;
                                BYTE dstR = (dst >> 16) & 0xFF;
                                BYTE dstG = (dst >> 8) & 0xFF;
                                BYTE dstB = dst & 0xFF;
                                BYTE outA = selAlpha + (BYTE)((dstA * invSelAlpha) / 255);
                                BYTE outR = selPmR + (BYTE)((dstR * invSelAlpha) / 255);
                                BYTE outG = selPmG + (BYTE)((dstG * invSelAlpha) / 255);
                                BYTE outB = selPmB + (BYTE)((dstB * invSelAlpha) / 255);
                                pixels[y * w + x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
                            }
                        }
                    }
                }
                // Hover highlight for this row (before drawing content)
                else if (i == hoveredIcon && !isDraggingIcon)
                {
                    BYTE hovAlpha = HOVER_ALPHA;
                    BYTE hovPmR = (BYTE)((HOVER_R * hovAlpha) / 255);
                    BYTE hovPmG = (BYTE)((HOVER_G * hovAlpha) / 255);
                    BYTE hovPmB = (BYTE)((HOVER_B * hovAlpha) / 255);
                    BYTE invHovAlpha = 255 - hovAlpha;

                    for (int y = drawTop; y < drawBottom && y < h; y++)
                    {
                        if (y < visibleTop)
                            continue;
                        for (int x = icon.rect.left; x < icon.rect.right && x < w; x++)
                        {
                            if (x >= 0 && y >= 0)
                            {
                                DWORD dst = pixels[y * w + x];
                                BYTE dstA = (dst >> 24) & 0xFF;
                                BYTE dstR = (dst >> 16) & 0xFF;
                                BYTE dstG = (dst >> 8) & 0xFF;
                                BYTE dstB = dst & 0xFF;
                                BYTE outA = hovAlpha + (BYTE)((dstA * invHovAlpha) / 255);
                                BYTE outR = hovPmR + (BYTE)((dstR * invHovAlpha) / 255);
                                BYTE outG = hovPmG + (BYTE)((dstG * invHovAlpha) / 255);
                                BYTE outB = hovPmB + (BYTE)((dstB * invHovAlpha) / 255);
                                pixels[y * w + x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
                            }
                        }
                    }
                }

                int currentIconSize = Dpi(ICON_SIZE_DETAILS);
                HICON hIconToDraw = icon.hIconSmall ? icon.hIconSmall : icon.hIcon;

                // Draw small icon
                if (hIconToDraw)
                {
                    if (useTempIcon && iconTempPixels)
                    {
                        // Draw to temp DIB, apply tint+opacity, composite onto main buffer
                        memset(iconTempPixels, 0, ICON_TEMP_SIZE * ICON_TEMP_SIZE * 4);
                        DrawIconEx(iconTempDC, 0, 0, hIconToDraw, currentIconSize, currentIconSize, 0, nullptr, DI_NORMAL);
                        for (int py = 0; py < currentIconSize; py++)
                        {
                            int destY = iconDrawTop + py;
                            if (destY < visibleTop || destY >= h)
                                continue;
                            for (int px = 0; px < currentIconSize; px++)
                            {
                                int destX = icon.iconRect.left + px;
                                if (destX < 0 || destX >= w)
                                    continue;
                                DWORD srcPx = iconTempPixels[py * ICON_TEMP_SIZE + px];
                                BYTE srcA = (srcPx >> 24) & 0xFF;
                                BYTE srcR = (srcPx >> 16) & 0xFF;
                                BYTE srcG = (srcPx >> 8) & 0xFF;
                                BYTE srcB = srcPx & 0xFF;
                                if (srcA == 0 && srcR == 0 && srcG == 0 && srcB == 0)
                                    continue;
                                if (srcA == 0)
                                    srcA = 255; // Old-style icon: GDI zeroed alpha
                                else if (srcA < 255)
                                { // Un-premultiply modern icon
                                    int uR = (srcR * 255 + srcA / 2) / srcA;
                                    srcR = (BYTE)(uR > 255 ? 255 : uR);
                                    int uG = (srcG * 255 + srcA / 2) / srcA;
                                    srcG = (BYTE)(uG > 255 ? 255 : uG);
                                    int uB = (srcB * 255 + srcA / 2) / srcA;
                                    srcB = (BYTE)(uB > 255 ? 255 : uB);
                                }
                                if (tintStrength > 0)
                                {
                                    srcR = (BYTE)((srcR * tintInv + tintR * tintStrength) / 255);
                                    srcG = (BYTE)((srcG * tintInv + tintG * tintStrength) / 255);
                                    srcB = (BYTE)((srcB * tintInv + tintB * tintStrength) / 255);
                                }
                                BYTE finalA = (BYTE)((srcA * iconAlpha) / 255);
                                if (finalA == 0)
                                    continue;
                                BYTE pmR = (BYTE)((srcR * finalA) / 255);
                                BYTE pmG = (BYTE)((srcG * finalA) / 255);
                                BYTE pmB = (BYTE)((srcB * finalA) / 255);
                                DWORD dstPx = pixels[destY * w + destX];
                                BYTE invFA = 255 - finalA;
                                pixels[destY * w + destX] =
                                    ((BYTE)(finalA + (((dstPx >> 24) & 0xFF) * invFA) / 255) << 24) |
                                    ((BYTE)(pmR + (((dstPx >> 16) & 0xFF) * invFA) / 255) << 16) |
                                    ((BYTE)(pmG + (((dstPx >> 8) & 0xFF) * invFA) / 255) << 8) |
                                    (BYTE)(pmB + ((dstPx & 0xFF) * invFA) / 255);
                            }
                        }
                    }
                    else
                    {
                        DrawIconEx(memDC, icon.iconRect.left, iconDrawTop, hIconToDraw,
                                   currentIconSize, currentIconSize, 0, nullptr, DI_NORMAL);
                        // Fix alpha for old-style icons (no tint/opacity needed in this path)
                        for (int py = iconDrawTop; py < iconDrawTop + currentIconSize && py < h; py++)
                        {
                            if (py < visibleTop)
                                continue;
                            for (int px = icon.iconRect.left; px < icon.iconRect.left + currentIconSize && px < w; px++)
                            {
                                if (px >= 0 && py >= 0)
                                {
                                    DWORD pixel = pixels[py * w + px];
                                    if ((pixel >> 24) == 0 && (pixel & 0xFFFFFF) != 0)
                                    {
                                        BYTE r = (pixel >> 16) & 0xFF, g = (pixel >> 8) & 0xFF, b = pixel & 0xFF;
                                        pixels[py * w + px] = (255 << 24) | (r << 16) | (g << 8) | b;
                                    }
                                }
                            }
                        }
                    }
                }

                // Column layout for details view — single source of truth (resizable widths)
                // Columns: Name | Type | Size | Date | Sync (fixed trailing indicator)
                int nameCol = detailCols[0].left;
                int typeCol = detailCols[1].left;
                int sizeCol = detailCols[2].left;
                int dateCol = detailCols[3].left;
                int syncCol = detailCols[3].right;

                // Draw detail columns (DrawThemeTextEx ignores clip regions, so guard visibility)
                if (drawTop >= visibleTop && drawBottom <= visibleBottom)
                {
                    // Draw name (bright white)
                    RECT nameRect = {nameCol, drawTop, typeCol - 4, drawBottom};
                    drawShadowedText(icon.displayName.c_str(), (int)icon.displayName.length(),
                                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
                                     nameRect, RGB(255, 255, 255));

                    // Draw type
                    RECT typeRect = {typeCol, drawTop, sizeCol - 4, drawBottom};
                    drawShadowedText(icon.fileType.c_str(), (int)icon.fileType.length(),
                                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX,
                                     typeRect, RGB(255, 255, 255));

                    // Draw size
                    std::wstring sizeStr;
                    if (icon.fileSize < 1024)
                    {
                        sizeStr = std::to_wstring(icon.fileSize) + Tr(Str::Unit_Bytes);
                    }
                    else if (icon.fileSize < 1024 * 1024)
                    {
                        sizeStr = std::to_wstring(icon.fileSize / 1024) + Tr(Str::Unit_KB);
                    }
                    else if (icon.fileSize < 1024 * 1024 * 1024)
                    {
                        sizeStr = std::to_wstring(icon.fileSize / (1024 * 1024)) + Tr(Str::Unit_MB);
                    }
                    else
                    {
                        sizeStr = std::to_wstring(icon.fileSize / (1024 * 1024 * 1024)) + Tr(Str::Unit_GB);
                    }
                    RECT sizeRect = {sizeCol, drawTop, dateCol - 4, drawBottom};
                    drawShadowedText(sizeStr.c_str(), (int)sizeStr.length(),
                                     DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                                     sizeRect, RGB(255, 255, 255));

                    // Draw date
                    FILETIME localTime;
                    SYSTEMTIME sysTime;
                    FileTimeToLocalFileTime(&icon.modifiedTime, &localTime);
                    FileTimeToSystemTime(&localTime, &sysTime);
                    wchar_t dateStr[32];
                    swprintf_s(dateStr, L"%02d/%02d/%04d", sysTime.wMonth, sysTime.wDay, sysTime.wYear);
                    RECT dateRect = {dateCol, drawTop, syncCol - 4, drawBottom};
                    drawShadowedText(dateStr, -1,
                                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                                     dateRect, RGB(255, 255, 255));

                    // Draw sync status indicator
                    if (icon.syncStatus != SyncStatus::None)
                    {
                        const wchar_t *syncSymbol = L"";
                        COLORREF syncColor = RGB(180, 180, 180);
                        switch (icon.syncStatus)
                        {
                        case SyncStatus::Synced:
                            syncSymbol = L"\u2713";         // Check mark
                            syncColor = RGB(100, 200, 100); // Green
                            break;
                        case SyncStatus::Syncing:
                            syncSymbol = L"\u21BB";         // Circular arrows
                            syncColor = RGB(100, 150, 255); // Blue
                            break;
                        case SyncStatus::Pending:
                            syncSymbol = L"\u23F1";         // Stopwatch/clock
                            syncColor = RGB(100, 150, 255); // Blue
                            break;
                        case SyncStatus::Error:
                            syncSymbol = L"\u2717";         // X mark
                            syncColor = RGB(255, 100, 100); // Red
                            break;
                        case SyncStatus::CloudOnly:
                            syncSymbol = L"\u2601";         // Cloud
                            syncColor = RGB(150, 150, 255); // Light blue
                            break;
                        default:
                            break;
                        }
                        RECT syncRect = {syncCol, drawTop, icon.rect.right - 4, drawBottom};
                        drawShadowedText(syncSymbol, -1,
                                         DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                                         syncRect, syncColor);
                    }
                }

                // Draw selection border for selected row in details view
                if (IsSelected(i) && !isDraggingIcon)
                {
                    DWORD borderPixel = (255 << 24) | (100 << 16) | (150 << 8) | 255;
                    // Top border
                    for (int x = icon.rect.left; x < icon.rect.right && x < w; x++)
                    {
                        if (x >= 0 && drawTop >= visibleTop && drawTop < h)
                        {
                            pixels[drawTop * w + x] = borderPixel;
                        }
                    }
                    // Bottom border
                    for (int x = icon.rect.left; x < icon.rect.right && x < w; x++)
                    {
                        if (x >= 0 && (drawBottom - 1) >= visibleTop && (drawBottom - 1) < h)
                        {
                            pixels[(drawBottom - 1) * w + x] = borderPixel;
                        }
                    }
                }
            }
            else
            {
                // Grid view (Small/Medium/Large icons)
                // Icon image
                if (icon.hIcon)
                {
                    if (useTempIcon && iconTempPixels)
                    {
                        // Draw to temp DIB, apply tint+opacity, composite onto main buffer
                        memset(iconTempPixels, 0, ICON_TEMP_SIZE * ICON_TEMP_SIZE * 4);
                        DrawIconEx(iconTempDC, 0, 0, icon.hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
                        for (int py = 0; py < iconSize; py++)
                        {
                            int destY = iconDrawTop + py;
                            if (destY < visibleTop || destY >= h)
                                continue;
                            for (int px = 0; px < iconSize; px++)
                            {
                                int destX = icon.iconRect.left + px;
                                if (destX < 0 || destX >= w)
                                    continue;
                                DWORD srcPx = iconTempPixels[py * ICON_TEMP_SIZE + px];
                                BYTE srcA = (srcPx >> 24) & 0xFF;
                                BYTE srcR = (srcPx >> 16) & 0xFF;
                                BYTE srcG = (srcPx >> 8) & 0xFF;
                                BYTE srcB = srcPx & 0xFF;
                                if (srcA == 0 && srcR == 0 && srcG == 0 && srcB == 0)
                                    continue;
                                if (srcA == 0)
                                    srcA = 255; // Old-style icon: GDI zeroed alpha
                                else if (srcA < 255)
                                { // Un-premultiply modern icon
                                    int uR = (srcR * 255 + srcA / 2) / srcA;
                                    srcR = (BYTE)(uR > 255 ? 255 : uR);
                                    int uG = (srcG * 255 + srcA / 2) / srcA;
                                    srcG = (BYTE)(uG > 255 ? 255 : uG);
                                    int uB = (srcB * 255 + srcA / 2) / srcA;
                                    srcB = (BYTE)(uB > 255 ? 255 : uB);
                                }
                                if (tintStrength > 0)
                                {
                                    srcR = (BYTE)((srcR * tintInv + tintR * tintStrength) / 255);
                                    srcG = (BYTE)((srcG * tintInv + tintG * tintStrength) / 255);
                                    srcB = (BYTE)((srcB * tintInv + tintB * tintStrength) / 255);
                                }
                                BYTE finalA = (BYTE)((srcA * iconAlpha) / 255);
                                if (finalA == 0)
                                    continue;
                                BYTE pmR = (BYTE)((srcR * finalA) / 255);
                                BYTE pmG = (BYTE)((srcG * finalA) / 255);
                                BYTE pmB = (BYTE)((srcB * finalA) / 255);
                                DWORD dstPx = pixels[destY * w + destX];
                                BYTE invFA = 255 - finalA;
                                pixels[destY * w + destX] =
                                    ((BYTE)(finalA + (((dstPx >> 24) & 0xFF) * invFA) / 255) << 24) |
                                    ((BYTE)(pmR + (((dstPx >> 16) & 0xFF) * invFA) / 255) << 16) |
                                    ((BYTE)(pmG + (((dstPx >> 8) & 0xFF) * invFA) / 255) << 8) |
                                    (BYTE)(pmB + ((dstPx & 0xFF) * invFA) / 255);
                            }
                        }
                    }
                    else
                    {
                        DrawIconEx(memDC, icon.iconRect.left, iconDrawTop, icon.hIcon,
                                   iconSize, iconSize, 0, nullptr, DI_NORMAL);
                        // Fix alpha for old-style icons (no tint/opacity needed in this path)
                        for (int py = iconDrawTop; py < iconDrawTop + iconSize && py < h; py++)
                        {
                            if (py < visibleTop)
                                continue;
                            for (int px = icon.iconRect.left; px < icon.iconRect.left + iconSize && px < w; px++)
                            {
                                if (px >= 0 && py >= 0)
                                {
                                    DWORD pixel = pixels[py * w + px];
                                    if ((pixel >> 24) == 0 && (pixel & 0xFFFFFF) != 0)
                                    {
                                        BYTE r = (pixel >> 16) & 0xFF, g = (pixel >> 8) & 0xFF, b = pixel & 0xFF;
                                        pixels[py * w + px] = (255 << 24) | (r << 16) | (g << 8) | b;
                                    }
                                }
                            }
                        }
                    }
                }

                // Label with glow shadow (matches Windows desktop style)
                int labelTop = iconDrawTop + iconSize + Dpi(2);
                RECT labelRect = {
                    icon.rect.left,
                    labelTop,
                    icon.rect.right,
                    drawBottom};
                // Only draw if label area is fully within visible region.
                // DrawThemeTextEx with DTT_COMPOSITED ignores GDI clip regions,
                // so we skip the label entirely when it would overflow into the title bar
                // (clamping the rect causes the label to visually "stick" at the border).
                if (labelTop >= visibleTop && labelTop < visibleBottom && drawBottom > visibleTop)
                {
                    drawShadowedText(icon.displayName.c_str(), (int)icon.displayName.length(),
                                     DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX,
                                     labelRect, RGB(255, 255, 255));
                }
            }
        }

        // Fade the finished label layer onto the back buffer.
        if (useLabelLayer && labelOpacity > 0)
        {
            const DWORD *lay = labels.Pixels();
            for (int y = 0; y < labels.Height(); y++)
            {
                const int destY = y + visibleTop;
                if (destY < 0 || destY >= h)
                    continue;
                for (int x = 0; x < w; x++)
                {
                    DWORD glyph = lay[y * w + x];
                    if (((glyph >> 24) & 0xFF) == 0)
                        continue;
                    pixels[destY * w + x] = ChromeAlpha::PremultipliedOver(
                        ChromeAlpha::ScalePremultiplied(glyph, labelOpacity),
                        pixels[destY * w + x]);
                }
            }
        }
        if (oldLayerFont)
            SelectObject(labelDC, oldLayerFont);

        // Clean up temp icon DIB
        if (iconTempDC)
        {
            SelectObject(iconTempDC, iconTempOldBmp);
            DeleteObject(iconTempBmp);
            DeleteDC(iconTempDC);
        }

        // Clean up text theme
        if (hTextTheme)
        {
            CloseThemeData(hTextTheme);
        }

        // Remove clipping region
        SelectClipRgn(memDC, nullptr);
        DeleteObject(clipRegion);

        SelectObject(memDC, oldIconFont);
        DeleteObject(iconFont);
    }

    // Rubber-band selection rectangle (tinted fill + solid outline)
    if (isRubberBanding)
    {
        RECT band = GetRubberBandRect();
        band.top -= scrollPosition;
        band.bottom -= scrollPosition;
        int bandTop = (std::max)((int)band.top, GetIconAreaTop());
        int bandBottom = (std::min)((int)band.bottom, h);
        int bandLeft = (std::max)((int)band.left, 0);
        int bandRight = (std::min)((int)band.right, w);

        const BYTE fillAlpha = 60;
        DWORD outlinePixel = (255u << 24) | (120u << 16) | (170u << 8) | 230u;

        for (int y = bandTop; y < bandBottom; y++)
        {
            for (int x = bandLeft; x < bandRight; x++)
            {
                bool edge = (y == bandTop || y == bandBottom - 1 || x == bandLeft || x == bandRight - 1);
                if (edge)
                {
                    pixels[y * w + x] = outlinePixel;
                    continue;
                }
                DWORD dst = pixels[y * w + x];
                BYTE inv = 255 - fillAlpha;
                BYTE outA = fillAlpha + (BYTE)((((dst >> 24) & 0xFF) * inv) / 255);
                BYTE outR = (BYTE)((120 * fillAlpha) / 255) + (BYTE)((((dst >> 16) & 0xFF) * inv) / 255);
                BYTE outG = (BYTE)((170 * fillAlpha) / 255) + (BYTE)((((dst >> 8) & 0xFF) * inv) / 255);
                BYTE outB = (BYTE)((230 * fillAlpha) / 255) + (BYTE)(((dst & 0xFF) * inv) / 255);
                pixels[y * w + x] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
            }
        }
    }

    // Draw PowerShell-style scrollbar (only when needed, blends with appearance)
    if (NeedsScrollbar() && (!config.IsRolledUp || isHoverExpanded))
    {
        RECT thumb = GetScrollbarThumbRect();

        if (!isScrollbarHovered)
        {
            // Draw narrow grey indicator (PowerShell minimal mode)
            int narrowWidth = Dpi(SCROLLBAR_NARROW_WIDTH);
            int indicatorLeft = w - narrowWidth - Dpi(SCROLLBAR_MARGIN);

            // Grey color for indicator (RGB: 128, 128, 128 with some alpha)
            BYTE indAlpha = 180;
            BYTE indR = 128, indG = 128, indB = 128;
            BYTE indPmR = (BYTE)((indR * indAlpha) / 255);
            BYTE indPmG = (BYTE)((indG * indAlpha) / 255);
            BYTE indPmB = (BYTE)((indB * indAlpha) / 255);
            DWORD indicatorPixel = (indAlpha << 24) | (indPmR << 16) | (indPmG << 8) | indPmB;

            // Draw narrow thumb indicator
            for (int y = thumb.top; y < thumb.bottom && y < h; y++)
            {
                for (int x = indicatorLeft; x < indicatorLeft + narrowWidth && x < w; x++)
                {
                    if (x >= 0 && y >= 0)
                    {
                        pixels[y * w + x] = indicatorPixel;
                    }
                }
            }
        }
        else
        {
            // Full scrollbar on hover (PowerShell expanded mode)
            RECT track = GetScrollbarTrackRect();
            int arrowSize = Dpi(SCROLLBAR_ARROW_SIZE);

            // Background: corral color but lighter ("heller")
            // Increase RGB components by ~30% to make it lighter
            BYTE trackR = (BYTE)(std::min(255, bgR + (255 - bgR) * 30 / 100));
            BYTE trackG = (BYTE)(std::min(255, bgG + (255 - bgG) * 30 / 100));
            BYTE trackB = (BYTE)(std::min(255, bgB + (255 - bgB) * 30 / 100));
            BYTE trackAlpha = bgAlpha;
            BYTE trackPmR = (BYTE)((trackR * trackAlpha) / 255);
            BYTE trackPmG = (BYTE)((trackG * trackAlpha) / 255);
            BYTE trackPmB = (BYTE)((trackB * trackAlpha) / 255);
            DWORD trackPixel = (trackAlpha << 24) | (trackPmR << 16) | (trackPmG << 8) | trackPmB;

            // Draw track background
            for (int y = track.top; y < track.bottom && y < h; y++)
            {
                for (int x = track.left; x < track.right && x < w; x++)
                {
                    if (x >= 0 && y >= 0)
                    {
                        pixels[y * w + x] = trackPixel;
                    }
                }
            }

            // Draw arrow buttons (top and bottom)
            RECT topArrow = {track.left, track.top, track.right, track.top + arrowSize};
            RECT bottomArrow = {track.left, track.bottom - arrowSize, track.right, track.bottom};

            // Arrows use slightly darker color
            BYTE arrowAlpha = 200;
            BYTE arrowR = 100, arrowG = 100, arrowB = 100;
            BYTE arrowPmR = (BYTE)((arrowR * arrowAlpha) / 255);
            BYTE arrowPmG = (BYTE)((arrowG * arrowAlpha) / 255);
            BYTE arrowPmB = (BYTE)((arrowB * arrowAlpha) / 255);
            DWORD arrowPixel = (arrowAlpha << 24) | (arrowPmR << 16) | (arrowPmG << 8) | arrowPmB;

            // Draw arrow backgrounds
            for (int y = topArrow.top; y < topArrow.bottom && y < h; y++)
            {
                for (int x = topArrow.left; x < topArrow.right && x < w; x++)
                {
                    if (x >= 0 && y >= 0)
                    {
                        pixels[y * w + x] = arrowPixel;
                    }
                }
            }
            for (int y = bottomArrow.top; y < bottomArrow.bottom && y < h; y++)
            {
                for (int x = bottomArrow.left; x < bottomArrow.right && x < w; x++)
                {
                    if (x >= 0 && y >= 0)
                    {
                        pixels[y * w + x] = arrowPixel;
                    }
                }
            }

            // Draw arrow triangles
            int arrowCenterX = (topArrow.left + topArrow.right) / 2;
            int arrowCenterY_top = (topArrow.top + topArrow.bottom) / 2;
            int arrowCenterY_bottom = (bottomArrow.top + bottomArrow.bottom) / 2;

            BYTE arrowTriAlpha = 255;
            BYTE arrowTriR = 255, arrowTriG = 255, arrowTriB = 255;
            BYTE arrowTriPmR = (BYTE)((arrowTriR * arrowTriAlpha) / 255);
            BYTE arrowTriPmG = (BYTE)((arrowTriG * arrowTriAlpha) / 255);
            BYTE arrowTriPmB = (BYTE)((arrowTriB * arrowTriAlpha) / 255);
            DWORD arrowTriPixel = (arrowTriAlpha << 24) | (arrowTriPmR << 16) | (arrowTriPmG << 8) | arrowTriPmB;

            // Simple triangle drawing (up arrow)
            for (int dy = -2; dy <= 2; dy++)
            {
                int width = 2 - abs(dy);
                for (int dx = -width; dx <= width; dx++)
                {
                    int x = arrowCenterX + dx;
                    int y = arrowCenterY_top - dy;
                    if (x >= 0 && x < w && y >= 0 && y < h)
                    {
                        pixels[y * w + x] = arrowTriPixel;
                    }
                }
            }

            // Simple triangle drawing (down arrow)
            for (int dy = -2; dy <= 2; dy++)
            {
                int width = 2 - abs(dy);
                for (int dx = -width; dx <= width; dx++)
                {
                    int x = arrowCenterX + dx;
                    int y = arrowCenterY_bottom + dy;
                    if (x >= 0 && x < w && y >= 0 && y < h)
                    {
                        pixels[y * w + x] = arrowTriPixel;
                    }
                }
            }

            // Draw thumb (grey, matching PowerShell style)
            BYTE thumbAlpha = 200;
            BYTE thumbR = 128, thumbG = 128, thumbB = 128;
            BYTE thumbPmR = (BYTE)((thumbR * thumbAlpha) / 255);
            BYTE thumbPmG = (BYTE)((thumbG * thumbAlpha) / 255);
            BYTE thumbPmB = (BYTE)((thumbB * thumbAlpha) / 255);
            DWORD thumbPixel = (thumbAlpha << 24) | (thumbPmR << 16) | (thumbPmG << 8) | thumbPmB;

            // Adjust thumb to avoid arrow areas
            RECT adjustedThumb = thumb;
            adjustedThumb.top = std::max(adjustedThumb.top, track.top + arrowSize);
            adjustedThumb.bottom = std::min(adjustedThumb.bottom, track.bottom - arrowSize);

            for (int y = adjustedThumb.top; y < adjustedThumb.bottom && y < h; y++)
            {
                for (int x = adjustedThumb.left; x < adjustedThumb.right && x < w; x++)
                {
                    if (x >= 0 && y >= 0)
                    {
                        pixels[y * w + x] = thumbPixel;
                    }
                }
            }
        }
    }

    // Resize grip (bottom-right corner, skip when rolled up).
    // Deliberately unaffected by BorderOpacity: the grip is the corral's resize
    // affordance, not part of the frame. A frameless corral (border at 0) still
    // needs somewhere visible to grab, so the grip stays at full strength while
    // the frame around it fades away. Confirmed as wanted in UAT of #1.
    if (!config.IsRolledUp)
    {
        DWORD gripPixel = (255 << 24) | (150 << 16) | (150 << 8) | 150;
        for (int i = 0; i < 8; i++)
        {
            int x = w - 2;
            int y = h - 10 + i;
            if (x >= 0 && x < w && y >= 0 && y < h)
            {
                pixels[y * w + x] = gripPixel;
            }
        }
        for (int i = 0; i < 8; i++)
        {
            int x = w - 10 + i;
            int y = h - 2;
            if (x >= 0 && x < w && y >= 0 && y < h)
            {
                pixels[y * w + x] = gripPixel;
            }
        }
    }

    // Update the layered window
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {w, h};
    POINT ptDst = {rect.left, rect.top};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    // Whole-window alpha is 255 except during a quick-hide fade
    blend.SourceConstantAlpha = (BYTE)quickHideAlpha;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, &ptDst, &sizeWnd, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}
