#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "app/game_controller.h"

class CountdownRestartTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.regenInterval = 500;
        config.mode = MatchMode::ScoreTarget;  // 第一條連五就結束，方便測試結束後的流程
        config.targetScore = 5;
    }

    void confirmBoth(GameController& game, SkillId blackSkill = SkillId::Dominate,
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
    EXPECT_TRUE(view.lastClearedLines.empty());
    EXPECT_EQ(view.scores, (std::array<int, 2>{0, 0}));
    EXPECT_EQ(view.opponentSkillRevealed, std::nullopt);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(view.board.isEmpty({i, 0}));
    }
}

TEST_F(CountdownRestartTest, G4_RestartKeepsPreviousSkillAsDefault) {
    GameController game{config};
    playBlackWin(game);
    game.restart();
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.skill, SkillId::Dominate);
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
    EXPECT_TRUE(game.submit(PlaceAction{PlayerId::Black, {0, 0}}, 0).accepted);  // 舊棋子已清除
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
    confirmBoth(game, SkillId::Bomb, SkillId::Dominate);
    const PlayerView black = game.viewFor(PlayerId::Black);
    EXPECT_EQ(black.self.skill, SkillId::Bomb);
    EXPECT_EQ(black.self.energy, 1);
    EXPECT_EQ(game.viewFor(PlayerId::White).self.skill, SkillId::Dominate);
}

// ---- T11a：中途重開與中止（G4a、G4b、W5）----

TEST_F(CountdownRestartTest, G4a_RestartDuringCountdown) {
    GameController game{config};
    confirmBoth(game);
    game.tick(-1500);
    game.restart();
    const PlayerView view = game.viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::SkillSelect);
    EXPECT_EQ(view.self.skill, SkillId::Dominate);  // 預設上一局的技能
    game.confirmSkill(PlayerId::Black);
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::SkillSelect);  // 要重新確定
}

TEST_F(CountdownRestartTest, G4a_RestartDuringRunningResetsEverything) {
    config.mode = MatchMode::TimeLimit;
    config.startEnergy = config.maxEnergy;  // 開局就夠用技能
    GameController game{config};
    confirmBoth(game, SkillId::Destroy, SkillId::Dominate);
    game.tick(0);
    ASSERT_TRUE(game.submit(SkillAction{PlayerId::White, SkillId::Dominate, std::nullopt}, 0).accepted);
    ASSERT_TRUE(game.submit(PlaceAction{PlayerId::White, {0, 14}}, 0).accepted);
    ASSERT_TRUE(game.submit(SkillAction{PlayerId::Black, SkillId::Destroy, Pos{7, 7}}, 0).accepted);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(game.submit(PlaceAction{PlayerId::Black, {i, 0}}, i * 1000).accepted);
    }
    ASSERT_EQ(game.viewFor(PlayerId::Black).scores[0], 5);
    ASSERT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Running);

    game.restart();
    const PlayerView view = game.viewFor(PlayerId::Black);
    EXPECT_EQ(view.status, GameStatus::SkillSelect);
    EXPECT_EQ(view.scores, (std::array<int, 2>{0, 0}));
    EXPECT_TRUE(view.zones.empty());
    EXPECT_TRUE(view.lastClearedLines.empty());
    EXPECT_EQ(view.board.at({7, 7}), Cell::Empty);  // 已摧毀的格子也清掉
    EXPECT_EQ(view.board.at({0, 14}), Cell::Empty);

    game.confirmSkill(PlayerId::Black);
    game.confirmSkill(PlayerId::White);
    game.tick(0);
    EXPECT_FALSE(game.viewFor(PlayerId::Black).self.destroyUsed);
    EXPECT_EQ(game.viewFor(PlayerId::White).self.dominateCharges, 0);
}

TEST_F(CountdownRestartTest, G4a_RestartDuringSkillSelectKeepsSelection) {
    GameController game{config};
    game.selectSkill(PlayerId::Black, SkillId::Bomb);
    game.confirmSkill(PlayerId::Black);
    game.restart();
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::SkillSelect);
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
    game.selectSkill(PlayerId::Black, SkillId::Destroy);  // 確定被清掉，可以重選
    EXPECT_EQ(game.viewFor(PlayerId::Black).self.skill, SkillId::Destroy);
}

TEST_F(CountdownRestartTest, W5_AbortEndsWithoutResult) {
    GameController game{config};
    std::vector<GameStatus> overs;
    QObject::connect(&game, &GameController::gameOver,
                     [&](GameStatus s, std::array<int, 2>) { overs.push_back(s); });
    confirmBoth(game);
    game.tick(0);
    game.abort();
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Aborted);
    ASSERT_EQ(overs.size(), 1u);
    EXPECT_EQ(overs[0], GameStatus::Aborted);
    EXPECT_EQ(game.submit(PlaceAction{PlayerId::Black, {7, 7}}, 100).reason, RejectReason::GameNotRunning);
    game.abort();  // 已結束時再中止不會重複發出
    EXPECT_EQ(overs.size(), 1u);
}

TEST_F(CountdownRestartTest, G4b_AbortDuringSkillSelect) {
    GameController game{config};
    game.abort();
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Aborted);
    game.restart();
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::SkillSelect);
}
