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
 * LayoutMath.h - Pure icon layout calculations (no Win32 window calls).
 *
 * These free functions compute icon cell positions given pre-resolved dimensions
 * (client width, DPI-scaled sizes, etc.).  The caller is responsible for querying
 * window/DPI state; these functions perform only arithmetic so they are fully
 * unit-testable without a live HWND.
 */

#pragma once
#include <vector>
#include <windows.h> // RECT

namespace LayoutMath
{

    struct IconCell
    {
        RECT rect;     ///< Full cell rect (used for selection / hit-testing)
        RECT iconRect; ///< Icon image rect within the cell
    };

    /// Direction for keyboard navigation between icon cells.
    enum class NavDirection
    {
        Up,
        Down,
        Left,
        Right
    };

    /**
     * Finds the neighbouring cell in a direction, purely geometrically.
     *
     * Candidates are the cells whose centre lies strictly in the given direction.
     * The winner is the one with the smallest offset on the perpendicular axis,
     * ties broken by the smallest gap along the direction axis. This works for any
     * column count and for details view (one row per item) without special cases.
     *
     * @return Index of the neighbour, or -1 if there is none.
     */
    int FindNeighbor(const std::vector<IconCell> &cells, int current, NavDirection dir);

    /**
     * Computes grid layout for SmallIcons / MediumIcons / LargeIcons view.
     *
     * Icons are placed left-to-right, wrapping to the next row when the next
     * icon would exceed (clientWidth - rightPadding).  The icon image is
     * centred horizontally within its spacing cell.
     *
     * @param iconCount     Number of icons to lay out.
     * @param clientWidth   Available horizontal space in pixels.
     * @param iconAreaTop   Y coordinate where the first row starts (below title bar).
     * @param iconSize      Square side length of the icon image in pixels.
     * @param iconSpacingX  Horizontal cell width (>= iconSize).
     * @param iconSpacingY  Vertical cell height (>= iconSize).
     * @param leftPadding   Left margin / x-origin for each row start.
     * @param rightPadding  Space reserved on the right (e.g. for a scrollbar).
     * @param outContentHeight [out] Total height of all laid-out rows + bottom padding.
     * @return              One IconCell per icon, in order.
     */
    std::vector<IconCell> ComputeGridLayout(
        int iconCount, int clientWidth, int iconAreaTop,
        int iconSize, int iconSpacingX, int iconSpacingY,
        int leftPadding, int rightPadding,
        int &outContentHeight);

    /**
     * Computes details-view layout (one row per icon, fixed row height).
     *
     * @param iconCount       Number of icons.
     * @param clientWidth     Available horizontal space.
     * @param iconAreaTop     Y coordinate of the first row.
     * @param detailsRowHeight Height of each row in pixels.
     * @param detailsIconSize  Side length of the small icon image.
     * @param leftPadding     Left margin.
     * @param rightPadding    Right margin (e.g. for scrollbar).
     * @param outContentHeight [out] Total content height.
     * @return                One IconCell per icon, in order.
     */
    std::vector<IconCell> ComputeDetailsLayout(
        int iconCount, int clientWidth, int iconAreaTop,
        int detailsRowHeight, int detailsIconSize,
        int leftPadding, int rightPadding,
        int &outContentHeight);

    /**
     * Clamps spacingX / spacingY up to the minimums needed to avoid overlap.
     *
     * @param spacingX   [in/out] Horizontal cell width.
     * @param spacingY   [in/out] Vertical cell height.
     * @param iconSize   Icon image side length.
     * @param minLabelX  Minimum extra pixels beside the icon (horizontal label space).
     * @param minLabelY  Minimum extra pixels below the icon (vertical label space).
     */
    void EnforceSpacingMinimums(int &spacingX, int &spacingY, int iconSize,
                                int minLabelX, int minLabelY);

    /**
     * Finds the top-left for a width x height rect as close as possible to
     * `desiredTopLeft` that fits inside `work` and overlaps none of `occupied`.
     *
     * The desired position is clamped into the work area and tried first; if it
     * is taken, the work area is scanned on a coarse grid for the nearest free
     * spot. `maxShift` caps how far that spot may sit from the clamped desired
     * position — beyond it, appearing where the user asked (overlapping) beats
     * teleporting across the monitor, so the clamped desired position is
     * returned. A non-positive `maxShift` means no cap.
     *
     * Pure arithmetic: the caller resolves the work area and the occupied rects.
     */
    POINT FindNearestFreeTopLeft(POINT desiredTopLeft, int width, int height,
                                 const RECT &work, const std::vector<RECT> &occupied,
                                 int maxShift);

} // namespace LayoutMath
