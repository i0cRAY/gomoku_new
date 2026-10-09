#include <gtest/gtest.h>

#include <string>

#include "ui/menu_model.h"

// ---- G1：回能間隔只提供合法值 ----

TEST(MenuModelTest, G1_RegenIntervalOptionsAre500To10000Step500) {
    const auto options = regenIntervalOptions();
    ASSERT_EQ(options.size(), 20u);
    EXPECT_EQ(options.front(), 500);
    EXPECT_EQ(options.back(), 10000);
    for (std::size_t i = 1; i < options.size(); ++i) {
        EXPECT_EQ(options[i] - options[i - 1], 500);
    }
}

TEST(MenuModelTest, G1_DefaultIntervalIsInOptions) {
    const auto options = regenIntervalOptions();
    const std::size_t index = defaultRegenIntervalIndex();
    ASSERT_LT(index, options.size());
    EXPECT_EQ(options[index], MatchConfig{}.regenInterval);
}

TEST(MenuModelTest, G1_IntervalLabelInSeconds) {
    EXPECT_EQ(regenIntervalLabel(500), "0.5 秒");
    EXPECT_EQ(regenIntervalLabel(2000), "2 秒");
    EXPECT_EQ(regenIntervalLabel(7500), "7.5 秒");
}

// ---- U7：技能說明包含效果與冷卻 ----

TEST(MenuModelTest, U7_AccelerateDescriptionShowsEffectAndCooldown) {
    const std::string text = skillDescription(SkillId::Accelerate, SkillConfig{});
    EXPECT_NE(text.find("2 倍"), std::string::npos);
    EXPECT_NE(text.find("5 秒"), std::string::npos);
    EXPECT_NE(text.find("冷卻 25 秒"), std::string::npos);
}

TEST(MenuModelTest, U7_BombDescriptionShowsCooldown) {
    const std::string text = skillDescription(SkillId::Bomb, SkillConfig{});
    EXPECT_NE(text.find("冷卻 20 秒"), std::string::npos);
}

TEST(MenuModelTest, U7_DescriptionFollowsConfig) {
    SkillConfig config;
    config.bombCooldown = 15000;
    EXPECT_NE(skillDescription(SkillId::Bomb, config).find("冷卻 15 秒"), std::string::npos);
}

// ---- G2：倒數文字 ----

TEST(MenuModelTest, G2_CountdownTextRoundsUp) {
    EXPECT_EQ(countdownText(3000), "3");
    EXPECT_EQ(countdownText(2001), "3");
    EXPECT_EQ(countdownText(2000), "2");
    EXPECT_EQ(countdownText(1), "1");
}

// ---- G4、U6：結果文字 ----

TEST(MenuModelTest, U6_ResultText) {
    EXPECT_EQ(resultText(GameStatus::BlackWon), "黑方獲勝");
    EXPECT_EQ(resultText(GameStatus::WhiteWon), "白方獲勝");
    EXPECT_EQ(resultText(GameStatus::Draw), "和局");
    EXPECT_EQ(resultText(GameStatus::Aborted), "連線中斷");
    EXPECT_EQ(resultText(GameStatus::Running), "");
}

TEST(MenuModelTest, G4_IsFinished) {
    EXPECT_TRUE(isFinished(GameStatus::BlackWon));
    EXPECT_TRUE(isFinished(GameStatus::WhiteWon));
    EXPECT_TRUE(isFinished(GameStatus::Draw));
    EXPECT_TRUE(isFinished(GameStatus::Aborted));
    EXPECT_FALSE(isFinished(GameStatus::Running));
    EXPECT_FALSE(isFinished(GameStatus::Countdown));
    EXPECT_FALSE(isFinished(GameStatus::SkillSelect));
}

// ---- G1、M1：AI 難度 ----

TEST(MenuModelTest, G1_M1_DifficultyOptionsInOrder) {
    const auto options = difficultyOptions();
    ASSERT_EQ(options.size(), 3u);
    EXPECT_EQ(options[0], Difficulty::Easy);
    EXPECT_EQ(options[1], Difficulty::Normal);
    EXPECT_EQ(options[2], Difficulty::Hard);
    EXPECT_EQ(options[defaultDifficultyIndex()], MatchConfig{}.ai.difficulty);
}

TEST(MenuModelTest, A2_DifficultyLabels) {
    EXPECT_EQ(difficultyLabel(Difficulty::Easy), "簡單");
    EXPECT_EQ(difficultyLabel(Difficulty::Normal), "普通");
    EXPECT_EQ(difficultyLabel(Difficulty::Hard), "困難");
}
