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

#include "LayoutMath.h"

namespace LayoutMath
{

    int FindNeighbor(const std::vector<IconCell> &cells, int current, NavDirection dir)
    {
        if (current < 0 || current >= (int)cells.size())
            return -1;

        const RECT &cur = cells[current].rect;
        int curX = (cur.left + cur.right) / 2;
        int curY = (cur.top + cur.bottom) / 2;

        int best = -1;
        long long bestPerp = 0, bestGap = 0;

        for (int i = 0; i < (int)cells.size(); i++)
        {
            if (i == current)
                continue;
            const RECT &r = cells[i].rect;
            int cx = (r.left + r.right) / 2;
            int cy = (r.top + r.bottom) / 2;

            long long gap, perp;
            switch (dir)
            {
            case NavDirection::Up:
                gap = curY - cy;
                perp = cx - curX;
                break;
            case NavDirection::Down:
                gap = cy - curY;
                perp = cx - curX;
                break;
            case NavDirection::Left:
                gap = curX - cx;
                perp = cy - curY;
                break;
            default: // Right
                gap = cx - curX;
                perp = cy - curY;
                break;
            }
            if (gap <= 0)
                continue; // Not in the requested direction
            if (perp < 0)
                perp = -perp;

            // Perpendicular offset first (stay in the same column/row), then distance.
            if (best < 0 || perp < bestPerp || (perp == bestPerp && gap < bestGap))
            {
                best = i;
                bestPerp = perp;
                bestGap = gap;
            }
        }

        return best;
    }

    std::vector<IconCell> ComputeGridLayout(
        int iconCount, int clientWidth, int iconAreaTop,
        int iconSize, int iconSpacingX, int iconSpacingY,
        int leftPadding, int rightPadding,
        int &outContentHeight)
    {
        std::vector<IconCell> result(iconCount);

        int x = leftPadding;
        int y = iconAreaTop;

        for (int i = 0; i < iconCount; i++)
        {
            int iconImgX = x + (iconSpacingX - iconSize) / 2;
            int iconImgY = y;

            result[i].iconRect = {iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize};
            result[i].rect = {x, y, x + iconSpacingX, y + iconSpacingY};

            x += iconSpacingX;
            if (x + iconSpacingX > clientWidth - rightPadding)
            {
                x = leftPadding;
                y += iconSpacingY;
            }
        }

        if (iconCount > 0)
        {
            outContentHeight = result.back().rect.bottom + leftPadding;
        }
        else
        {
            outContentHeight = iconAreaTop;
        }

        return result;
    }

    std::vector<IconCell> ComputeDetailsLayout(
        int iconCount, int clientWidth, int iconAreaTop,
        int detailsRowHeight, int detailsIconSize,
        int leftPadding, int rightPadding,
        int &outContentHeight)
    {
        std::vector<IconCell> result(iconCount);

        int y = iconAreaTop;

        for (int i = 0; i < iconCount; i++)
        {
            int iconImgX = leftPadding + 2;
            int iconImgY = y + (detailsRowHeight - detailsIconSize) / 2;

            result[i].iconRect = {iconImgX, iconImgY, iconImgX + detailsIconSize, iconImgY + detailsIconSize};
            result[i].rect = {leftPadding, y, clientWidth - rightPadding, y + detailsRowHeight};

            y += detailsRowHeight;
        }

        if (iconCount > 0)
        {
            outContentHeight = y + leftPadding;
        }
        else
        {
            outContentHeight = iconAreaTop;
        }

        return result;
    }

    void EnforceSpacingMinimums(int &spacingX, int &spacingY, int iconSize,
                                int minLabelX, int minLabelY)
    {
        if (spacingX < iconSize + minLabelX)
            spacingX = iconSize + minLabelX;
        if (spacingY < iconSize + minLabelY)
            spacingY = iconSize + minLabelY;
    }

} // namespace LayoutMath
