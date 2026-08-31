/**
 * test_layout.cpp - Unit tests for LayoutMath grid/details layout functions.
 *
 * All inputs are plain integers (no HWND required).  Tests verify icon
 * positioning, row wrapping, content-height computation, and spacing guards.
 */

#include <gtest/gtest.h>
#include "LayoutMath.h"

using namespace LayoutMath;

// ---------------------------------------------------------------------------
// Grid layout
// ---------------------------------------------------------------------------

TEST(GridLayout, Empty) {
    int contentHeight = -1;
    auto cells = ComputeGridLayout(0, 400, 36, 32, 80, 72, 8, 8, contentHeight);
    EXPECT_TRUE(cells.empty());
    EXPECT_EQ(contentHeight, 36);  // iconAreaTop returned when no icons
}

TEST(GridLayout, SingleIcon_Position) {
    // leftPadding=8, iconAreaTop=36
    int ch;
    auto cells = ComputeGridLayout(1, 400, 36, 32, 80, 72, 8, 8, ch);
    ASSERT_EQ(cells.size(), 1u);
    EXPECT_EQ(cells[0].rect.left, 8);
    EXPECT_EQ(cells[0].rect.top,  36);
    EXPECT_EQ(cells[0].rect.right,  8 + 80);
    EXPECT_EQ(cells[0].rect.bottom, 36 + 72);
}

TEST(GridLayout, SingleIcon_IconRectCenteredHorizontally) {
    // iconSpacingX=80, iconSize=32 -> center offset = (80-32)/2 = 24
    int ch;
    auto cells = ComputeGridLayout(1, 400, 36, 32, 80, 72, 8, 8, ch);
    ASSERT_EQ(cells.size(), 1u);
    EXPECT_EQ(cells[0].iconRect.left,  8 + 24);   // leftPadding + (spacingX-iconSize)/2
    EXPECT_EQ(cells[0].iconRect.right, 8 + 24 + 32);
    EXPECT_EQ(cells[0].iconRect.top,   36);        // y == iconAreaTop
}

TEST(GridLayout, RowWrapping) {
    // clientWidth=300, leftPadding=8, rightPadding=8 -> usable = 284
    // iconSpacingX=90 -> fits floor(284/90)=3 per row
    // 4th icon should wrap to row 2
    int ch;
    // Place exactly 3 icons, then a 4th
    auto cells = ComputeGridLayout(4, 300, 0, 32, 90, 70, 8, 8, ch);
    ASSERT_EQ(cells.size(), 4u);

    // Icons 0-2 should be on row 0 (y==0)
    EXPECT_EQ(cells[0].rect.top, 0);
    EXPECT_EQ(cells[1].rect.top, 0);
    EXPECT_EQ(cells[2].rect.top, 0);
    // Icon 3 wraps
    EXPECT_EQ(cells[3].rect.top, 70);
    EXPECT_EQ(cells[3].rect.left, 8);  // back to leftPadding
}

TEST(GridLayout, RowWrapping_ExactlyFits) {
    // If 3 icons fit exactly in one row, icon 4 is still on row 2 (not row 1)
    // clientWidth=278, leftPadding=8, rightPadding=8 -> usable=262
    // spacingX=87 -> 3*87=261 <= 262, 4*87=348 > 262 -> 3 per row
    int ch;
    auto cells = ComputeGridLayout(3, 278, 0, 32, 87, 70, 8, 8, ch);
    ASSERT_EQ(cells.size(), 3u);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(cells[i].rect.top, 0) << "icon " << i << " should be on row 0";
    }
}

TEST(GridLayout, ContentHeightCalculation) {
    // 2 rows of icons; bottom of last icon = iconAreaTop + 2*spacingY
    int ch;
    // 1 icon per row to force 2 rows: clientWidth just barely fits one
    // clientWidth=96, spacingX=90, leftPadding=8, rightPadding=8 -> usable=80 < 90*2
    auto cells = ComputeGridLayout(2, 96, 10, 32, 90, 70, 8, 8, ch);
    ASSERT_EQ(cells.size(), 2u);
    EXPECT_EQ(cells[1].rect.bottom, 10 + 2 * 70);
    EXPECT_EQ(ch, cells[1].rect.bottom + 8);  // + leftPadding
}

