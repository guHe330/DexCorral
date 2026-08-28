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

#pragma once
#include <windows.h>

///
/// TextLayer.h - A scratch 32-bit DIB to draw text into before compositing it.
///
/// Text opacity is not something either text engine we use can be asked for:
/// GDI's DrawTextW writes no alpha at all, and DrawThemeTextEx's DTTOPTS has no
/// alpha field. Both are therefore drawn into a throwaway layer of the same
/// pixel format as the corral's back buffer, and the layer is faded as it is
/// composited on top. That also confines DrawThemeTextEx's habit of ignoring
/// GDI clip regions: the composite bounds do the clipping instead.
///
/// The layer owns its DC and bitmap and releases them in the destructor, so the
/// early-out paths in the render function cannot leak them.
///
/// Fill value for a coverage layer: white with no alpha. GDI text drawn onto it
/// in black leaves darkness == glyph coverage, which becomes the text's alpha.
constexpr DWORD TEXT_COVERAGE_LAYER_FILL = 0x00FFFFFF;

/// Fill value for a layer whose text engine writes real alpha (DrawThemeTextEx
/// with DTT_COMPOSITED): fully transparent, so only the glyphs survive.
constexpr DWORD TEXT_ALPHA_LAYER_FILL = 0x00000000;

class TextLayer
{
public:
    /**
     * @param referenceDC A DC to create the DIB section compatible with.
     * @param width       Layer width in pixels.
     * @param height      Layer height in pixels.
     * @param fillPixel   Value every pixel starts at. Transparent black (0) for
     *                    a layer with real alpha; opaque white for a coverage
     *                    layer that GDI text will be drawn into in black.
     */
    TextLayer(HDC referenceDC, int width, int height, DWORD fillPixel)
        : dc(nullptr), bmp(nullptr), oldBmp(nullptr), pixels(nullptr), w(width), h(height)
    {
        if (w <= 0 || h <= 0)
            return;

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // Top-down, matching the corral back buffer
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void *bits = nullptr;
        dc = CreateCompatibleDC(referenceDC);
        if (!dc)
            return;

        bmp = CreateDIBSection(referenceDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bmp || !bits)
        {
            if (bmp)
                DeleteObject(bmp);
            bmp = nullptr;
            DeleteDC(dc);
            dc = nullptr;
            return;
        }

        oldBmp = (HBITMAP)SelectObject(dc, bmp);
        pixels = (DWORD *)bits;
        for (int i = 0; i < w * h; i++)
            pixels[i] = fillPixel;

        SetBkMode(dc, TRANSPARENT);
    }

    ~TextLayer()
    {
        if (dc)
        {
            if (oldBmp)
                SelectObject(dc, oldBmp);
            DeleteDC(dc);
        }
        if (bmp)
            DeleteObject(bmp);
    }

    TextLayer(const TextLayer &) = delete;
    TextLayer &operator=(const TextLayer &) = delete;

    bool IsValid() const { return pixels != nullptr; }
    HDC DC() const { return dc; }
    DWORD *Pixels() const { return pixels; }
    int Width() const { return w; }
    int Height() const { return h; }

private:
    HDC dc;
    HBITMAP bmp;
    HBITMAP oldBmp;
    DWORD *pixels;
    int w;
    int h;
};
