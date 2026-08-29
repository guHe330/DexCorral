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
#include "Constants.h"

///
/// ChromeAlpha.h - Pure alpha arithmetic for corral chrome (header, tabs, border, text).
///
/// The header opacity and the border opacity are user settings; the inactive tab
/// appearance is *derived* from the header opacity rather than configured, so the
/// active tab always stays distinguishable no matter where the slider sits.
///
/// The header title and icon label opacities are separate user settings, applied
/// to a scratch text layer at composite time (see CorralWindowRender.cpp).
///
/// These are free functions performing only arithmetic, so they are unit-testable
/// without a live HWND (see tests/test_chrome_alpha.cpp). The tunable values they
/// read live in Constants.h.
///
namespace ChromeAlpha
{

    /// Clamps a configured header opacity into the usable range.
    /// The floor keeps the corral's only grab handle findable — see HEADER_OPACITY_MIN.
    inline int ClampHeaderOpacity(int configured)
    {
        if (configured < HEADER_OPACITY_MIN)
            return HEADER_OPACITY_MIN;
        if (configured > 255)
            return 255;
        return configured;
    }

    /// Clamps a configured border opacity. Unlike the header, 0 is allowed:
    /// the border is decoration, not a hit target.
    inline int ClampBorderOpacity(int configured)
    {
        if (configured < 0)
            return 0;
        if (configured > 255)
            return 255;
        return configured;
    }

    /**
     * Clamps a configured text opacity (header title or icon label).
     *
     * Like the border and unlike the header itself, 0 is allowed: text is not a
     * hit target. Invisible tab titles still leave a draggable header, and
     * faded-out labels still leave clickable icons.
     */
    inline int ClampTextOpacity(int configured)
    {
        if (configured < 0)
            return 0;
        if (configured > 255)
            return 255;
        return configured;
    }

    /**
     * Turns an antialiasing coverage value into a pixel alpha.
     *
     * The header title is drawn black-on-white into a scratch layer, which
     * yields per-pixel coverage rather than a colour (see CorralWindowRender).
     * Scaling that coverage by the configured text opacity gives the alpha the
     * glyph pixel should end up with.
     */
    inline BYTE TextCoverageAlpha(BYTE coverage, int textOpacity)
    {
        return (BYTE)((coverage * ClampTextOpacity(textOpacity)) / 255);
    }

    /**
     * Scales a premultiplied pixel's opacity.
     *
     * Premultiplied alpha scales by plain multiplication on all four channels,
     * so this is exact — no round-trip through straight alpha. Used to fade the
     * icon label layer, whose pixels come out of DrawThemeTextEx already
     * premultiplied.
     */
    inline DWORD ScalePremultiplied(DWORD px, int opacity)
    {
        const int f = ClampTextOpacity(opacity);
        if (f >= 255)
            return px;
        if (f == 0)
            return 0;
        return ((DWORD)((((px >> 24) & 0xFF) * f) / 255) << 24) |
               ((DWORD)((((px >> 16) & 0xFF) * f) / 255) << 16) |
               ((DWORD)((((px >> 8) & 0xFF) * f) / 255) << 8) |
               (DWORD)(((px & 0xFF) * f) / 255);
    }

    /**
     * Blends a configured text opacity towards fully opaque as a hover fade runs.
     *
     * The icon, header and border opacities animate their value directly, but
     * the header title opacity is per *tab* while the animation state is per
     * window, so what animates for text is the progress itself: 0 leaves every
     * tab at its configured opacity, 255 brings all of them to full. Feeding
     * the same eased progress through this produces exactly the value a direct
     * animation would have reached, so text fades in step with the icons.
     */
    inline int HoverBlendTextOpacity(int configured, int hoverProgress)
    {
        const int base = ClampTextOpacity(configured);
        const int p = ClampTextOpacity(hoverProgress);
        return base + ((255 - base) * p) / 255;
    }

    /**
     * Returns the header alpha to actually render with.
     *
     * @param animatedHeaderAlpha Current (hover-animated) header alpha.
     * @param isRolledUp          True while the corral is rolled up, where the
     *                            header is the entire corral and needs a higher
     *                            floor to stay findable.
     *
     * This is a render-time clamp only: the configured HeaderOpacity is never
     * rewritten, so unrolling restores exactly what the user set.
     */
    inline int EffectiveHeaderAlpha(int animatedHeaderAlpha, bool isRolledUp)
    {
        int alpha = animatedHeaderAlpha;
        if (alpha < 0)
            alpha = 0;
        if (alpha > 255)
            alpha = 255;
        if (isRolledUp && alpha < ROLLED_UP_HEADER_ALPHA_MIN)
            alpha = ROLLED_UP_HEADER_ALPHA_MIN;
        return alpha;
    }