TEST(GridLayout, ScrollbarNarrowsAvailableWidth) {
    // Without scrollbar (rightPadding=8): 3 per row
    // With scrollbar (rightPadding=28 = 8+10+2*5): 2 per row
    int ch1, ch2;
    auto wide = ComputeGridLayout(3, 300, 0, 32, 90, 70, 8, 8,  ch1);
    auto narr = ComputeGridLayout(3, 300, 0, 32, 90, 70, 8, 28, ch2);

    // Wide: all 3 on row 0
    EXPECT_EQ(wide[2].rect.top, 0);
    // Narrow: 2 fit on row 0, 3rd wraps
    EXPECT_EQ(narr[2].rect.top, 70);
    // Narrow has greater contentHeight
    EXPECT_GT(ch2, ch1);
}

// ---------------------------------------------------------------------------
// Details layout
// ---------------------------------------------------------------------------

TEST(DetailsLayout, Empty) {
    int ch = -1;
    auto cells = ComputeDetailsLayout(0, 400, 36, 20, 16, 8, 8, ch);
    EXPECT_TRUE(cells.empty());
    EXPECT_EQ(ch, 36);
}

TEST(DetailsLayout, RowPositions) {
    // iconAreaTop=36, rowHeight=20, 3 icons
    int ch;
    auto cells = ComputeDetailsLayout(3, 400, 36, 20, 16, 8, 8, ch);
    ASSERT_EQ(cells.size(), 3u);
    EXPECT_EQ(cells[0].rect.top, 36);
    EXPECT_EQ(cells[1].rect.top, 56);
    EXPECT_EQ(cells[2].rect.top, 76);
    // contentHeight = y after last row + leftPadding = 96 + 8 = 104
    EXPECT_EQ(ch, 96 + 8);
}

TEST(DetailsLayout, IconVerticallycenteredInRow) {
    // rowHeight=20, iconSize=16 -> topOffset = (20-16)/2 = 2
    int ch;
    auto cells = ComputeDetailsLayout(1, 400, 36, 20, 16, 8, 8, ch);
    ASSERT_EQ(cells.size(), 1u);
    EXPECT_EQ(cells[0].iconRect.top,    36 + 2);   // rowTop + (rowH-iconSz)/2
    EXPECT_EQ(cells[0].iconRect.bottom, 36 + 2 + 16);
}

TEST(DetailsLayout, RowSpansFullWidth) {
    // rect should span from leftPadding to clientWidth - rightPadding
    int ch;
    auto cells = ComputeDetailsLayout(1, 400, 0, 20, 16, 8, 8, ch);
    ASSERT_EQ(cells.size(), 1u);
    EXPECT_EQ(cells[0].rect.left,  8);
    EXPECT_EQ(cells[0].rect.right, 400 - 8);
}

TEST(DetailsLayout, ScrollbarNarrowsRows) {
    // With scrollbar rightPadding=28: row right = 400-28=372
    int ch;
    auto cells = ComputeDetailsLayout(1, 400, 0, 20, 16, 8, 28, ch);
    ASSERT_EQ(cells.size(), 1u);
    EXPECT_EQ(cells[0].rect.right, 400 - 28);
}

// ---------------------------------------------------------------------------
// EnforceSpacingMinimums
// ---------------------------------------------------------------------------

TEST(EnforceSpacing, BelowMinimum_ClampsUp) {
    int sx = 10, sy = 10;
    // iconSize=32, minLabelX=16, minLabelY=20 -> minimums: 48, 52
    EnforceSpacingMinimums(sx, sy, 32, 16, 20);
    EXPECT_EQ(sx, 48);
    EXPECT_EQ(sy, 52);
}

TEST(EnforceSpacing, AboveMinimum_Unchanged) {
    int sx = 100, sy = 100;
    EnforceSpacingMinimums(sx, sy, 32, 16, 20);
    EXPECT_EQ(sx, 100);
    EXPECT_EQ(sy, 100);
}

