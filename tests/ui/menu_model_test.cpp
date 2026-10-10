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

// ---- U7：技能說明包含效果與能量消耗 ----

TEST(MenuModelTest, U7_SB3_BombDescriptionShowsAreaAndCost) {
    const std::string text = skillDescription(SkillId::Bomb, SkillConfig{});
    EXPECT_NE(text.find("2×2"), std::string::npos);
    EXPECT_NE(text.find("雙方"), std::string::npos);
    EXPECT_NE(text.find("消耗 2 格能量"), std::string::npos);
    EXPECT_EQ(text.find("冷卻"), std::string::npos);
}

TEST(MenuModelTest, U7_DescriptionFollowsConfig) {
    SkillConfig config;
    config.bombCost = 4;
    config.dominateCost = 5;
    config.destroyCost = 6;
    EXPECT_NE(skillDescription(SkillId::Bomb, config).find("消耗 4 格能量"), std::string::npos);
    EXPECT_NE(skillDescription(SkillId::Dominate, config).find("消耗 5 格能量"), std::string::npos);
    EXPECT_NE(skillDescription(SkillId::Destroy, config).find("消耗 6 格能量"), std::string::npos);
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

// ---- G1b：比賽模式 ----

TEST(MenuModelTest, G1b_TimeLimitOptionsAreFiveThreeOneMinutes) {
    const auto options = timeLimitOptions();
    ASSERT_EQ(options.size(), 3u);
    EXPECT_EQ(options[0], 300000);
    EXPECT_EQ(options[1], 180000);
    EXPECT_EQ(options[2], 60000);
    EXPECT_EQ(options[defaultTimeLimitIndex()], MatchConfig{}.timeLimit);
    EXPECT_EQ(timeLimitLabel(300000), "5 分鐘");
    EXPECT_EQ(timeLimitLabel(60000), "1 分鐘");
}

TEST(MenuModelTest, G1b_TargetScoreRange) {
    EXPECT_EQ(kMinTargetScore, 5);
    EXPECT_EQ(kMaxTargetScore, 100);
    EXPECT_GE(MatchConfig{}.targetScore, kMinTargetScore);
    EXPECT_LE(MatchConfig{}.targetScore, kMaxTargetScore);
}

TEST(MenuModelTest, G1b_ModeLabels) {
    EXPECT_EQ(matchModeLabel(MatchMode::TimeLimit), "限時");
    EXPECT_EQ(matchModeLabel(MatchMode::ScoreTarget), "達分");
}

// ---- U6：結束畫面顯示勝負與分數 ----

TEST(MenuModelTest, U6_FinalResultTextIncludesScores) {
    EXPECT_EQ(finalResultText(GameStatus::BlackWon, {10, 5}), "黑方獲勝\n黑 10 : 5 白");
    EXPECT_EQ(finalResultText(GameStatus::Draw, {0, 0}), "和局\n黑 0 : 0 白");
    EXPECT_EQ(finalResultText(GameStatus::Running, {1, 2}), "");
}

// ---- U7：四項技能 ----

TEST(MenuModelTest, U7_SkillOptionsListAllThree) {
    const auto skills = skillOptions();
    ASSERT_EQ(skills.size(), 3u);
    EXPECT_EQ(skills[0], SkillId::Bomb);
    EXPECT_EQ(skills[1], SkillId::Dominate);
    EXPECT_EQ(skills[2], SkillId::Destroy);
}

TEST(MenuModelTest, U7_DominateDescription) {
    const std::string text = skillDescription(SkillId::Dominate, SkillConfig{});
    EXPECT_NE(text.find("3 顆"), std::string::npos);
    EXPECT_NE(text.find("3 秒"), std::string::npos);
    EXPECT_NE(text.find("消耗 3 格能量"), std::string::npos);
}

TEST(MenuModelTest, U7_DestroyDescriptionMentionsOncePerGame) {
    const std::string text = skillDescription(SkillId::Destroy, SkillConfig{});
    EXPECT_NE(text.find("5×5"), std::string::npos);
    EXPECT_NE(text.find("每局一次"), std::string::npos);
    EXPECT_NE(text.find("消耗 3 格能量"), std::string::npos);
}
