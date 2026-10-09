#include <gtest/gtest.h>

#include <vector>

#include "app/game_controller.h"

class CountdownRestartTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.regenInterval = 500;
    }

    void confirmBoth(GameController& game, SkillId blackSkill = SkillId::Accelerate,
                     SkillId whiteSkill = SkillId::Bomb) {
        game.selectSkill(PlayerId::Black, blackSkill);
        game.selectSkill(PlayerId::White, whiteSkill);
        game.confirmSkill(PlayerId::Black);
        game.confirmSkill(PlayerId::White);
    }

    // 黑方在第 0 列連五獲勝
    void playBlackWin(GameController& game) {
        confirmBoth(game);
        game.tick(0);
        for (int i = 0; i < 5; ++i) {
            ASSERT_TRUE(game.submit(PlaceAction{PlayerId::Black, {i, 0}}, i * 1000).accepted);
        }
        ASSERT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::BlackWon);
    }

    MatchConfig config;
};

// ---- G2：3 秒倒數 ----

TEST_F(CountdownRestartTest, G2_CountdownStartsAtMinusThreeSeconds) {
    GameController game{config};
    confirmBoth(game);
    const PlayerView view = game.viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::Countdown);
    EXPECT_EQ(view.now, -3000);
    EXPECT_EQ(view.countdownRemaining, 3000);
}

TEST_F(CountdownRestartTest, G2_CountdownRemainingFollowsTicks) {
    GameController game{config};
    confirmBoth(game);
    game.tick(-1200);
    EXPECT_EQ(game.viewFor(PlayerId::White).countdownRemaining, 1200);
    EXPECT_EQ(game.viewFor(PlayerId::White).status, GameStatus::Countdown);
    game.tick(-1);
    EXPECT_EQ(game.viewFor(PlayerId::White).status, GameStatus::Countdown);
}

TEST_F(CountdownRestartTest, G2_RunningWhenCountdownReachesZero) {
    GameController game{config};
    confirmBoth(game);
    game.tick(0);
    const PlayerView view = game.viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::Running);
    EXPECT_EQ(view.countdownRemaining, 0);
    EXPECT_EQ(view.self.energy, 1);
}

TEST_F(CountdownRestartTest, G2_NoEnergyRegenDuringCountdown) {
    GameController game{config};
    confirmBoth(game);
    game.tick(-2000);
    game.tick(0);
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.energy, 1);
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.regenProgress, 0);
}

TEST_F(CountdownRestartTest, G2_LateFirstTickCatchesUpFromZero) {
    GameController game{config};
    confirmBoth(game);
    game.tick(1250);  // 倒數期間沒有 tick，直接跳到 1250
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Running);
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.energy, 3);  // 從 0 起算：1 + 2 格（T = 500）
}

TEST_F(CountdownRestartTest, G2_CountdownLengthComesFromConfig) {
    config.countdown = 5000;
    GameController game{config};
    confirmBoth(game);
    EXPECT_EQ(game.viewFor(PlayerId::Black).countdownRemaining, 5000);
}

// ---- G4：再來一局 ----

TEST_F(CountdownRestartTest, G4_RestartResetsEverythingToSkillSelect) {
    GameController game{config};
    playBlackWin(game);
    int changes = 0;
    QObject::connect(&game, &GameController::stateChanged, [&] { ++changes; });

    game.restart();
    EXPECT_EQ(changes, 1);
    const PlayerView view = game.viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::SkillSelect);
    EXPECT_TRUE(view.winningLine.empty());
    EXPECT_EQ(view.opponentSkillRevealed, std::nullopt);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(view.board.isEmpty({i, 0}));
    }
}

TEST_F(CountdownRestartTest, G4_RestartKeepsPreviousSkillAsDefault) {
    GameController game{config};
    playBlackWin(game);
    game.restart();
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.skill, SkillId::Accelerate);
    EXPECT_EQ(game.viewFor(PlayerId::White).self.skill, SkillId::Bomb);

    // 不改選直接確定也可以開始
    game.confirmSkill(PlayerId::Black);
    game.confirmSkill(PlayerId::White);
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Countdown);
}

TEST_F(CountdownRestartTest, G4_RestartRequiresConfirmAgainAndAllowsChange) {
    GameController game{config};
    playBlackWin(game);
    game.restart();
    game.selectSkill(PlayerId::Black, SkillId::Bomb);  // 可以更換
    game.confirmSkill(PlayerId::Black);
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::SkillSelect);  // 白方還沒確定
    game.confirmSkill(PlayerId::White);
    game.tick(0);
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
}

TEST_F(CountdownRestartTest, G4_S4_NewMatchStartsFresh) {
    GameController game{config};
    playBlackWin(game);
    game.restart();
    confirmBoth(game);
    game.tick(0);
    const PlayerView view = game.viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::Running);
    EXPECT_EQ(view.now, 0);
    EXPECT_EQ(view.self.energy, 1);
    EXPECT_EQ(view.self.lastPlaceTime, std::nullopt);
    EXPECT_EQ(view.self.skillReadyAt, 25000);  // S4：重新等一次完整冷卻
    EXPECT_TRUE(game.submit(PlaceAction{PlayerId::Black, {0, 0}}, 0).accepted);  // 舊棋子已清除
}

TEST_F(CountdownRestartTest, G4_RestartIgnoredWhileRunning) {
    GameController game{config};
    confirmBoth(game);
    game.tick(0);
    ASSERT_TRUE(game.submit(PlaceAction{PlayerId::Black, {7, 7}}, 0).accepted);
    game.restart();
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Running);
    EXPECT_EQ(game.viewFor(PlayerId::Black).board.at({7, 7}), Cell::Black);
}

TEST_F(CountdownRestartTest, U7_SkillConfirmedSignalTellsWho) {
    GameController game{config};
    std::vector<PlayerId> confirmedPlayers;
    QObject::connect(&game, &GameController::skillConfirmed, [&](PlayerId p) { confirmedPlayers.push_back(p); });
    game.selectSkill(PlayerId::White, SkillId::Bomb);
    game.confirmSkill(PlayerId::Black);  // 沒選，忽略
    game.confirmSkill(PlayerId::White);
    ASSERT_EQ(confirmedPlayers.size(), 1u);
    EXPECT_EQ(confirmedPlayers[0], PlayerId::White);
}

TEST_F(CountdownRestartTest, G2_U2_CountdownViewShowsChosenSkillAndStartEnergy) {
    GameController game{config};
    confirmBoth(game, SkillId::Bomb, SkillId::Accelerate);
    const PlayerView black = game.viewFor(PlayerId::Black);
    EXPECT_EQ(black.self.skill, SkillId::Bomb);
    EXPECT_EQ(black.self.energy, 1);
    EXPECT_EQ(black.self.skillReadyAt, 20000);  // S4：從對局時間 0 起算的冷卻
    EXPECT_EQ(game.viewFor(PlayerId::White).self.skill, SkillId::Accelerate);
}
