/**
 * test_chrome_alpha.cpp - Unit tests for the corral chrome alpha arithmetic.
 *
 * Covers the rules the opacity settings have to hold no matter where the
 * sliders sit: the header never fades below the point where it can still be
 * found and grabbed, an inactive tab is always dimmer than the active one (but
 * never invisible), and text at full opacity composites to exactly what drawing
 * it straight into the back buffer produced.
 */

#include <gtest/gtest.h>
#include "ChromeAlpha.h"

using namespace ChromeAlpha;

// ---------------------------------------------------------------------------
// Header opacity clamping
// ---------------------------------------------------------------------------

TEST(HeaderOpacity, ClampsBelowMinimum) {
    // The header is the corral's only grab handle: 0 would be an invisible
    // window that still swallows mouse input.
    EXPECT_EQ(ClampHeaderOpacity(0), HEADER_OPACITY_MIN);
    EXPECT_EQ(ClampHeaderOpacity(-40), HEADER_OPACITY_MIN);
    EXPECT_EQ(ClampHeaderOpacity(HEADER_OPACITY_MIN - 1), HEADER_OPACITY_MIN);
}

TEST(HeaderOpacity, PassesThroughValidRange) {
    EXPECT_EQ(ClampHeaderOpacity(HEADER_OPACITY_MIN), HEADER_OPACITY_MIN);
    EXPECT_EQ(ClampHeaderOpacity(128), 128);
    EXPECT_EQ(ClampHeaderOpacity(HEADER_OPACITY_DEFAULT), HEADER_OPACITY_DEFAULT);
    EXPECT_EQ(ClampHeaderOpacity(255), 255);
}

TEST(HeaderOpacity, ClampsAboveMaximum) {
    EXPECT_EQ(ClampHeaderOpacity(256), 255);
    EXPECT_EQ(ClampHeaderOpacity(9999), 255);
}

TEST(BorderOpacity, AllowsFullyTransparent) {
    // Unlike the header, a border at 0 is fine - it is decoration, not a target.
    EXPECT_EQ(ClampBorderOpacity(0), 0);
    EXPECT_EQ(ClampBorderOpacity(-1), 0);
    EXPECT_EQ(ClampBorderOpacity(255), 255);
    EXPECT_EQ(ClampBorderOpacity(300), 255);
}

// ---------------------------------------------------------------------------
// Rolled-up floor
// ---------------------------------------------------------------------------

TEST(EffectiveHeaderAlpha, UnrolledUsesAnimatedValue) {
    EXPECT_EQ(EffectiveHeaderAlpha(HEADER_OPACITY_MIN, false), HEADER_OPACITY_MIN);
    EXPECT_EQ(EffectiveHeaderAlpha(200, false), 200);
}

TEST(EffectiveHeaderAlpha, RolledUpRaisesToItsOwnFloor) {
    // Rolled up, the header IS the corral - a thin strip at the normal floor is
    // too hard to find.
    EXPECT_EQ(EffectiveHeaderAlpha(HEADER_OPACITY_MIN, true), ROLLED_UP_HEADER_ALPHA_MIN);
    EXPECT_EQ(EffectiveHeaderAlpha(ROLLED_UP_HEADER_ALPHA_MIN - 1, true), ROLLED_UP_HEADER_ALPHA_MIN);
}

TEST(EffectiveHeaderAlpha, RolledUpNeverLowersAValueAboveTheFloor) {
    EXPECT_EQ(EffectiveHeaderAlpha(200, true), 200);
    EXPECT_EQ(EffectiveHeaderAlpha(255, true), 255);
}

// ---------------------------------------------------------------------------
// Derived inactive tab alpha
// ---------------------------------------------------------------------------

TEST(InactiveTabAlpha, IsAlwaysDimmerThanTheHeader) {
    // The core promise of the feature: the active tab is distinguishable at
    // every possible header opacity.
    for (int headerAlpha = 0; headerAlpha <= 255; ++headerAlpha) {
        EXPECT_LE(InactiveTabAlpha(headerAlpha), headerAlpha)
            << "inverted at headerAlpha=" << headerAlpha;
    }
}

TEST(InactiveTabAlpha, NeverBelowItsFloorWithinTheSliderRange) {
    for (int headerAlpha = HEADER_OPACITY_MIN; headerAlpha <= 255; ++headerAlpha) {
        EXPECT_GE(InactiveTabAlpha(headerAlpha), INACTIVE_TAB_ALPHA_MIN)
            << "below floor at headerAlpha=" << headerAlpha;
    }
}

TEST(InactiveTabAlpha, ScalesWithTheHeader) {
    EXPECT_EQ(InactiveTabAlpha(255), (int)(255 * INACTIVE_TAB_ALPHA_FACTOR));
    EXPECT_LT(InactiveTabAlpha(128), InactiveTabAlpha(255));
    EXPECT_LT(InactiveTabAlpha(HEADER_OPACITY_MIN), InactiveTabAlpha(128));
}

