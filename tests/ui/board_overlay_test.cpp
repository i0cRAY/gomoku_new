#include <gtest/gtest.h>

#include <algorithm>

#include "ui/board_overlay.h"

TEST(BoardOverlayTest, U3_DestroyPreviewIsNineByNineAtCenter) {
    const auto cells = destroyPreviewCells({7, 7}, 4);
    EXPECT_EQ(cells.size(), 81u);
    EXPECT_NE(std::find(cells.begin(), cells.end(), Pos{3, 3}), cells.end());
    EXPECT_NE(std::find(cells.begin(), cells.end(), Pos{11, 11}), cells.end());
    EXPECT_EQ(std::find(cells.begin(), cells.end(), Pos{2, 7}), cells.end());
}

TEST(BoardOverlayTest, U3_DestroyPreviewClippedAtCorner) {
    EXPECT_EQ(destroyPreviewCells({0, 0}, 4).size(), 25u);
    EXPECT_EQ(destroyPreviewCells({14, 7}, 4).size(), 45u);  // 5 × 9
}

TEST(BoardOverlayTest, U9_ZoneOpacityFadesWithRemainingTime) {
    const ZoneCell zone{{3, 3}, PlayerId::Black, 3000};
    EXPECT_DOUBLE_EQ(zoneOpacity(zone, 0, 3000), 1.0);
    EXPECT_DOUBLE_EQ(zoneOpacity(zone, 1500, 3000), 0.5);
    EXPECT_DOUBLE_EQ(zoneOpacity(zone, 3000, 3000), 0.0);
    EXPECT_DOUBLE_EQ(zoneOpacity(zone, 9000, 3000), 0.0);
}

TEST(BoardOverlayTest, U9_ZoneRemainingSecondsRoundsUp) {
    const ZoneCell zone{{3, 3}, PlayerId::Black, 3000};
    EXPECT_EQ(zoneRemainingSeconds(zone, 0), 3);
    EXPECT_EQ(zoneRemainingSeconds(zone, 1001), 2);
    EXPECT_EQ(zoneRemainingSeconds(zone, 2999), 1);
    EXPECT_EQ(zoneRemainingSeconds(zone, 3000), 0);
}

TEST(BoardOverlayTest, U11_ClearedScoreTextSumsLines) {
    const ClearedLine five{PlayerId::Black, {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}};
    const ClearedLine six{PlayerId::Black, {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}}};
    EXPECT_EQ(clearedScoreText({five}), "+5");
    EXPECT_EQ(clearedScoreText({five, six}), "+11");
    EXPECT_EQ(clearedScoreText({}), "");
}

TEST(BoardOverlayTest, U11_LabelAtMiddleStoneOfFirstLine) {
    const ClearedLine five{PlayerId::Black, {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}};
    EXPECT_EQ(clearedLabelCell({five}), std::optional<Pos>(Pos{2, 0}));
    EXPECT_EQ(clearedLabelCell({}), std::nullopt);
}

TEST(BoardOverlayTest, U11_FlashLastsAbout500Ms) {
    EXPECT_EQ(kClearFlashDurationMs, 500);
}
