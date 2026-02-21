#include "LayoutMath.h"

namespace LayoutMath {

std::vector<IconCell> ComputeGridLayout(
    int iconCount, int clientWidth, int iconAreaTop,
    int iconSize, int iconSpacingX, int iconSpacingY,
    int leftPadding, int rightPadding,
    int& outContentHeight)
{
    std::vector<IconCell> result(iconCount);

    int x = leftPadding;
    int y = iconAreaTop;

    for (int i = 0; i < iconCount; i++) {
        int iconImgX = x + (iconSpacingX - iconSize) / 2;
        int iconImgY = y;

        result[i].iconRect = { iconImgX, iconImgY, iconImgX + iconSize, iconImgY + iconSize };
        result[i].rect     = { x, y, x + iconSpacingX, y + iconSpacingY };

        x += iconSpacingX;
        if (x + iconSpacingX > clientWidth - rightPadding) {
            x = leftPadding;
            y += iconSpacingY;
        }
    }

    if (iconCount > 0) {
        outContentHeight = result.back().rect.bottom + leftPadding;
    } else {
        outContentHeight = iconAreaTop;
    }

    return result;
}

std::vector<IconCell> ComputeDetailsLayout(
    int iconCount, int clientWidth, int iconAreaTop,
    int detailsRowHeight, int detailsIconSize,
    int leftPadding, int rightPadding,
    int& outContentHeight)
{
    std::vector<IconCell> result(iconCount);

    int y = iconAreaTop;

    for (int i = 0; i < iconCount; i++) {
        int iconImgX = leftPadding + 2;
        int iconImgY = y + (detailsRowHeight - detailsIconSize) / 2;

        result[i].iconRect = { iconImgX, iconImgY, iconImgX + detailsIconSize, iconImgY + detailsIconSize };
        result[i].rect     = { leftPadding, y, clientWidth - rightPadding, y + detailsRowHeight };

        y += detailsRowHeight;
    }

    if (iconCount > 0) {
        outContentHeight = y + leftPadding;
    } else {
        outContentHeight = iconAreaTop;
    }

    return result;
}

void EnforceSpacingMinimums(int& spacingX, int& spacingY, int iconSize,
                             int minLabelX, int minLabelY)
{
    if (spacingX < iconSize + minLabelX) spacingX = iconSize + minLabelX;
    if (spacingY < iconSize + minLabelY) spacingY = iconSize + minLabelY;
}

} // namespace LayoutMath