TEST(InactiveTabAlpha, MonotonicInTheHeaderAlpha) {
    int previous = InactiveTabAlpha(0);
    for (int headerAlpha = 1; headerAlpha <= 255; ++headerAlpha) {
        int current = InactiveTabAlpha(headerAlpha);
        EXPECT_GE(current, previous) << "went backwards at headerAlpha=" << headerAlpha;
        previous = current;
    }
}

// ---------------------------------------------------------------------------
// Inactive tab colour darkening
// ---------------------------------------------------------------------------

TEST(InactiveTabRgbScale, DeepensAsTheHeaderFades) {
    // Near the floor the alpha gap alone is imperceptible, so the colour has to
    // carry the distinction.
    EXPECT_EQ(InactiveTabRgbScalePercent(255), INACTIVE_TAB_RGB_SCALE_MAX_PERCENT);
    EXPECT_EQ(InactiveTabRgbScalePercent(HEADER_OPACITY_MIN), INACTIVE_TAB_RGB_SCALE_MIN_PERCENT);
    EXPECT_LT(InactiveTabRgbScalePercent(64), InactiveTabRgbScalePercent(200));
}

TEST(InactiveTabRgbScale, StaysWithinBounds) {
    for (int headerAlpha = 0; headerAlpha <= 255; ++headerAlpha) {
        int percent = InactiveTabRgbScalePercent(headerAlpha);
        EXPECT_GE(percent, INACTIVE_TAB_RGB_SCALE_MIN_PERCENT);
        EXPECT_LE(percent, INACTIVE_TAB_RGB_SCALE_MAX_PERCENT);
    }
}

TEST(ScaleChannel, DarkensProportionally) {
    EXPECT_EQ(ScaleChannel(200, 50), 100);
    EXPECT_EQ(ScaleChannel(200, 25), 50);
    EXPECT_EQ(ScaleChannel(0, 50), 0);
}

// ---------------------------------------------------------------------------
// Pixel helpers
// ---------------------------------------------------------------------------

TEST(MakePremultiplied, OpaquePixelKeepsItsColour) {
    DWORD pixel = MakePremultiplied(255, 10, 20, 30);
    EXPECT_EQ((pixel >> 24) & 0xFF, 255u);
    EXPECT_EQ((pixel >> 16) & 0xFF, 10u);
    EXPECT_EQ((pixel >> 8) & 0xFF, 20u);
    EXPECT_EQ(pixel & 0xFF, 30u);
}

TEST(MakePremultiplied, HalfAlphaHalvesTheChannels) {
    DWORD pixel = MakePremultiplied(128, 200, 100, 50);
    EXPECT_EQ((pixel >> 24) & 0xFF, 128u);
    EXPECT_EQ((pixel >> 16) & 0xFF, (200u * 128u) / 255u);
    EXPECT_EQ((pixel >> 8) & 0xFF, (100u * 128u) / 255u);
    EXPECT_EQ(pixel & 0xFF, (50u * 128u) / 255u);
}

TEST(PremultipliedOver, TransparentSourceLeavesDestinationUntouched) {
    // A border at 0 must leave no trace at all - no ghost outline, no hole in
    // the corral fill.
    DWORD dst = MakePremultiplied(153, 0, 0, 255);
    EXPECT_EQ(PremultipliedOver(MakePremultiplied(0, 100, 100, 100), dst), dst);
}

TEST(PremultipliedOver, OpaqueSourceReplacesDestination) {
    DWORD src = MakePremultiplied(255, 100, 100, 100);
    DWORD dst = MakePremultiplied(153, 0, 0, 255);
    EXPECT_EQ(PremultipliedOver(src, dst), src);
}

TEST(PremultipliedOver, PartialSourceBlendsTowardsBoth) {
    DWORD src = MakePremultiplied(128, 100, 100, 100);
    DWORD dst = MakePremultiplied(255, 0, 0, 0);
    DWORD out = PremultipliedOver(src, dst);

    // Result is fully opaque (opaque destination) and carries the source colour
    // at roughly half strength.
    EXPECT_EQ((out >> 24) & 0xFF, 255u);
    EXPECT_GT((out >> 16) & 0xFF, 0u);
    EXPECT_LT((out >> 16) & 0xFF, 100u);
}

TEST(PremultipliedOver, NeverOverflowsAChannel) {
    DWORD src = MakePremultiplied(200, 255, 255, 255);
    DWORD dst = MakePremultiplied(255, 255, 255, 255);
    DWORD out = PremultipliedOver(src, dst);
    EXPECT_LE((out >> 24) & 0xFF, 255u);
    EXPECT_LE((out >> 16) & 0xFF, 255u);
    EXPECT_LE((out >> 8) & 0xFF, 255u);
    EXPECT_LE(out & 0xFF, 255u);
}

// ---------------------------------------------------------------------------
// Text opacity (header titles / icon labels)
// ---------------------------------------------------------------------------

