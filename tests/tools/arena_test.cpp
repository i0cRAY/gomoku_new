#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "arena.h"

// spec §11：AI 強化的跑分工具（design §4.8）

TEST(ArenaRateTest, Spec11_WinRateWithInterval) {
    const RateInterval even = winRate(200, 400);
    EXPECT_DOUBLE_EQ(even.rate, 0.5);
    EXPECT_NEAR(even.low, 0.451, 0.0005);  // 0.5 ± 1.96 × √(0.25 / 400)
    EXPECT_NEAR(even.high, 0.549, 0.0005);

    const RateInterval strong = winRate(304, 400);
    EXPECT_DOUBLE_EQ(strong.rate, 0.76);
    EXPECT_NEAR(strong.low, 0.7181, 0.0005);
    EXPECT_NEAR(strong.high, 0.8019, 0.0005);
}

TEST(ArenaRateTest, Spec11_DrawCountsAsHalf) {
    ArenaResult result;
    result.games = 4;
    result.wins = 1;
    result.draws = 2;
    result.losses = 1;
    EXPECT_DOUBLE_EQ(result.points(), 2.0);
    EXPECT_DOUBLE_EQ(winRate(result.points(), result.games).rate, 0.5);
}

TEST(ArenaRateTest, Spec11_IntervalStaysWithinZeroAndOne) {
    const RateInterval all = winRate(10, 10);
    EXPECT_DOUBLE_EQ(all.rate, 1.0);
    EXPECT_DOUBLE_EQ(all.high, 1.0);
    const RateInterval none = winRate(0, 10);
    EXPECT_DOUBLE_EQ(none.low, 0.0);
}

TEST(ArenaArgsTest, Spec11_Defaults) {
    std::string error;
    const auto options = parseArenaArgs({}, error);
    ASSERT_TRUE(options.has_value()) << error;
    EXPECT_EQ(options->games, 400);
    EXPECT_EQ(options->modes, (std::vector<MatchMode>{MatchMode::TimeLimit, MatchMode::ScoreTarget}));
    EXPECT_EQ(options->skill, ArenaSkill::None);
    EXPECT_EQ(options->a, ArenaEngine::New);
    EXPECT_EQ(options->b, ArenaEngine::Baseline);
    EXPECT_FALSE(options->reserve.has_value());
    EXPECT_FALSE(options->vcfMargin.has_value());
}

TEST(ArenaArgsTest, Spec11_ParsesAllOptions) {
    std::string error;
    const auto options = parseArenaArgs(
        {"--games", "12", "--mode", "score", "--seed", "7", "--skill", "destroy", "--a", "baseline", "--b", "new",
         "--reserve", "3", "--vcf-margin", "10"},
        error);
    ASSERT_TRUE(options.has_value()) << error;
    EXPECT_EQ(options->games, 12);
    EXPECT_EQ(options->modes, std::vector<MatchMode>{MatchMode::ScoreTarget});
    EXPECT_EQ(options->seed, 7u);
    EXPECT_EQ(options->skill, ArenaSkill::Destroy);
    EXPECT_EQ(options->a, ArenaEngine::Baseline);
    EXPECT_EQ(options->b, ArenaEngine::New);
    EXPECT_EQ(options->reserve, std::optional<int>(3));
    EXPECT_EQ(options->vcfMargin, std::optional<int>(10));
}

TEST(ArenaArgsTest, Spec11_RejectsInvalidArguments) {
    for (const std::vector<std::string>& args : std::vector<std::vector<std::string>>{
             {"--games", "0"}, {"--games", "abc"}, {"--games"}, {"--mode", "fast"}, {"--skill", "bomb"},
             {"--a", "old"}, {"--seed", "-1"}, {"--reserve", "x"}, {"--vcf-margin", "-2"}, {"--unknown"}}) {
        std::string error;
        EXPECT_FALSE(parseArenaArgs(args, error).has_value()) << args[0];
        EXPECT_FALSE(error.empty());
    }
}

namespace {

// Debug 建置一場要 1–2 秒，單元測試只跑 2 場（一場執黑、一場執白）
ArenaOptions twoGames(ArenaSkill skill) {
    ArenaOptions options;
    options.games = 2;
    options.seed = 3;
    options.skill = skill;
    return options;
}

}  // namespace