TEST(EnforceSpacing, ExactlyAtMinimum_Unchanged) {
    int sx = 48, sy = 52;
    EnforceSpacingMinimums(sx, sy, 32, 16, 20);
    EXPECT_EQ(sx, 48);
    EXPECT_EQ(sy, 52);
}

// ---------------------------------------------------------------------------
// FindNeighbor (keyboard navigation)
// ---------------------------------------------------------------------------

TEST(FindNeighbor, EmptyAndOutOfRange) {
    std::vector<IconCell> none;
    EXPECT_EQ(FindNeighbor(none, 0, NavDirection::Down), -1);

    int ch;
    auto cells = ComputeGridLayout(3, 300, 0, 32, 90, 70, 8, 8, ch);
    EXPECT_EQ(FindNeighbor(cells, -1, NavDirection::Down), -1);
    EXPECT_EQ(FindNeighbor(cells, 3, NavDirection::Down), -1);
}

TEST(FindNeighbor, SingleIcon_NoNeighbours) {
    int ch;
    auto cells = ComputeGridLayout(1, 300, 0, 32, 90, 70, 8, 8, ch);
    EXPECT_EQ(FindNeighbor(cells, 0, NavDirection::Up), -1);
    EXPECT_EQ(FindNeighbor(cells, 0, NavDirection::Down), -1);
    EXPECT_EQ(FindNeighbor(cells, 0, NavDirection::Left), -1);
    EXPECT_EQ(FindNeighbor(cells, 0, NavDirection::Right), -1);
}

TEST(FindNeighbor, Grid_WithinRow) {
    // 3 columns, 6 icons -> rows [0 1 2] [3 4 5]
    int ch;
    auto cells = ComputeGridLayout(6, 300, 0, 32, 90, 70, 8, 8, ch);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Left),  0);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Right), 2);
    EXPECT_EQ(FindNeighbor(cells, 0, NavDirection::Left),  -1);
    EXPECT_EQ(FindNeighbor(cells, 2, NavDirection::Right), -1);
}

TEST(FindNeighbor, Grid_BetweenRows) {
    int ch;
    auto cells = ComputeGridLayout(6, 300, 0, 32, 90, 70, 8, 8, ch);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Down), 4);
    EXPECT_EQ(FindNeighbor(cells, 4, NavDirection::Up),   1);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Up),   -1);
    EXPECT_EQ(FindNeighbor(cells, 4, NavDirection::Down), -1);
}

TEST(FindNeighbor, Grid_RaggedLastRow_FallsToNearestColumn) {
    // 3 columns, 5 icons -> rows [0 1 2] [3 4]; below icon 2 there is no cell in
    // that column, so Down picks the nearest column instead (icon 4).
    int ch;
    auto cells = ComputeGridLayout(5, 300, 0, 32, 90, 70, 8, 8, ch);
    EXPECT_EQ(FindNeighbor(cells, 2, NavDirection::Down), 4);
    EXPECT_EQ(FindNeighbor(cells, 4, NavDirection::Up),   1);
}

TEST(FindNeighbor, SingleColumn) {
    // clientWidth only fits one column
    int ch;
    auto cells = ComputeGridLayout(3, 120, 0, 32, 90, 70, 8, 8, ch);
    EXPECT_EQ(FindNeighbor(cells, 0, NavDirection::Down), 1);
    EXPECT_EQ(FindNeighbor(cells, 2, NavDirection::Up),   1);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Left), -1);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Right), -1);
}

TEST(FindNeighbor, DetailsView_RowsOnly) {
    int ch;
    auto cells = ComputeDetailsLayout(4, 400, 0, 20, 16, 8, 8, ch);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Down), 2);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Up),   0);
    EXPECT_EQ(FindNeighbor(cells, 3, NavDirection::Down), -1);
    // Every row spans the same x range, so there is no horizontal neighbour.
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Left),  -1);
    EXPECT_EQ(FindNeighbor(cells, 1, NavDirection::Right), -1);
}