TEST(TextOpacity, AllowsFullyTransparent) {
    // Text is not a hit target: a corral with no visible tab title is still
    // draggable by its header, and icons with no labels are still clickable.
    EXPECT_EQ(ClampTextOpacity(0), 0);
    EXPECT_EQ(ClampTextOpacity(-20), 0);
    EXPECT_EQ(ClampTextOpacity(128), 128);
    EXPECT_EQ(ClampTextOpacity(255), 255);
    EXPECT_EQ(ClampTextOpacity(400), 255);
}

TEST(TextCoverageAlpha, FullOpacityKeepsCoverageIntact) {
    // The whole point of the default: at 255 the glyph's antialiasing survives
    // untouched, so turning the setting on changes nothing until it is moved.
    EXPECT_EQ(TextCoverageAlpha(0, 255), 0);
    EXPECT_EQ(TextCoverageAlpha(128, 255), 128);
    EXPECT_EQ(TextCoverageAlpha(255, 255), 255);
}

TEST(TextCoverageAlpha, ScalesCoverageDown) {
    EXPECT_EQ(TextCoverageAlpha(255, 128), 128);
    EXPECT_EQ(TextCoverageAlpha(128, 128), 64);
}

TEST(TextCoverageAlpha, ZeroOpacityErasesEvenFullCoverage) {
    EXPECT_EQ(TextCoverageAlpha(255, 0), 0);
    EXPECT_EQ(TextCoverageAlpha(128, 0), 0);
}

TEST(TextCoverageAlpha, ClampsOutOfRangeOpacity) {
    EXPECT_EQ(TextCoverageAlpha(255, 999), 255);
    EXPECT_EQ(TextCoverageAlpha(255, -1), 0);
}

TEST(ScalePremultiplied, FullOpacityIsIdentity) {
    // Same guarantee for the icon labels, which arrive already premultiplied
    // from DrawThemeTextEx: at 255 the composite must be bit-identical to
    // drawing them straight into the back buffer.
    DWORD px = MakePremultiplied(200, 255, 128, 64);
    EXPECT_EQ(ScalePremultiplied(px, 255), px);
    EXPECT_EQ(ScalePremultiplied(px, 300), px);
}

TEST(ScalePremultiplied, ZeroOpacityErasesThePixel) {
    DWORD px = MakePremultiplied(255, 255, 255, 255);
    EXPECT_EQ(ScalePremultiplied(px, 0), 0u);
    EXPECT_EQ(ScalePremultiplied(px, -5), 0u);
}

TEST(ScalePremultiplied, HalvingScalesEveryChannel) {
    DWORD px = MakePremultiplied(200, 255, 128, 64);
    DWORD out = ScalePremultiplied(px, 128);

    EXPECT_EQ((out >> 24) & 0xFF, (((px >> 24) & 0xFF) * 128) / 255);
    EXPECT_EQ((out >> 16) & 0xFF, (((px >> 16) & 0xFF) * 128) / 255);
    EXPECT_EQ((out >> 8) & 0xFF, (((px >> 8) & 0xFF) * 128) / 255);
    EXPECT_EQ(out & 0xFF, ((px & 0xFF) * 128) / 255);
}

TEST(ScalePremultiplied, StaysPremultipliedAfterScaling) {
    // No colour channel may exceed the alpha, or the pixel is no longer a valid
    // premultiplied value and UpdateLayeredWindow renders it as a bright halo.
    for (int opacity = 0; opacity <= 255; opacity += 17) {
        DWORD out = ScalePremultiplied(MakePremultiplied(255, 255, 255, 255), opacity);
        BYTE a = (BYTE)((out >> 24) & 0xFF);
        EXPECT_LE((out >> 16) & 0xFF, a);
        EXPECT_LE((out >> 8) & 0xFF, a);
        EXPECT_LE(out & 0xFF, a);
    }
}

TEST(ScalePremultiplied, FadedTextOverABackgroundLandsBetweenTheTwo) {
    // What the render actually does: scale the glyph, then composite it over
    // the corral fill. Half-faded white text over black must read as grey.
    DWORD glyph = ScalePremultiplied(MakePremultiplied(255, 255, 255, 255), 128);
    DWORD out = PremultipliedOver(glyph, MakePremultiplied(255, 0, 0, 0));

    EXPECT_EQ((out >> 24) & 0xFF, 255u);
    EXPECT_GT((out >> 16) & 0xFF, 0u);
    EXPECT_LT((out >> 16) & 0xFF, 255u);
}

TEST(ScalePremultiplied, ZeroOpacityTextLeavesTheBackgroundUntouched) {
    DWORD background = MakePremultiplied(240, 30, 60, 90);
    DWORD glyph = ScalePremultiplied(MakePremultiplied(255, 255, 255, 255), 0);
    EXPECT_EQ(PremultipliedOver(glyph, background), background);
}
