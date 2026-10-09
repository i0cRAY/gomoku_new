#include <gtest/gtest.h>

#include <vector>

#include "app/game_controller.h"

namespace {

SkillAction useSkill(PlayerId player, SkillId skill, std::optional<Pos> target = std::nullopt) {
    return SkillAction{player, skill, target};
}

}  // namespace

class SkillFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        QObject::connect(&controller, &GameController::stateChanged, [this] { ++stateChangedCount; });
        QObject::connect(&controller, &GameController::actionRejected,
                         [this](PlayerId, RejectReason r, std::optional<Pos>) { rejections.push_back(r); });
        QObject::connect(&controller, &GameController::gameOver,
                         [this](GameStatus, std::vector<Pos>) { ++gameOverCount; });
    }

    void start(SkillId blackSkill, SkillId whiteSkill) {
        controller.selectSkill(PlayerId::Black, blackSkill);
        controller.selectSkill(PlayerId::White, whiteSkill);
        controller.confirmSkill(PlayerId::Black);
        controller.confirmSkill(PlayerId::White);
        controller.tick(0);
        ASSERT_EQ(status(), GameStatus::Running);
    }

    GameStatus status() const { return controller.viewFor(PlayerId::Black).status; }

    MatchConfig config;
    GameController controller{config};
    int stateChangedCount = 0;
    int gameOverCount = 0;
    std::vector<RejectReason> rejections;
};

// ---- G1a、S1：技能選擇階段 ----

TEST_F(SkillFlowTest, G1a_StartsInSkillSelect) {
    EXPECT_EQ(status(), GameStatus::SkillSelect);
}

TEST_F(SkillFlowTest, S1_CanChangeSelectionBeforeConfirm) {
    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);
    controller.selectSkill(PlayerId::Black, SkillId::Bomb);
    controller.selectSkill(PlayerId::White, SkillId::Accelerate);
    controller.confirmSkill(PlayerId::Black);
    controller.confirmSkill(PlayerId::White);
    controller.tick(0);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
}

TEST_F(SkillFlowTest, S1_G1a_CannotChangeAfterConfirm) {
    controller.selectSkill(PlayerId::Black, SkillId::Bomb);
    controller.confirmSkill(PlayerId::Black);
    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);  // 忽略
    controller.selectSkill(PlayerId::White, SkillId::Accelerate);
    controller.confirmSkill(PlayerId::White);
    controller.tick(0);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
}

TEST_F(SkillFlowTest, G1a_ConfirmWithoutSelectionIsIgnored) {
    controller.confirmSkill(PlayerId::Black);
    controller.selectSkill(PlayerId::White, SkillId::Bomb);
    controller.confirmSkill(PlayerId::White);
    EXPECT_EQ(status(), GameStatus::SkillSelect);

    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);  // 還沒確定，所以還能選
    controller.confirmSkill(PlayerId::Black);
    EXPECT_EQ(status(), GameStatus::Countdown);
}

TEST_F(SkillFlowTest, G2_CountdownOnlyAfterBothConfirm) {
    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);
    controller.selectSkill(PlayerId::White, SkillId::Bomb);
    controller.confirmSkill(PlayerId::Black);
    EXPECT_EQ(status(), GameStatus::SkillSelect);
    controller.tick(0);
    EXPECT_EQ(status(), GameStatus::SkillSelect);  // 只有一方確定，時間不會讓對局開始
    controller.confirmSkill(PlayerId::White);
    EXPECT_EQ(status(), GameStatus::Countdown);
}

TEST_F(SkillFlowTest, G1a_SelectionShownInOwnView) {
    controller.selectSkill(PlayerId::White, SkillId::Bomb);
    EXPECT_EQ(controller.viewFor(PlayerId::White).self.skill, SkillId::Bomb);
}

TEST_F(SkillFlowTest, G3_PlaceRejectedDuringSkillSelect) {
    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);
    EXPECT_EQ(controller.submit(PlaceAction{PlayerId::Black, {7, 7}}, 0).reason, RejectReason::GameNotRunning);
}

TEST_F(SkillFlowTest, G3_SA5_SkillRejectedWhenNotRunning) {
    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 99999).reason,
              RejectReason::GameNotRunning);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Bomb, Pos{-1, -1}), 99999).reason,
              RejectReason::GameNotRunning);  // SB4：排在 SKILL_NOT_OWNED、OUT_OF_BOARD 之前
}

TEST_F(SkillFlowTest, S1_BothPlayersMayPickSameSkill) {
    start(SkillId::Bomb, SkillId::Bomb);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
    EXPECT_EQ(controller.viewFor(PlayerId::White).self.skill, SkillId::Bomb);
}

// ---- S1、E5：看不到對手的選擇，直到對手第一次使用技能 ----

TEST_F(SkillFlowTest, S1_E5_OpponentSkillHiddenUntilUsed) {
    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);
    controller.confirmSkill(PlayerId::Black);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::nullopt);

    controller.selectSkill(PlayerId::White, SkillId::Bomb);
    controller.confirmSkill(PlayerId::White);
    controller.tick(0);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::nullopt);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).opponentSkillRevealed, std::nullopt);
}

