#include <gtest/gtest.h>

#include "ui/board_geometry.h"

// 800×800：格距 = 800 / 16 = 50，棋盤左上角交叉點在 (50, 50)
TEST(BoardGeometryTest, U1_SquareWidgetLayout) {
    const BoardGeometry g(800, 800);
    EXPECT_DOUBLE_EQ(g.cellSize(), 50.0);
    EXPECT_DOUBLE_EQ(g.cellCenter({0, 0}).x, 50.0);
    EXPECT_DOUBLE_EQ(g.cellCenter({0, 0}).y, 50.0);
    EXPECT_DOUBLE_EQ(g.cellCenter({14, 14}).x, 750.0);
    EXPECT_DOUBLE_EQ(g.cellCenter({14, 14}).y, 750.0);
}

TEST(BoardGeometryTest, U1_NonSquareWidgetIsCentered) {
    const BoardGeometry g(1000, 600);  // 格距 37.5，棋盤寬 525
    EXPECT_DOUBLE_EQ(g.cellSize(), 37.5);
    EXPECT_DOUBLE_EQ(g.cellCenter({0, 0}).x, 237.5);
    EXPECT_DOUBLE_EQ(g.cellCenter({0, 0}).y, 37.5);
}

TEST(BoardGeometryTest, U1_ClickMapsToNearestIntersection) {
    const BoardGeometry g(800, 800);
    EXPECT_EQ(g.pixelToCell(50, 50), std::optional<Pos>(Pos{0, 0}));
    EXPECT_EQ(g.pixelToCell(74, 50), std::optional<Pos>(Pos{0, 0}));
    EXPECT_EQ(g.pixelToCell(76, 50), std::optional<Pos>(Pos{1, 0}));
    EXPECT_EQ(g.pixelToCell(750, 750), std::optional<Pos>(Pos{14, 14}));
    EXPECT_EQ(g.pixelToCell(410, 380), std::optional<Pos>(Pos{7, 7}));
}

TEST(BoardGeometryTest, B1_XIsColumnAndYIsRow) {
    const BoardGeometry g(800, 800);
    EXPECT_EQ(g.pixelToCell(150, 50), std::optional<Pos>(Pos{2, 0}));
    EXPECT_EQ(g.pixelToCell(50, 150), std::optional<Pos>(Pos{0, 2}));
}

TEST(BoardGeometryTest, B3_ClickOutsideBoardIsIgnored) {
    const BoardGeometry g(800, 800);
    EXPECT_EQ(g.pixelToCell(10, 10), std::nullopt);
    EXPECT_EQ(g.pixelToCell(24, 50), std::nullopt);   // 離最左邊的線超過半格
    EXPECT_EQ(g.pixelToCell(50, 776), std::nullopt);  // 離最下面的線超過半格
    EXPECT_EQ(g.pixelToCell(-5, 400), std::nullopt);
}

TEST(BoardGeometryTest, U1_EmptyWidgetHasNoCells) {
    const BoardGeometry g(0, 0);
    EXPECT_EQ(g.pixelToCell(0, 0), std::nullopt);
}
