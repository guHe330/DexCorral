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
 * Manages color compositing, icon rendering, hover/selection highlights, and the alpha
 * fix-up mechanism necessary because GDI draws with alpha=0.
 */

#include "CorralWindow.h"
#include "Constants.h"
#include "App.h"
#include <windowsx.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <cstdio>
#include <algorithm>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "uxtheme.lib")

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
            // Active tab: bright, near-opaque
            BYTE activeAlpha = 240;
            BYTE pmR = (BYTE)((tabR * activeAlpha) / 255);
            BYTE pmG = (BYTE)((tabG * activeAlpha) / 255);
            BYTE pmB = (BYTE)((tabB * activeAlpha) / 255);
            tabPixels[i] = (activeAlpha << 24) | (pmR << 16) | (pmG << 8) | pmB;
        }
        else
        {
            // Inactive tab: darker, more transparent
            BYTE inactiveAlpha = TITLE_TEXT_ALPHA;
            BYTE pmR = (BYTE)((tabR * inactiveAlpha) / 255 / 2);
            BYTE pmG = (BYTE)((tabG * inactiveAlpha) / 255 / 2);
            BYTE pmB = (BYTE)((tabB * inactiveAlpha) / 255 / 2);
            tabPixels[i] = (inactiveAlpha << 24) | (pmR << 16) | (pmG << 8) | pmB;
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

    // Draw border (1px solid line at full opacity)
    DWORD borderPixel = CORRAL_BORDER_COLOR;
    // Top edge
    for (int x = 0; x < w; x++)
        pixels[x] = borderPixel;
    // Bottom edge
    for (int x = 0; x < w; x++)
        pixels[(h - 1) * w + x] = borderPixel;
    // Left edge
    for (int y = 0; y < h; y++)
        pixels[y * w] = borderPixel;
    // Right edge
    for (int y = 0; y < h; y++)
        pixels[y * w + (w - 1)] = borderPixel;

    // Now use GDI to draw content (text, icons) on top
    // GDI doesn't handle alpha properly, so we draw and then fix alpha

    SetBkMode(memDC, TRANSPARENT);

    // Draw tab titles — each tab uses its own font settings
    HFONT oldFont = nullptr;
    HFONT lastFont = nullptr;

    for (int i = 0; i < (int)config.Tabs.size(); i++)
    {
        const CorralTabConfig &tab = config.Tabs[i];

        // Parse this tab's header font color
        BYTE fontR = 255, fontG = 255, fontB = 255;
        const std::string &fontColorHex = tab.HeaderFontColor;
        if (!fontColorHex.empty() && fontColorHex[0] == '#' && fontColorHex.length() >= 7)
        {
            unsigned int fontColorVal;
            sscanf_s(fontColorHex.c_str() + 1, "%x", &fontColorVal);
            fontR = (fontColorVal >> 16) & 0xFF;
            fontG = (fontColorVal >> 8) & 0xFF;
            fontB = fontColorVal & 0xFF;
        }
        SetTextColor(memDC, RGB(fontR, fontG, fontB));

        // Create this tab's font
        std::wstring fontNameW = Utf8ToWide(tab.HeaderFontName);
        int fontHeight = -MulDiv(tab.HeaderFontSize, GetDpiForWindow(hwnd), 72);
        HFONT titleFont = CreateFontW(fontHeight, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, fontNameW.c_str());
        HFONT prevFont = (HFONT)SelectObject(memDC, titleFont);
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
            wtitle = L"\u2B07 " + wtitle; // Down arrow
        }

        RECT tabRect = GetTabRect(i);
        tabRect.left += 6;
        tabRect.right -= 6;
        tabRect.top += 2;

        DrawTextW(memDC, wtitle.c_str(), (int)wtitle.length(), &tabRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

        if (lastFont)
            DeleteObject(lastFont);
        lastFont = titleFont;
    }

    if (oldFont)
        SelectObject(memDC, oldFont);
    if (lastFont)
        DeleteObject(lastFont);

    /**
     * Alpha channel fix-up for GDI-drawn title/tab area.
     *
     * Problem: GDI (DrawTextW) renders with alpha=0, which makes the text invisible
     * in our 32-bit DIB with per-pixel alpha blending. This fix-up loop restores the
     * alpha channel for all pixels that GDI modified:
     *   - If GDI drew colored text (alpha=0 but RGB non-zero): set alpha to 255 (opaque)
     *   - If GDI cleared a pixel (alpha=0 and RGB=0): restore the background tab color
     * This allows layered window compositing to show the text correctly while maintaining
     * per-pixel transparency for the rest of the window.
     */
    for (int y = 0; y < GetTitleBarHeight() && y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            DWORD pixel = pixels[y * w + x];
            BYTE a = (pixel >> 24) & 0xFF;
            BYTE r = (pixel >> 16) & 0xFF;
            BYTE g = (pixel >> 8) & 0xFF;
            BYTE b = pixel & 0xFF;
            // If GDI drew here (alpha is 0 but has color), make it fully opaque
            if (a == 0 && (r > 0 || g > 0 || b > 0))
            {
                pixels[y * w + x] = (255 << 24) | (r << 16) | (g << 8) | b;
            }
            else if (a == 0)
            {
                // Restore tab background for pixels GDI cleared
                int tabIndex = -1;
                for (int ti = 0; ti < (int)config.Tabs.size(); ti++)
                {
                    RECT tr = GetTabRect(ti);
                    if (x >= tr.left && x < tr.right)
                    {
                        tabIndex = ti;
                        break;
                    }
                }
                pixels[y * w + x] = (tabIndex >= 0) ? tabPixels[tabIndex] : 0;
            }
        }
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

        // Open theme once for all text rendering (two-pass: shadow + foreground)
        HTHEME hTextTheme = OpenThemeData(hwnd, L"TextStyle");
        auto drawShadowedText = [&](const wchar_t *text, int len, DWORD flags, RECT rect, COLORREF color)
        {
            if (!hTextTheme)
                return;
            DTTOPTS dtOpts = {};
            dtOpts.dwSize = sizeof(dtOpts);
            dtOpts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR | DTT_GLOWSIZE;
            // Shadow pass: black text with wide glow for contrast on light backgrounds
            dtOpts.crText = RGB(0, 0, 0);
            dtOpts.iGlowSize = 8;
            RECT shadowRect = rect;
            OffsetRect(&shadowRect, 1, 1);
            DrawThemeTextEx(hTextTheme, memDC, 0, 0, text, len, flags, &shadowRect, &dtOpts);
            // Foreground pass: actual color with tight glow
            dtOpts.crText = color;
            dtOpts.iGlowSize = 4;
            DrawThemeTextEx(hTextTheme, memDC, 0, 0, text, len, flags, &rect, &dtOpts);
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
            if (i == selectedIcon && !isDraggingIcon && !isDetailsView)
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
            if (i == hoveredIcon && i != selectedIcon && !isDraggingIcon && !isDetailsView)
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
                if (i == selectedIcon && !isDraggingIcon)
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

                // Column layout for details view
                // Columns: Name (40%) | Type (20%) | Size (15%) | Date (15%) | Sync (10%)
                int contentWidth = icon.rect.right - icon.rect.left - Dpi(ICON_SIZE_DETAILS) - Dpi(8);
                int nameCol = icon.iconRect.left + Dpi(ICON_SIZE_DETAILS) + Dpi(4);
                int typeCol = nameCol + (int)(contentWidth * 0.40);
                int sizeCol = typeCol + (int)(contentWidth * 0.20);
                int dateCol = sizeCol + (int)(contentWidth * 0.15);
                int syncCol = dateCol + (int)(contentWidth * 0.15);

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
                        sizeStr = std::to_wstring(icon.fileSize) + L" B";
                    }
                    else if (icon.fileSize < 1024 * 1024)
                    {
                        sizeStr = std::to_wstring(icon.fileSize / 1024) + L" KB";
                    }
                    else if (icon.fileSize < 1024 * 1024 * 1024)
                    {
                        sizeStr = std::to_wstring(icon.fileSize / (1024 * 1024)) + L" MB";
                    }
                    else
                    {
                        sizeStr = std::to_wstring(icon.fileSize / (1024 * 1024 * 1024)) + L" GB";
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
                if (i == selectedIcon && !isDraggingIcon)
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

    // Resize grip (bottom-right corner, skip when rolled up)
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
