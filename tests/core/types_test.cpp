#include <gtest/gtest.h>

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
    EXPECT_EQ(config.placeCooldown, 1000);
    EXPECT_EQ(config.countdown, 3000);
    EXPECT_EQ(config.skill.accelerateCooldown, 25000);
    EXPECT_EQ(config.skill.accelerateDuration, 5000);
    EXPECT_EQ(config.skill.bombCooldown, 20000);
    EXPECT_EQ(config.skill.accelerateRegenMultiplier, 2);
    EXPECT_EQ(config.ai.reactionEasy, 1200);
    EXPECT_EQ(config.ai.reactionNormal, 700);
    EXPECT_EQ(config.ai.reactionHard, 350);
    EXPECT_DOUBLE_EQ(config.ai.defenseWeight, 0.8);
}