    /**
     * Derives the inactive tab alpha from the header alpha.
     *
     * Never exceeds the header alpha (inactive tabs can not out-shine the active
     * one) and never drops below INACTIVE_TAB_ALPHA_MIN, so an inactive tab stays
     * recognisable as a tab even at the header floor.
     */
    inline int InactiveTabAlpha(int headerAlpha)
    {
        if (headerAlpha < 0)
            headerAlpha = 0;
        if (headerAlpha > 255)
            headerAlpha = 255;

        int derived = (int)(headerAlpha * INACTIVE_TAB_ALPHA_FACTOR);
        if (derived < INACTIVE_TAB_ALPHA_MIN)
            derived = INACTIVE_TAB_ALPHA_MIN;
        if (derived > headerAlpha)
            derived = headerAlpha; // never inverted, incl. headerAlpha below the floor
        return derived;
    }

    /**
     * Returns how much of its own colour an inactive tab keeps, in percent.
     *
     * The alpha reduction alone is not enough near the opacity floor, where the
     * active/inactive gap collapses to a few units of alpha. The colour therefore
     * darkens further as the header fades: INACTIVE_TAB_RGB_SCALE_MAX_PERCENT at
     * full opacity down to INACTIVE_TAB_RGB_SCALE_MIN_PERCENT at HEADER_OPACITY_MIN.
     */
    inline int InactiveTabRgbScalePercent(int headerAlpha)
    {
        if (headerAlpha <= HEADER_OPACITY_MIN)
            return INACTIVE_TAB_RGB_SCALE_MIN_PERCENT;
        if (headerAlpha >= 255)
            return INACTIVE_TAB_RGB_SCALE_MAX_PERCENT;

        const int span = 255 - HEADER_OPACITY_MIN;
        const int range = INACTIVE_TAB_RGB_SCALE_MAX_PERCENT - INACTIVE_TAB_RGB_SCALE_MIN_PERCENT;
        return INACTIVE_TAB_RGB_SCALE_MIN_PERCENT +
               ((headerAlpha - HEADER_OPACITY_MIN) * range) / span;
    }

    /// Scales one colour channel by a percentage (used for inactive tab darkening)
    inline BYTE ScaleChannel(BYTE value, int percent)
    {
        return (BYTE)((value * percent) / 100);
    }

    /// Builds a premultiplied ARGB pixel from straight colour components
    inline DWORD MakePremultiplied(BYTE alpha, BYTE r, BYTE g, BYTE b)
    {
        BYTE pmR = (BYTE)((r * alpha) / 255);
        BYTE pmG = (BYTE)((g * alpha) / 255);
        BYTE pmB = (BYTE)((b * alpha) / 255);
        return ((DWORD)alpha << 24) | ((DWORD)pmR << 16) | ((DWORD)pmG << 8) | pmB;
    }

    /**
     * Composites one premultiplied pixel over another (source-over).
     *
     * Needed for the border: at less than full opacity it has to blend with the
     * corral fill underneath instead of replacing it, and at alpha 0 it has to
     * leave the destination completely untouched — no ghost outline.
     */
    inline DWORD PremultipliedOver(DWORD src, DWORD dst)
    {
        int srcA = (src >> 24) & 0xFF;
        if (srcA >= 255)
            return src;
        if (srcA == 0)
            return dst;

        const int inv = 255 - srcA;
        int a = srcA + (((dst >> 24) & 0xFF) * inv) / 255;
        int r = ((src >> 16) & 0xFF) + ((((dst >> 16) & 0xFF) * inv) / 255);
        int g = ((src >> 8) & 0xFF) + ((((dst >> 8) & 0xFF) * inv) / 255);
        int b = (src & 0xFF) + (((dst & 0xFF) * inv) / 255);

        return ((DWORD)(a > 255 ? 255 : a) << 24) |
               ((DWORD)(r > 255 ? 255 : r) << 16) |
               ((DWORD)(g > 255 ? 255 : g) << 8) |
               (DWORD)(b > 255 ? 255 : b);
    }

} // namespace ChromeAlpha
