#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "app/game_controller.h"

namespace {

PlaceAction white(int x, int y) {
    return PlaceAction{PlayerId::White, Pos{x, y}};
}

std::vector<Pos> sorted(std::vector<Pos> v) {
    std::sort(v.begin(), v.end(), [](Pos a, Pos b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return v;
}

struct GameOverEvent {
    GameStatus status;
    std::array<int, 2> scores;
    std::optional<std::array<int, 2>> boardScores;
};

}  // namespace

class ScoringTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.regenInterval = 500;  // 能量充足，只受下子間隔限制
    }

    void start(SkillId blackSkill = SkillId::Dominate, SkillId whiteSkill = SkillId::Bomb) {
        game = std::make_unique<GameController>(config);
        QObject::connect(game.get(), &GameController::linesCleared,
                         [this](std::vector<ClearedLine> lines) { cleared.push_back(lines); });
        QObject::connect(game.get(), &GameController::gameOver,
                         [this](GameStatus s, std::array<int, 2> scores, std::optional<std::array<int, 2>> board) {
                             gameOvers.push_back({s, scores, board});
                         });
        game->selectSkill(PlayerId::Black, blackSkill);
        game->selectSkill(PlayerId::White, whiteSkill);
        game->confirmSkill(PlayerId::Black);
        game->confirmSkill(PlayerId::White);
        game->tick(0);
    }

    // 黑方依序下 cells，每子間隔 1000 ms，從 startAt 開始；回傳下一個可用時間
    TimeMs placeBlack(const std::vector<Pos>& cells, TimeMs startAt = 0) {
        TimeMs t = startAt;
        for (Pos p : cells) {
            EXPECT_TRUE(game->submit(PlaceAction{PlayerId::Black, p}, t).accepted) << p.x << "," << p.y;
            t += 1000;
        }
        return t;
    }

    std::array<int, 2> scores() const { return game->viewFor(PlayerId::Black).scores; }

    MatchConfig config;
    std::unique_ptr<GameController> game;
    std::vector<std::vector<ClearedLine>> cleared;
    std::vector<GameOverEvent> gameOvers;
};

// ---- W1、W2、W6：消除與計分 ----

TEST_F(ScoringTest, W1_FiveScoresFiveAndClearsLine) {
    start();
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}});
    EXPECT_EQ(scores(), (std::array<int, 2>{5, 0}));
    ASSERT_EQ(cleared.size(), 1u);
    ASSERT_EQ(cleared[0].size(), 1u);
    EXPECT_EQ(cleared[0][0].owner, PlayerId::Black);
    EXPECT_EQ(sorted(cleared[0][0].stones), sorted({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}));
    const PlayerView view = game->viewFor(PlayerId::White);
    EXPECT_EQ(view.lastClearedLines.size(), 1u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(view.board.isEmpty({i, 0}));
    }
    EXPECT_EQ(view.status, GameStatus::Running);
}

TEST_F(ScoringTest, W2_SixInARowScoresSix) {
    start();
    // 先下兩端與中間留一個缺口，最後補上形成 6 連
    placeBlack({{0, 3}, {1, 3}, {2, 3}, {4, 3}, {5, 3}, {3, 3}});
    EXPECT_EQ(scores(), (std::array<int, 2>{6, 0}));
    ASSERT_EQ(cleared.size(), 1u);
    EXPECT_EQ(cleared[0][0].stones.size(), 6u);
}

TEST_F(ScoringTest, W6_CrossFiveScoresEachLine) {
    start();
    placeBlack({{3, 7}, {4, 7}, {5, 7}, {6, 7}, {7, 3}, {7, 4}, {7, 5}, {7, 6}, {7, 7}});
    EXPECT_EQ(scores(), (std::array<int, 2>{10, 0}));  // 交叉點在兩條線各算一次
    ASSERT_EQ(cleared.size(), 1u);
    EXPECT_EQ(cleared[0].size(), 2u);
    const Board board = game->viewFor(PlayerId::Black).board;
    EXPECT_TRUE(board.isEmpty({7, 7}));
    EXPECT_TRUE(board.isEmpty({3, 7}));
    EXPECT_TRUE(board.isEmpty({7, 3}));
}

TEST_F(ScoringTest, W1_FourDoesNotScore) {
    start();
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}});
    EXPECT_EQ(scores(), (std::array<int, 2>{0, 0}));
    EXPECT_TRUE(cleared.empty());
}

