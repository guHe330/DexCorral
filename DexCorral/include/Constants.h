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
// Off-Screen Positioning (Icon Hiding)
// ============================================================================

/// Off-screen X coordinate for hiding desktop icons (-5000)
/// Moves icons to a valid but invisible position for drag-drop occlusion.
/// Must be a valid screen coordinate to prevent Explorer crashes.
constexpr int ICON_HIDE_POSITION_X = -5000;

/// Off-screen Y coordinate for hiding desktop icons (-5000)
/// Moves icons to a valid but invisible position for drag-drop occlusion.
/// Must be a valid screen coordinate to prevent Explorer crashes.
constexpr int ICON_HIDE_POSITION_Y = -5000;

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

// ============================================================================
// Rendering: Colors (ARGB format)
// ============================================================================

/// Tab separator color (semi-transparent gray, ARGB: 200,80,80,80)
/// Divides tabs visually while blending with wallpaper
constexpr COLORREF TAB_SEPARATOR_COLOR = (200 << 24) | (80 << 16) | (80 << 8) | 80;

/// Corral border color (fully opaque dark gray, ARGB: 255,100,100,100)
/// Defines corral window edge
constexpr COLORREF CORRAL_BORDER_COLOR = (255 << 24) | (100 << 16) | (100 << 8) | 100;

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
