#include <gtest/gtest.h>

#include <iterator>
#include <variant>

#include "core/config.h"
#include "core/types.h"

TEST(TypesTest, T03_OpponentOfBlackIsWhite) {
    EXPECT_EQ(opponent(PlayerId::Black), PlayerId::White);
}

TEST(TypesTest, T03_OpponentOfWhiteIsBlack) {
    EXPECT_EQ(opponent(PlayerId::White), PlayerId::Black);
}

TEST(TypesTest, T03_OpponentTwiceIsSelf) {
    EXPECT_EQ(opponent(opponent(PlayerId::Black)), PlayerId::Black);
    EXPECT_EQ(opponent(opponent(PlayerId::White)), PlayerId::White);
}

TEST(TypesTest, T03_ActionHoldsPlaceOrSkill) {
    Action place = PlaceAction{PlayerId::Black, Pos{7, 7}};
    Action bomb = SkillAction{PlayerId::White, SkillId::Bomb, Pos{3, 4}};
    EXPECT_TRUE(std::holds_alternative<PlaceAction>(place));
    EXPECT_TRUE(std::holds_alternative<SkillAction>(bomb));
}

// spec §4、§5、G2、§7、A2、A10 的表格數值
TEST(ConfigTest, Sec4To8_DefaultsMatchSpecTables) {
    const MatchConfig config;
    EXPECT_EQ(config.regenInterval, 2000);
    EXPECT_EQ(config.maxEnergy, 10);
    EXPECT_EQ(config.startEnergy, 1);
    EXPECT_EQ(config.placeCooldown, 500);
    EXPECT_EQ(config.countdown, 3000);
    EXPECT_EQ(config.skill.bombCost, 2);
    EXPECT_EQ(config.skill.dominateCost, 3);
    EXPECT_EQ(config.skill.destroyCost, 3);
    EXPECT_EQ(config.skill.bombSize, 2);
    EXPECT_EQ(config.ai.reactionTime, 350);
    EXPECT_DOUBLE_EQ(config.ai.defenseWeight, 0.8);
}

// spec G1b、W1、SZ、SX、M1a 的預設值（技能沒有冷卻，S3）
TEST(ConfigTest, G1b_SZ_SX_NewDefaultsMatchSpec) {
    const MatchConfig config;
    EXPECT_EQ(config.minLineLength, 5);
    EXPECT_EQ(config.mode, MatchMode::TimeLimit);
    EXPECT_EQ(config.timeLimit, 180000);
    EXPECT_EQ(config.targetScore, 20);
    EXPECT_FALSE(config.showAiInfo);
    EXPECT_EQ(config.skill.dominateStones, 3);
    EXPECT_EQ(config.skill.zoneDuration, 3000);
    EXPECT_EQ(config.skill.destroyRadius, 2);  // 5×5
}

TEST(TypesTest, B2_S1_NewEnumValues) {
    EXPECT_NE(Cell::Destroyed, Cell::Empty);
    const SkillId all[] = {SkillId::Bomb, SkillId::Dominate, SkillId::Destroy};  // 加速已刪除
    EXPECT_EQ(std::size(all), 3u);
    const RejectReason added[] = {RejectReason::DestroyedCell, RejectReason::RestrictedZone,
                                  RejectReason::SkillUsedUp};
    EXPECT_EQ(std::size(added), 3u);
}

TEST(TypesTest, W1_SZ4_ClearedLineAndZoneCell) {
    const ClearedLine line{PlayerId::White, {Pos{0, 0}, Pos{1, 0}, Pos{2, 0}, Pos{3, 0}, Pos{4, 0}}};
    EXPECT_EQ(line.owner, PlayerId::White);
    EXPECT_EQ(line.stones.size(), 5u);
    const ZoneCell zone{Pos{3, 4}, PlayerId::Black, 4500};
    EXPECT_EQ(zone.pos, (Pos{3, 4}));
    EXPECT_EQ(zone.owner, PlayerId::Black);
    EXPECT_EQ(zone.expiresAt, 4500);
}