TEST_F(ScoringTest, W3_BombNeverScores) {
    start(SkillId::Bomb, SkillId::Bomb);
    // 黑方四子，白子在 (4, 0) 擋住；黑方炸掉白子也不會得分
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}});
    ASSERT_TRUE(game->submit(white(4, 0), 0).accepted);
    ASSERT_TRUE(game->submit(SkillAction{PlayerId::Black, SkillId::Bomb, Pos{4, 0}}, 5000).accepted);
    EXPECT_EQ(scores(), (std::array<int, 2>{0, 0}));
    EXPECT_TRUE(cleared.empty());
}

TEST_F(ScoringTest, W3_DestroyNeverScores) {
    config.startEnergy = config.maxEnergy;
    start(SkillId::Destroy, SkillId::Bomb);
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}});
    ASSERT_TRUE(game->submit(SkillAction{PlayerId::Black, SkillId::Destroy, Pos{10, 10}}, 5000).accepted);
    EXPECT_EQ(scores(), (std::array<int, 2>{0, 0}));
    EXPECT_TRUE(cleared.empty());
}

// ---- W7：限時模式 ----

TEST_F(ScoringTest, W7_RequestAtTimeLimitRejectedAndGameEnds) {
    config.mode = MatchMode::TimeLimit;
    config.timeLimit = 60000;
    start();
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}});  // 黑方 5 分
    EXPECT_TRUE(game->submit(white(7, 7), 59999).accepted);
    EXPECT_EQ(game->submit(white(8, 8), 60000).reason, RejectReason::GameNotRunning);
    EXPECT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::BlackWon);  // W9
    ASSERT_EQ(gameOvers.size(), 1u);
    EXPECT_EQ(gameOvers[0].scores, (std::array<int, 2>{5, 0}));
}

TEST_F(ScoringTest, W7_TickPastLimitEndsGame) {
    config.mode = MatchMode::TimeLimit;
    config.timeLimit = 60000;
    start();
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}});  // 黑方 5 分，不會進延長賽（W10）
    game->tick(59950);
    EXPECT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::Running);
    EXPECT_EQ(game->viewFor(PlayerId::Black).timeRemaining, 50);
    game->tick(60010);
    const PlayerView view = game->viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::BlackWon);  // W9
    EXPECT_EQ(view.timeRemaining, 0);
    EXPECT_EQ(view.now, 60000);  // 停在時限
    EXPECT_EQ(gameOvers.size(), 1u);
}

TEST_F(ScoringTest, W9_HigherScoreWinsAtTimeLimit) {
    config.mode = MatchMode::TimeLimit;
    config.timeLimit = 60000;
    start();
    TimeMs t = 0;
    for (int i = 0; i < 5; ++i, t += 1000) {
        ASSERT_TRUE(game->submit(white(i, 9), t).accepted);
    }
    game->tick(60000);
    EXPECT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::WhiteWon);
}

TEST_F(ScoringTest, G1b_ViewShowsTimeRemainingDuringCountdownAndMatch) {
    config.mode = MatchMode::TimeLimit;
    config.timeLimit = 300000;
    game = std::make_unique<GameController>(config);
    game->selectSkill(PlayerId::Black, SkillId::Bomb);
    game->selectSkill(PlayerId::White, SkillId::Bomb);
    game->confirmSkill(PlayerId::Black);
    game->confirmSkill(PlayerId::White);
    PlayerView view = game->viewFor(PlayerId::White);
    EXPECT_EQ(view.mode, MatchMode::TimeLimit);
    EXPECT_EQ(view.timeRemaining, 300000);  // 倒數中還沒開始扣
    game->tick(12345);
    EXPECT_EQ(game->viewFor(PlayerId::White).timeRemaining, 300000 - 12345);
}

// ---- W8：達分模式 ----

TEST_F(ScoringTest, W8_ReachingTargetWinsImmediately) {
    config.mode = MatchMode::ScoreTarget;
    config.targetScore = 10;
    start();
    TimeMs t = placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}});
    EXPECT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::Running);  // 5 分未達標
    placeBlack({{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}}, t);
    EXPECT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::BlackWon);
    ASSERT_EQ(gameOvers.size(), 1u);
    EXPECT_EQ(gameOvers[0].status, GameStatus::BlackWon);
    EXPECT_EQ(gameOvers[0].scores, (std::array<int, 2>{10, 0}));
}

TEST_F(ScoringTest, W8_NoTimeLimitInScoreTargetMode) {
    config.mode = MatchMode::ScoreTarget;
    config.targetScore = 20;
    config.timeLimit = 60000;
    start();
    game->tick(600000);
    const PlayerView view = game->viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::Running);
    EXPECT_EQ(view.mode, MatchMode::ScoreTarget);
    EXPECT_EQ(view.targetScore, 20);
}

