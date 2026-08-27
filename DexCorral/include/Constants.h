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

///
/// Constants.h - Global constants for DexCorral application
///
/// Centralized definitions for all magic numbers, colors, sizes, and thresholds
/// used throughout the application. Keeps constants organized, documented, and
/// easy to tune without searching through source files.
///

// ============================================================================
// Buffer Sizes
// ============================================================================

/// Buffer size for cross-process ListView item retrieval (4KB)
/// Used when reading window titles, paths, and other metadata from Explorer
constexpr DWORD DESKTOP_ICON_BUFFER_SIZE = 4096;

/// Dialog template buffer size (4KB)
/// Sufficient for in-memory DLGTEMPLATE construction and dialog resources
constexpr DWORD DIALOG_TEMPLATE_BUFFER_SIZE = 4096;

// ============================================================================
// Rendering: Opacity and Alpha
// ============================================================================

/// Title bar text alpha (86% opacity, 220/255)
/// Reduces title bar visibility while maintaining readability
constexpr BYTE TITLE_TEXT_ALPHA = 220;

/// Icon selection overlay alpha (70% opacity, 180/255)
/// Provides visual feedback for selected icons without blocking them
constexpr BYTE SELECTION_ALPHA = 180;

/// Icon hover overlay alpha (30% opacity, 77/255)
/// Subtle highlight on mouse-over
constexpr BYTE HOVER_ALPHA = 77;

// ----------------------------------------------------------------------------
// Corral chrome (header / tab strip / border) opacity
//
// The values below are the tunables behind the configurable header and border
// opacity. They are gathered here so tuning is a one-line change per value.
// See ChromeAlpha.h for the arithmetic that uses them.
// ----------------------------------------------------------------------------

/// Default header opacity (240/255, ~94%)
/// The value the active tab header was hard-coded to before it became
/// configurable, so existing corrals look unchanged.
constexpr int HEADER_OPACITY_DEFAULT = 240;

/// Minimum configurable header opacity (20/255, ~8%)
/// The header is a corral's only grab handle: it is what you drag to move the
/// corral, double-click to roll up, and right-click for the context menu. At 0
/// a corral with a transparent background would be an invisible window that
/// still swallows mouse input — unfindable and unrecoverable through the UI.
/// 20 reads as "invisible" against a busy wallpaper but leaves a faint edge to
/// aim at. Same reasoning as the icon opacity floor (CorralWindow::SetCurrentOpacity).
constexpr int HEADER_OPACITY_MIN = 20;

/// Minimum header opacity while a corral is rolled up (60/255, ~24%)
/// Rolled up, the header *is* the whole corral, and a ~26px strip at the 20
/// floor is a very thin target to hunt for. Applied at render time only: the
/// configured value is never rewritten, so unrolling restores it exactly.
constexpr int ROLLED_UP_HEADER_ALPHA_MIN = 60;

/// Inactive tab alpha as a fraction of the header opacity
constexpr float INACTIVE_TAB_ALPHA_FACTOR = 0.55f;

/// Absolute floor for the derived inactive tab alpha (10/255)
constexpr int INACTIVE_TAB_ALPHA_MIN = 10;

/// Inactive tab colour darkening, in percent of the tab's own colour.
/// Near the opacity floor the alpha difference alone collapses (20 vs 11 is
/// barely perceptible over a busy wallpaper), so the darkening deepens as the
/// header fades: 50% at full opacity, 25% at HEADER_OPACITY_MIN.
constexpr int INACTIVE_TAB_RGB_SCALE_MAX_PERCENT = 50;
constexpr int INACTIVE_TAB_RGB_SCALE_MIN_PERCENT = 25;

/// Default border opacity (255, fully opaque) — the previous hard-coded value
constexpr int BORDER_OPACITY_DEFAULT = 255;

// ============================================================================
// Rendering: Colors (ARGB format)
// ============================================================================

/// Tab separator color (semi-transparent gray, ARGB: 200,80,80,80)
/// Divides tabs visually while blending with wallpaper
constexpr COLORREF TAB_SEPARATOR_COLOR = (200 << 24) | (80 << 16) | (80 << 8) | 80;

/// Corral border color RGB components (dark gray: 100, 100, 100)
/// Defines the corral window edge; the alpha comes from CorralWindowConfig::BorderOpacity
constexpr BYTE CORRAL_BORDER_R = 100;
constexpr BYTE CORRAL_BORDER_G = 100;
constexpr BYTE CORRAL_BORDER_B = 100;

/// Icon selection color RGB components (sky blue: 60, 120, 200)
/// Provides visual highlight for selected icons
constexpr BYTE SELECTION_R = 60;
constexpr BYTE SELECTION_G = 120;
constexpr BYTE SELECTION_B = 200;

/// Icon hover color RGB components (light gray: 180, 180, 200)
constexpr BYTE HOVER_R = 180;
constexpr BYTE HOVER_G = 180;
constexpr BYTE HOVER_B = 200;

// ============================================================================
// Icon Rendering
// ============================================================================

/// Temporary icon rendering size threshold (64px)
/// Icons larger than this are rendered at native size for performance
constexpr int ICON_TEMP_SIZE_THRESHOLD = 64;

/// Maximum icon size in pixels (256)
/// Upper limit for icon dimensions in the grid
constexpr int MAX_ICON_SIZE = 256;

// ============================================================================
// Icon Content Detection
// ============================================================================

/// Icon content detection threshold (70%)
/// Icons with more than 70% non-transparent pixels are considered "full content".
/// Used to detect padded icons (like shortcuts with badges) vs empty spaces.
constexpr double ICON_CONTENT_THRESHOLD = 0.7;

/// OneDrive cloud reparse point tag mask (0x90000000)
/// OneDrive uses 0x9000xxxx tags for cloud-synced or syncing files.
/// Used to detect cloud status badges on icon file paths.
constexpr DWORD ONEDRIVE_CLOUD_TAG_MASK = 0x90000000;
