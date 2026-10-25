#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "app/game_controller.h"
#include "app/local_session.h"
#include "core/ai_engine.h"

namespace {

constexpr TimeMs kTick = 50;
constexpr TimeMs kMatchLimit = 30 * 60 * 1000;  // spec §11：每場模擬時間上限 30 分鐘

int countStones(const Board& board, Cell cell) {
    int count = 0;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            count += board.at({x, y}) == cell ? 1 : 0;
        }
    }
    return count;
}

bool finished(GameStatus s) {
    return s == GameStatus::BlackWon || s == GameStatus::WhiteWon || s == GameStatus::Draw;
}

}  // namespace

class AIControllerTest : public ::testing::Test {
protected:
    // 技能消耗設為 0：AI 不必保留能量（A9a），開局就會下子，方便測接線與出手節奏
    AIControllerTest() {
        config.skill.bombCost = 0;
        config.skill.dominateCost = 0;
        config.skill.destroyCost = 0;
    }

    std::unique_ptr<AIEngine> whiteAI(std::uint32_t seed = 3) {
        return std::make_unique<AIEngine>(PlayerId::White, config.ai, config.skill, seed);
    }

    void humanConfirms(GameController& game) {
        game.selectSkill(PlayerId::Black, SkillId::Dominate);
        game.confirmSkill(PlayerId::Black);
    }

    MatchConfig config;
};

TEST_F(AIControllerTest, A2a_AttachedAIChoosesAndConfirmsSkill) {
    GameController game{config};
    std::vector<PlayerId> confirmed;
    QObject::connect(&game, &GameController::skillConfirmed, [&](PlayerId p) { confirmed.push_back(p); });
    game.attachAI(PlayerId::White, whiteAI());
    ASSERT_EQ(confirmed.size(), 1u);
    EXPECT_EQ(confirmed[0], PlayerId::White);
    EXPECT_EQ(game.viewFor(PlayerId::White).status, GameStatus::SkillSelect);  // 等人類確定
    humanConfirms(game);
    EXPECT_EQ(game.viewFor(PlayerId::White).status, GameStatus::Countdown);
}

TEST_F(AIControllerTest, M1_AIActsOnTickThroughSubmit) {
    GameController game{config};
    game.attachAI(PlayerId::White, whiteAI());
    humanConfirms(game);
    game.tick(-1000);
    EXPECT_EQ(countStones(game.viewFor(PlayerId::Black).board, Cell::White), 0);  // 倒數中不行動（G3）
    game.tick(0);
    EXPECT_EQ(game.viewFor(PlayerId::Black).board.at({7, 7}), Cell::White);  // A12
}

TEST_F(AIControllerTest, A1_AIRespectsEnergyAndPlaceInterval) {
    config.startEnergy = 1;
    GameController game{config};  // T = 2000：開局 1 格，之後每 2 秒 1 格
    game.attachAI(PlayerId::White, whiteAI());
    humanConfirms(game);
    for (TimeMs t = 0; t <= 5000; t += kTick) {
        game.tick(t);
    }
    // 0、2000、4000 ms 各最多一子
    EXPECT_LE(countStones(game.viewFor(PlayerId::Black).board, Cell::White), 3);
}

TEST_F(AIControllerTest, M1_A2_AIPaceFollowsConfigReactionTime) {
    // 能量與下子間隔都不是限制（每 500 ms 回 1 格），出手速度只由 AIConfig::reactionTime 決定
    config.regenInterval = 500;
    config.ai.reactionTime = 1000;
    GameController game{config};
    game.attachAI(PlayerId::White, whiteAI());
    humanConfirms(game);
    game.tick(0);
    EXPECT_EQ(countStones(game.viewFor(PlayerId::Black).board, Cell::White), 1);
    game.tick(500);
    EXPECT_EQ(countStones(game.viewFor(PlayerId::Black).board, Cell::White), 1);  // 預設 350 ms 會在這裡出手
    game.tick(1000);
    EXPECT_EQ(countStones(game.viewFor(PlayerId::Black).board, Cell::White), 2);
}

// M1a 只影響 UI：開關開啟與否，AI 拿到的都是 viewFor(AI 自己)，所以同一個 seed 下的行為完全相同
TEST_F(AIControllerTest, M1a_A3_ShowAiInfoDoesNotChangeWhatAISees) {
    auto play = [this](bool showAiInfo) {
        MatchConfig match = config;
        match.showAiInfo = showAiInfo;
        match.regenInterval = 500;
        GameController game{match};
        game.attachAI(PlayerId::White, std::make_unique<AIEngine>(PlayerId::White, match.ai, match.skill, 11));
        game.selectSkill(PlayerId::Black, SkillId::Bomb);
        game.confirmSkill(PlayerId::Black);
        std::vector<Board> boards;
        for (TimeMs t = 0; t <= 20000; t += kTick) {
            if (t % 1000 == 0) {
                game.submit(PlaceAction{PlayerId::Black, Pos{static_cast<int>(t / 1000) % Board::kSize, 3}}, t);
            }
            game.tick(t);
            boards.push_back(game.viewFor(PlayerId::Black).board);
        }
        return boards;
    };
    const std::vector<Board> hidden = play(false);
    const std::vector<Board> shown = play(true);
    ASSERT_EQ(hidden.size(), shown.size());
    for (std::size_t i = 0; i < hidden.size(); ++i) {
        for (int y = 0; y < Board::kSize; ++y) {
            for (int x = 0; x < Board::kSize; ++x) {
                ASSERT_EQ(hidden[i].at({x, y}), shown[i].at({x, y})) << "tick " << i;
            }
        }
    }
}