// ---- E5：分數是公開資訊 ----

TEST_F(ScoringTest, E5_BothPlayersSeeBothScores) {
    start();
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}});
    EXPECT_EQ(game->viewFor(PlayerId::Black).scores, (std::array<int, 2>{5, 0}));
    EXPECT_EQ(game->viewFor(PlayerId::White).scores, (std::array<int, 2>{5, 0}));
}

// ---- W9、W10、W11：延長賽與盤面分決勝 ----

class OvertimeTest : public ScoringTest {
protected:
    void SetUp() override {
        ScoringTest::SetUp();
        config.mode = MatchMode::TimeLimit;
        config.timeLimit = 60000;
        config.overtime = 60000;
    }

    static constexpr TimeMs kLimit = 60000;
    static constexpr TimeMs kEnd = 120000;  // 時限 + 延長賽

    PlayerView view() const { return game->viewFor(PlayerId::Black); }
};

TEST_F(OvertimeTest, W9_DifferentScoresAtLimitEndsWithoutBoardScores) {
    start();
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}});
    game->tick(kLimit);
    EXPECT_EQ(view().status, GameStatus::BlackWon);
    EXPECT_FALSE(view().overtime);
    EXPECT_FALSE(view().boardScores.has_value());
    ASSERT_EQ(gameOvers.size(), 1u);
    EXPECT_FALSE(gameOvers[0].boardScores.has_value());
}

TEST_F(OvertimeTest, W10_TieAtLimitEntersOvertime) {
    start();
    game->tick(kLimit);
    EXPECT_EQ(view().status, GameStatus::Running);
    EXPECT_TRUE(view().overtime);
    EXPECT_EQ(view().timeRemaining, 60000);
    EXPECT_TRUE(gameOvers.empty());
    game->tick(kLimit + 12345);
    EXPECT_EQ(view().timeRemaining, 60000 - 12345);
    EXPECT_TRUE(game->viewFor(PlayerId::White).overtime);  // 公開資訊（U8、N4）
}

TEST_F(OvertimeTest, W10_NonZeroTieAlsoEntersOvertime) {
    start();
    TimeMs t = placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}});
    for (int i = 0; i < 5; ++i, t += 1000) {
        ASSERT_TRUE(game->submit(white(i, 9), t).accepted);
    }
    ASSERT_EQ(scores(), (std::array<int, 2>{5, 5}));
    game->tick(kLimit);
    EXPECT_EQ(view().status, GameStatus::Running);
    EXPECT_TRUE(view().overtime);
}

TEST_F(OvertimeTest, W10_RequestAtLimitAcceptedWhenTied) {
    start();
    EXPECT_TRUE(game->submit(white(7, 7), kLimit).accepted);
    EXPECT_TRUE(view().overtime);
}

TEST_F(OvertimeTest, W10_ScoringDuringOvertimeDecidesWinner) {
    start();
    game->tick(kLimit);
    placeBlack({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}, kLimit);
    EXPECT_EQ(scores(), (std::array<int, 2>{5, 0}));
    EXPECT_EQ(view().status, GameStatus::Running);  // 打滿 1 分鐘，不是驟死
    game->tick(kEnd);
    EXPECT_EQ(view().status, GameStatus::BlackWon);
    EXPECT_FALSE(view().boardScores.has_value());
    ASSERT_EQ(gameOvers.size(), 1u);
    EXPECT_EQ(gameOvers[0].status, GameStatus::BlackWon);
}

TEST_F(OvertimeTest, W11_TieAfterOvertimeDecidedByBoardScore) {
    start();
    ASSERT_TRUE(game->submit(white(3, 9), 0).accepted);
    ASSERT_TRUE(game->submit(white(4, 9), 1000).accepted);  // 白方兩子不算分
    placeBlack({{3, 3}, {4, 3}, {5, 3}}, 1000);             // 黑方連三
    game->tick(kLimit);
    ASSERT_TRUE(view().overtime);
    game->tick(kEnd);
    EXPECT_EQ(view().status, GameStatus::BlackWon);
    EXPECT_EQ(view().boardScores, (std::array<int, 2>{1, 0}));
    EXPECT_EQ(view().scores, (std::array<int, 2>{0, 0}));  // 盤面分不加進分數
    ASSERT_EQ(gameOvers.size(), 1u);
    EXPECT_EQ(gameOvers[0].boardScores, (std::array<int, 2>{1, 0}));
}