TEST_F(SkillFlowTest, S1_OpponentSkillRevealedAfterFirstUse) {
    start(SkillId::Accelerate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 25000).accepted);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::optional<SkillId>(SkillId::Accelerate));
    EXPECT_EQ(controller.viewFor(PlayerId::Black).opponentSkillRevealed, std::nullopt);  // 白方還沒用
}

TEST_F(SkillFlowTest, S1_RejectedUseDoesNotReveal) {
    start(SkillId::Accelerate, SkillId::Bomb);
    EXPECT_FALSE(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 1000).accepted);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::nullopt);
}

// ---- 透過 submit 使用技能 ----

TEST_F(SkillFlowTest, S4_S3_SkillOnCooldownAtMatchStart) {
    start(SkillId::Accelerate, SkillId::Bomb);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 24999).reason,
              RejectReason::SkillCooldown);
    ASSERT_FALSE(rejections.empty());
    EXPECT_EQ(rejections.back(), RejectReason::SkillCooldown);
}

TEST_F(SkillFlowTest, S1a_UnownedSkillRejected) {
    start(SkillId::Accelerate, SkillId::Bomb);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Bomb, Pos{7, 7}), 30000).reason,
              RejectReason::SkillNotOwned);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Accelerate), 30000).reason,
              RejectReason::SkillNotOwned);
}

TEST_F(SkillFlowTest, SA1_SA2_AccelerateDoublesRegenInView) {
    start(SkillId::Accelerate, SkillId::Bomb);
    controller.tick(25000);  // 能量 1 + 12 → 上限 10
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {0, 0}}, 25000).accepted);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {1, 0}}, 26000).accepted);  // 能量 8，進度 1000
    const int changesBefore = stateChangedCount;
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 26000).accepted);
    EXPECT_EQ(stateChangedCount, changesBefore + 1);

    EXPECT_TRUE(controller.viewFor(PlayerId::Black).accelerating);
    controller.tick(27000);  // 加速中：進度 1000 + 2000 = 3000 → 能量 9，進度 1000
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, 9);
    EXPECT_DOUBLE_EQ(controller.viewFor(PlayerId::Black).nextEnergyRatio, 0.5);
    EXPECT_EQ(controller.viewFor(PlayerId::White).self.energy, 10);  // 白方不受影響

    controller.tick(31000);
    EXPECT_FALSE(controller.viewFor(PlayerId::Black).accelerating);  // SA1：5 秒後結束
}

TEST_F(SkillFlowTest, S2_S5_SkillKeepsEnergyAndPlaceCooldown) {
    start(SkillId::Accelerate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {0, 0}}, 25000).accepted);
    const int energyBefore = controller.viewFor(PlayerId::Black).self.energy;
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 25000).accepted);  // S5：剛下完子也能用
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, energyBefore);                         // S2
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.lastPlaceTime, std::optional<TimeMs>(25000));
    EXPECT_EQ(controller.submit(PlaceAction{PlayerId::Black, {1, 0}}, 25500).reason, RejectReason::PlaceCooldown);
}

TEST_F(SkillFlowTest, S6_CooldownRestartsAfterUse) {
    start(SkillId::Accelerate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 30000).accepted);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 54999).reason,
              RejectReason::SkillCooldown);
    EXPECT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Accelerate), 55000).accepted);
}

TEST_F(SkillFlowTest, SB3_BombRemovesOpponentStone) {
    start(SkillId::Accelerate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {7, 7}}, 0).accepted);
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{7, 7}), 20000).accepted);
    EXPECT_TRUE(controller.viewFor(PlayerId::Black).board.isEmpty({7, 7}));
    EXPECT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {7, 7}}, 20000).accepted);  // 可以立刻再下
}

TEST_F(SkillFlowTest, SB2_SB4_BombRejectionsThroughController) {
    start(SkillId::Accelerate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::White, {3, 3}}, 0).accepted);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{15, 0}), 0).reason,
              RejectReason::OutOfBoard);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{3, 3}), 0).reason,
              RejectReason::SkillCooldown);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{3, 3}), 20000).reason,
              RejectReason::InvalidTarget);  // 自己的子
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, std::nullopt), 20000).reason,
              RejectReason::InvalidTarget);  // SB5
    EXPECT_TRUE(controller.viewFor(PlayerId::White).board.at({3, 3}) == Cell::White);
}

TEST_F(SkillFlowTest, W3_BombNeverTriggersWin) {
    config.regenInterval = 500;
    GameController game{config};
    int gameOvers = 0;
    QObject::connect(&game, &GameController::gameOver, [&](GameStatus, std::vector<Pos>) { ++gameOvers; });
    game.selectSkill(PlayerId::Black, SkillId::Bomb);
    game.selectSkill(PlayerId::White, SkillId::Bomb);
    game.confirmSkill(PlayerId::Black);
    game.confirmSkill(PlayerId::White);
    game.tick(0);

    // 黑方 (0..3, 0) 四子、白方 (4, 0) 擋住；炸掉白子後黑方只剩四子，不會獲勝
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(game.submit(PlaceAction{PlayerId::Black, {i, 0}}, i * 1000).accepted);
    }
    ASSERT_TRUE(game.submit(PlaceAction{PlayerId::White, {4, 0}}, 0).accepted);
    ASSERT_TRUE(game.submit(SkillAction{PlayerId::Black, SkillId::Bomb, Pos{4, 0}}, 20000).accepted);
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Running);
    EXPECT_EQ(gameOvers, 0);
}