TEST(ArenaRunTest, Spec11_EveryGameFinishesAndColorsAlternate) {
    for (MatchMode mode : {MatchMode::TimeLimit, MatchMode::ScoreTarget}) {
        const ArenaResult result = runArena(twoGames(ArenaSkill::None), mode);
        EXPECT_EQ(result.games, 2);
        EXPECT_EQ(result.wins + result.losses + result.draws, 2);
        EXPECT_EQ(result.unfinished, 0);
        EXPECT_EQ(result.blackGames, 1);
        EXPECT_EQ(result.whiteGames, 1);
        EXPECT_GT(result.a.decisions, 0);
        EXPECT_GT(result.b.decisions, 0);
        EXPECT_GT(result.a.score + result.b.score, 0);
    }
}

TEST(ArenaRunTest, Spec11_SameOptionsGiveSameResult) {
    const ArenaResult first = runArena(twoGames(ArenaSkill::Random), MatchMode::ScoreTarget);
    const ArenaResult second = runArena(twoGames(ArenaSkill::Random), MatchMode::ScoreTarget);
    EXPECT_EQ(first.wins, second.wins);
    EXPECT_EQ(first.losses, second.losses);
    EXPECT_EQ(first.draws, second.draws);
    EXPECT_DOUBLE_EQ(first.blackPoints, second.blackPoints);
    for (auto side : {&ArenaResult::a, &ArenaResult::b}) {
        EXPECT_EQ((first.*side).score, (second.*side).score);
        EXPECT_EQ((first.*side).skillUses, (second.*side).skillUses);
        EXPECT_EQ((first.*side).rejections, (second.*side).rejections);
        EXPECT_EQ((first.*side).decisions, (second.*side).decisions);
    }
}

TEST(ArenaRunTest, Spec11_DifferentSeedGivesDifferentGames) {
    ArenaOptions other = twoGames(ArenaSkill::None);
    other.seed = 4;
    const ArenaResult a = runArena(twoGames(ArenaSkill::None), MatchMode::TimeLimit);
    const ArenaResult b = runArena(other, MatchMode::TimeLimit);
    EXPECT_NE(a.a.score * 1000 + a.b.score, b.a.score * 1000 + b.b.score);
}

TEST(ArenaRunTest, Spec11_NoSkillModeNeverUsesSkills) {
    const ArenaResult result = runArena(twoGames(ArenaSkill::None), MatchMode::TimeLimit);
    EXPECT_EQ(result.a.skillUses, 0);
    EXPECT_EQ(result.b.skillUses, 0);
}

TEST(ArenaRunTest, Spec11_ForcedSkillIsUsed) {
    const ArenaResult result = runArena(twoGames(ArenaSkill::Destroy), MatchMode::TimeLimit);
    EXPECT_GT(result.a.skillUses, 0);
    EXPECT_GT(result.b.skillUses, 0);
    EXPECT_LE(result.a.skillUses, 2);  // SX3：每局最多一次
    EXPECT_LE(result.b.skillUses, 2);
}

TEST(ArenaRunTest, Spec11_SameEngineOnBothSidesIsSymmetric) {
    ArenaOptions options = twoGames(ArenaSkill::None);
    options.a = ArenaEngine::Baseline;
    options.b = ArenaEngine::Baseline;
    const ArenaResult result = runArena(options, MatchMode::ScoreTarget);
    EXPECT_EQ(result.games, 2);
    EXPECT_EQ(result.unfinished, 0);
}

TEST(ArenaFormatTest, Spec11_ReportShowsRateAndRejections) {
    ArenaOptions options = twoGames(ArenaSkill::None);
    ArenaResult result;
    result.games = 4;
    result.wins = 3;
    result.losses = 1;
    result.blackGames = 2;
    result.whiteGames = 2;
    result.blackPoints = 2;
    result.whitePoints = 1;
    result.b.rejections[RejectReason::NoEnergy] = 2;
    const std::string text = formatResult(options, MatchMode::TimeLimit, result);
    EXPECT_NE(text.find("75.0%"), std::string::npos) << text;
    EXPECT_NE(text.find("能量不足 2"), std::string::npos) << text;
    EXPECT_NE(text.find("限時"), std::string::npos) << text;
}

TEST(ArenaRunTest, A15_ReserveOptionChangesNewEngineOnly) {
    ArenaOptions keep = twoGames(ArenaSkill::None);
    keep.reserve = 9;  // 幾乎不下不緊急的棋
    const ArenaResult held = runArena(keep, MatchMode::ScoreTarget);
    const ArenaResult normal = runArena(twoGames(ArenaSkill::None), MatchMode::ScoreTarget);
    EXPECT_NE(held.a.score * 1000 + held.b.score, normal.a.score * 1000 + normal.b.score);
}
