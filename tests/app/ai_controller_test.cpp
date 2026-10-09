#include <gtest/gtest.h>

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
    AIEngine whiteAI(std::uint32_t seed = 3) { return AIEngine::fromConfig(PlayerId::White, config, seed); }

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
    GameController game{config};  // T = 2000：開局 1 格，之後每 2 秒 1 格
    game.attachAI(PlayerId::White, whiteAI());
    humanConfirms(game);
    for (TimeMs t = 0; t <= 5000; t += kTick) {
        game.tick(t);
    }
    // 0、2000、4000 ms 各最多一子
    EXPECT_LE(countStones(game.viewFor(PlayerId::Black).board, Cell::White), 3);
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

// spec §11：AI 對 AI 連續 100 場，每場都要結束，除了被拒絕之外沒有其他錯誤
TEST_F(AIControllerTest, A1_AIVersusAISmokeTest100Games) {
    int draws = 0;
    for (std::uint32_t game = 0; game < 100; ++game) {
        MatchConfig match;
        const MatchConfig& other = match;

        GameController controller{match};
        controller.attachAI(PlayerId::Black, AIEngine::fromConfig(PlayerId::Black, match, game * 2 + 1));
        controller.attachAI(PlayerId::White, AIEngine::fromConfig(PlayerId::White, other, game * 2 + 2));
        ASSERT_EQ(controller.viewFor(PlayerId::Black).status, GameStatus::Countdown) << "game " << game;

        TimeMs t = -match.countdown;
        while (!finished(controller.viewFor(PlayerId::Black).status) && t <= kMatchLimit) {
            controller.tick(t);
            t += kTick;
        }
        const GameStatus result = controller.viewFor(PlayerId::Black).status;
        ASSERT_TRUE(finished(result)) << "game " << game << " 沒有在 30 分鐘內結束";
        draws += result == GameStatus::Draw ? 1 : 0;
    }
    RecordProperty("draws", draws);
}