TEST_F(OvertimeTest, W11_WhiteWinsByBoardScore) {
    start();
    TimeMs t = 0;
    for (int i = 0; i < 4; ++i, t += 1000) {
        ASSERT_TRUE(game->submit(white(3 + i, 9), t).accepted);  // 白方連四
    }
    game->tick(kEnd);
    EXPECT_EQ(view().status, GameStatus::WhiteWon);
    EXPECT_EQ(view().boardScores, (std::array<int, 2>{0, 2}));
}

TEST_F(OvertimeTest, W11_EqualBoardScoreIsDraw) {
    start();
    game->tick(kLimit);
    game->tick(kEnd);
    EXPECT_EQ(view().status, GameStatus::Draw);
    EXPECT_EQ(view().boardScores, (std::array<int, 2>{0, 0}));
    EXPECT_EQ(view().timeRemaining, 0);
    EXPECT_EQ(view().now, kEnd);
    ASSERT_EQ(gameOvers.size(), 1u);
    EXPECT_EQ(gameOvers[0].status, GameStatus::Draw);
    EXPECT_EQ(gameOvers[0].boardScores, (std::array<int, 2>{0, 0}));
}

TEST_F(OvertimeTest, W10_RequestAtOvertimeEndRejected) {
    start();
    game->tick(kLimit);
    EXPECT_TRUE(game->submit(white(7, 7), kEnd - 1).accepted);
    EXPECT_EQ(game->submit(white(8, 8), kEnd).reason, RejectReason::GameNotRunning);
    EXPECT_EQ(view().status, GameStatus::Draw);
    EXPECT_EQ(gameOvers.size(), 1u);
}

TEST_F(OvertimeTest, W10_SingleTickPastLimitAndOvertimeEndsGame) {
    start();
    game->tick(kLimit - 1000);
    game->tick(kEnd + 50000);  // 一次跨過時限與延長賽
    EXPECT_EQ(view().status, GameStatus::Draw);
    EXPECT_TRUE(view().overtime);
    EXPECT_EQ(view().now, kEnd);
    EXPECT_EQ(gameOvers.size(), 1u);
}

TEST_F(OvertimeTest, W10_OvertimeHappensOnlyOnce) {
    start();
    game->tick(kEnd);
    EXPECT_EQ(view().status, GameStatus::Draw);  // 延長賽後仍同分不會再延長
    game->tick(kEnd + 60000);
    EXPECT_EQ(gameOvers.size(), 1u);
}

TEST_F(OvertimeTest, W10_StateCarriesIntoOvertime) {
    start();
    placeBlack({{0, 0}, {1, 0}});
    ASSERT_TRUE(game->submit(white(5, 5), kLimit - 100).accepted);
    game->tick(kLimit);
    const PlayerView v = game->viewFor(PlayerId::White);
    EXPECT_EQ(v.board.at({0, 0}), Cell::Black);
    EXPECT_EQ(v.board.at({5, 5}), Cell::White);
    EXPECT_EQ(v.placeCooldownRemaining, config.placeCooldown - 100);  // 下子間隔延續
    EXPECT_EQ(game->submit(white(6, 6), kLimit).reason, RejectReason::PlaceCooldown);
}

TEST_F(OvertimeTest, W10_ScoreTargetModeHasNoOvertime) {
    config.mode = MatchMode::ScoreTarget;
    start();
    game->tick(kEnd * 10);
    EXPECT_EQ(view().status, GameStatus::Running);
    EXPECT_FALSE(view().overtime);
}

TEST_F(OvertimeTest, G4_RestartResetsOvertime) {
    start();
    game->tick(kEnd);
    ASSERT_TRUE(view().boardScores.has_value());
    game->restart();
    EXPECT_FALSE(view().overtime);
    EXPECT_FALSE(view().boardScores.has_value());
    game->confirmSkill(PlayerId::Black);
    game->confirmSkill(PlayerId::White);
    game->tick(kLimit - 1);
    EXPECT_EQ(view().status, GameStatus::Running);
    EXPECT_FALSE(view().overtime);
    EXPECT_EQ(view().timeRemaining, 1);
}

TEST_F(OvertimeTest, G4_RestartDuringOvertime) {
    start();
    game->tick(kLimit + 5000);
    ASSERT_TRUE(view().overtime);
    game->restart();
    game->confirmSkill(PlayerId::Black);
    game->confirmSkill(PlayerId::White);
    game->tick(0);
    EXPECT_FALSE(view().overtime);
    EXPECT_EQ(view().timeRemaining, kLimit);
}
