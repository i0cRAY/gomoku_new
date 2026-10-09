#include <gtest/gtest.h>

#include <set>
#include <utility>

#include "board_test_util.h"
#include "core/ai_engine.h"
#include "core/config.h"

namespace {

constexpr double kWeight = 0.8;

AIEngine blackEngine(std::uint32_t seed = 1) {
    return AIEngine(PlayerId::Black, 700, kWeight, seed);
}

// 第 7 列放一排棋子（避免碰到邊界），回傳 (x, 7) 的分數
double scoreOnRow(std::string_view row, int x) {
    const Board board = boardFromRows({"", "", "", "", "", "", "", row});
    return blackEngine().score(board, {x, 7});
}

}  // namespace

// ---- A10：評分表 ----

TEST(ScoreTest, A10_PatternScoreTable) {
    EXPECT_EQ(AIEngine::patternScore(Pattern::Five), 100000);
    EXPECT_EQ(AIEngine::patternScore(Pattern::OpenFour), 10000);
    EXPECT_EQ(AIEngine::patternScore(Pattern::Four), 1000);
    EXPECT_EQ(AIEngine::patternScore(Pattern::OpenThree), 1000);
    EXPECT_EQ(AIEngine::patternScore(Pattern::Three), 100);
    EXPECT_EQ(AIEngine::patternScore(Pattern::OpenTwo), 100);
    EXPECT_EQ(AIEngine::patternScore(Pattern::Two), 10);
    EXPECT_EQ(AIEngine::patternScore(Pattern::None), 0);
}

TEST(ScoreTest, A10_AttackScoreSumsOwnPatterns) {
    EXPECT_DOUBLE_EQ(scoreOnRow("..XXXX...", 6), 100000);  // 連五
    EXPECT_DOUBLE_EQ(scoreOnRow("..XXX....", 5), 10000);   // 活四
    EXPECT_DOUBLE_EQ(scoreOnRow("...XX....", 5), 1000);    // 活三
    EXPECT_DOUBLE_EQ(scoreOnRow("....X....", 5), 100);     // 活二
}

TEST(ScoreTest, A10_DefenseScoreIsWeighted) {
    const Board board = boardFromRows({"", "", "", "", "", "", "", "..OOO...."});
    EXPECT_DOUBLE_EQ(blackEngine().score(board, {5, 7}), 10000 * kWeight);  // 擋對手的活四
}

TEST(ScoreTest, A10_AttackPlusDefense) {
    // (5,7)：黑方橫向活三（1000）＋ 白方直向活三（1000 × 0.8）
    const Board board = boardFromRows({"", "", "", "", "", ".....O", ".....O", "...XX....", "", ""});
    EXPECT_DOUBLE_EQ(blackEngine().score(board, {5, 7}), 1000 + 1000 * kWeight);
}

TEST(ScoreTest, A10_DoubleOpenThreeBonus) {
    // (5,7)：橫向與直向都形成活三 → 1000 + 1000 + 8000
    const Board board = boardFromRows({"", "", "", "", "", ".....X", ".....X", "...XX....", "", ""});
    EXPECT_DOUBLE_EQ(blackEngine().score(board, {5, 7}), 1000 + 1000 + 8000);
}

TEST(ScoreTest, A10_FourPlusOpenThreeGetsBonus) {
    // (5,7)：橫向衝四（左邊被擋）＋ 直向活三
    const Board board = boardFromRows({"", "", "", "", "", ".....X", ".....X", ".OXXX....", "", ""});
    EXPECT_DOUBLE_EQ(blackEngine().score(board, {5, 7}), 1000 + 1000 + 8000);
}

TEST(ScoreTest, A10_SingleOpenThreeHasNoBonus) {
    EXPECT_DOUBLE_EQ(scoreOnRow("...XX....", 5), 1000);
}

TEST(ScoreTest, A10_DefenseBonusAlsoWeighted) {
    const Board board = boardFromRows({"", "", "", "", "", ".....O", ".....O", "...OO....", "", ""});
    EXPECT_DOUBLE_EQ(blackEngine().score(board, {5, 7}), (1000 + 1000 + 8000) * kWeight);
}

TEST(ScoreTest, A10_ScoresComeFromConfig) {
    PatternScores scores;
    scores.openFour = 5;
    const AIEngine engine(PlayerId::Black, 700, kWeight, 1, scores);
    const Board board = boardFromRows({"", "", "", "", "", "", "", "..XXX...."});
    EXPECT_DOUBLE_EQ(engine.score(board, {5, 7}), 5);
}

// ---- A11、A12：挑選位置 ----

TEST(ScoreTest, A12_EmptyBoardPlaysCenter) {
    EXPECT_EQ(blackEngine().bestPlacement(Board{}), std::optional<Pos>(Pos{7, 7}));
}

TEST(ScoreTest, A10_BestPlacementTakesHighestScore) {
    const Board board = boardFromRows({"", "", "", ".OXXXX...", "", "", "......OO."});
    EXPECT_EQ(blackEngine().bestPlacement(board), std::optional<Pos>(Pos{6, 3}));  // 下這裡成五
}

TEST(ScoreTest, A11_SameSeedPicksSameTiedCell) {
    Board board;
    board.set({7, 7}, Cell::Black);  // 周圍 8 個方向對稱，有多格同分
    for (std::uint32_t seed : {1u, 7u, 42u}) {
        AIEngine a = blackEngine(seed);
        AIEngine b = blackEngine(seed);
        EXPECT_EQ(a.bestPlacement(board), b.bestPlacement(board)) << "seed " << seed;
    }
}

TEST(ScoreTest, A11_TiesArePickedRandomlyAmongBest) {
    Board board;
    board.set({7, 7}, Cell::Black);
    const AIEngine reference = blackEngine();
    double best = 0;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            if (board.isEmpty({x, y})) {
                best = std::max(best, reference.score(board, {x, y}));
            }
        }
    }

    std::set<std::pair<int, int>> picks;
    for (std::uint32_t seed = 0; seed < 20; ++seed) {
        AIEngine engine = blackEngine(seed);
        const auto pick = engine.bestPlacement(board);
        ASSERT_TRUE(pick.has_value());
        EXPECT_DOUBLE_EQ(reference.score(board, *pick), best);  // 一定是最高分之一
        picks.insert({pick->x, pick->y});
    }
    EXPECT_GT(picks.size(), 1u);  // 不同 seed 會挑到不同格
}

TEST(ScoreTest, A11_FullBoardHasNoPlacement) {
    Board board;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            board.set({x, y}, ((x / 2) + y) % 2 == 0 ? Cell::Black : Cell::White);
        }
    }
    EXPECT_EQ(blackEngine().bestPlacement(board), std::nullopt);
}