// design §4.8：兩方都是 AI 時，同一次 tick 裡的決策先後每次輪流；開局第一次 tick 由黑方先。
// 回能與下子間隔都是 500 ms，所以每次 tick 雙方都會各下一子
TEST_F(AIControllerTest, P5_TwoAIsAlternateFirstMoveEachTick) {
    config.regenInterval = 500;
    GameController game{config};
    std::vector<PlayerId> order;
    int blackStones = 0;
    int whiteStones = 0;
    QObject::connect(&game, &GameController::stateChanged, [&] {
        const Board board = game.viewFor(PlayerId::Black).board;
        const int black = countStones(board, Cell::Black);
        const int white = countStones(board, Cell::White);
        if (black > blackStones) {
            order.push_back(PlayerId::Black);
        }
        if (white > whiteStones) {
            order.push_back(PlayerId::White);
        }
        blackStones = black;
        whiteStones = white;
    });
    game.attachAI(PlayerId::Black, std::make_unique<AIEngine>(PlayerId::Black, config.ai, config.skill, 1));
    game.attachAI(PlayerId::White, std::make_unique<AIEngine>(PlayerId::White, config.ai, config.skill, 2));
    for (TimeMs t : {0, 500, 1000}) {
        game.tick(t);
    }
    const std::vector<PlayerId> expected{PlayerId::Black, PlayerId::White, PlayerId::White,
                                         PlayerId::Black, PlayerId::Black, PlayerId::White};
    EXPECT_EQ(order, expected);

    game.restart();  // 兩個 AI 自動選好技能，直接進入倒數
    order.clear();
    blackStones = 0;
    whiteStones = 0;
    game.tick(0);
    EXPECT_EQ(order, (std::vector<PlayerId>{PlayerId::Black, PlayerId::White}));  // 新的一局又從黑方先
}

TEST_F(AIControllerTest, G4_AIChoosesAgainAfterRestart) {
    GameController game{config};
    game.attachAI(PlayerId::White, whiteAI());
    humanConfirms(game);
    TimeMs t = 0;
    while (!finished(game.viewFor(PlayerId::Black).status) && t < kMatchLimit) {
        game.tick(t);  // 黑方不下子，白方 AI 自己連五
        t += kTick;
    }
    ASSERT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::WhiteWon);

    std::vector<PlayerId> confirmed;
    QObject::connect(&game, &GameController::skillConfirmed, [&](PlayerId p) { confirmed.push_back(p); });
    game.restart();
    ASSERT_EQ(confirmed.size(), 1u);
    EXPECT_EQ(confirmed[0], PlayerId::White);

    humanConfirms(game);
    game.tick(0);
    EXPECT_EQ(game.viewFor(PlayerId::Black).board.at({7, 7}), Cell::White);  // 新的一局從 0 重新計時
}

TEST_F(AIControllerTest, U7_LocalSessionReportsAIReady) {
    LocalSession session(config, PlayerId::Black);
    int ready = 0;
    QObject::connect(&session, &GameSession::opponentReady, [&] { ++ready; });
    session.attachAI(PlayerId::White, 7);
    EXPECT_EQ(ready, 1);
}

// spec §11：AI 對 AI，指定比賽模式跑 50 場（不同 seed）。每場都要在 30 分鐘內結束，
// 除了被拒絕之外不能有其他錯誤（例外）
namespace {

void runSmokeGames(MatchMode mode) {
    constexpr std::uint32_t kGames = 50;
    for (std::uint32_t game = 0; game < kGames; ++game) {
        MatchConfig match;
        match.mode = mode;

        GameController controller{match};
        controller.attachAI(PlayerId::Black, std::make_unique<AIEngine>(PlayerId::Black, match.ai, match.skill, game * 2 + 1));
        controller.attachAI(PlayerId::White, std::make_unique<AIEngine>(PlayerId::White, match.ai, match.skill, game * 2 + 2));
        ASSERT_EQ(controller.viewFor(PlayerId::Black).status, GameStatus::Countdown) << "game " << game;

        TimeMs t = -match.countdown;
        while (!finished(controller.viewFor(PlayerId::Black).status) && t <= kMatchLimit) {
            ASSERT_NO_THROW(controller.tick(t)) << "game " << game << " t=" << t;
            t += kTick;
        }
        const PlayerView view = controller.viewFor(PlayerId::Black);
        ASSERT_TRUE(finished(view.status)) << "game " << game << " 沒有在 30 分鐘內結束";
        if (mode == MatchMode::TimeLimit) {
            EXPECT_LE(t - kTick, match.timeLimit + match.overtime) << "game " << game;  // W7、W10：時間到就結束
        } else {
            const bool reachedTarget = view.scores[0] >= match.targetScore || view.scores[1] >= match.targetScore;
            const bool boardFull = countStones(view.board, Cell::Empty) == 0;
            EXPECT_TRUE(reachedTarget || boardFull) << "game " << game;  // W8、W4
        }
    }
}

}  // namespace

TEST_F(AIControllerTest, A1_AIVersusAISmokeTestTimeLimit50Games) {
    runSmokeGames(MatchMode::TimeLimit);
}

TEST_F(AIControllerTest, A1_AIVersusAISmokeTestTargetScore50Games) {
    runSmokeGames(MatchMode::ScoreTarget);
}
