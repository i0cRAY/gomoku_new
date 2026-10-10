#include <gtest/gtest.h>

#include <algorithm>
#include <tuple>

#include "ui/board_overlay.h"

TEST(BoardOverlayTest, U3_DestroyPreviewCenteredOnTarget) {
    SkillConfig config;
    config.destroyRadius = 4;
    const auto area = skillArea(SkillId::Destroy, config);
    ASSERT_TRUE(area.has_value());
    const auto cells = areaCells({7, 7}, *area);
    EXPECT_EQ(cells.size(), 81u);
    EXPECT_NE(std::find(cells.begin(), cells.end(), Pos{3, 3}), cells.end());
    EXPECT_NE(std::find(cells.begin(), cells.end(), Pos{11, 11}), cells.end());
    EXPECT_EQ(std::find(cells.begin(), cells.end(), Pos{2, 7}), cells.end());
}

TEST(BoardOverlayTest, U3_DestroyPreviewClippedAtCorner) {
    SkillConfig config;
    config.destroyRadius = 4;
    const SkillArea area = *skillArea(SkillId::Destroy, config);
    EXPECT_EQ(areaCells({0, 0}, area).size(), 25u);
    EXPECT_EQ(areaCells({14, 7}, area).size(), 45u);  // 5 × 9
}

TEST(BoardOverlayTest, U3_BombPreviewIsTwoByTwoFromTopLeft) {
    const auto area = skillArea(SkillId::Bomb, SkillConfig{});
    ASSERT_TRUE(area.has_value());
    auto cells = areaCells({7, 7}, *area);
    std::sort(cells.begin(), cells.end(), [](Pos a, Pos b) { return std::tie(a.y, a.x) < std::tie(b.y, b.x); });
    EXPECT_EQ(cells, (std::vector<Pos>{{7, 7}, {8, 7}, {7, 8}, {8, 8}}));
    EXPECT_EQ(areaCells({14, 14}, *area), (std::vector<Pos>{{14, 14}}));  // 超出棋盤的部分截掉
}

TEST(BoardOverlayTest, U3_DominateHasNoPreview) {
    EXPECT_EQ(skillArea(SkillId::Dominate, SkillConfig{}), std::nullopt);
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
