#include <gtest/gtest.h>

#include <array>
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
                         [this](GameStatus, std::array<int, 2>) { ++gameOverCount; });
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
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);
    controller.selectSkill(PlayerId::Black, SkillId::Bomb);
    controller.selectSkill(PlayerId::White, SkillId::Dominate);
    controller.confirmSkill(PlayerId::Black);
    controller.confirmSkill(PlayerId::White);
    controller.tick(0);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
}

TEST_F(SkillFlowTest, S1_G1a_CannotChangeAfterConfirm) {
    controller.selectSkill(PlayerId::Black, SkillId::Bomb);
    controller.confirmSkill(PlayerId::Black);
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);  // 忽略
    controller.selectSkill(PlayerId::White, SkillId::Dominate);
    controller.confirmSkill(PlayerId::White);
    controller.tick(0);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
}

TEST_F(SkillFlowTest, G1a_ConfirmWithoutSelectionIsIgnored) {
    controller.confirmSkill(PlayerId::Black);
    controller.selectSkill(PlayerId::White, SkillId::Bomb);
    controller.confirmSkill(PlayerId::White);
    EXPECT_EQ(status(), GameStatus::SkillSelect);

    controller.selectSkill(PlayerId::Black, SkillId::Dominate);  // 還沒確定，所以還能選
    controller.confirmSkill(PlayerId::Black);
    EXPECT_EQ(status(), GameStatus::Countdown);
}

TEST_F(SkillFlowTest, G2_CountdownOnlyAfterBothConfirm) {
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);
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
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);
    EXPECT_EQ(controller.submit(PlaceAction{PlayerId::Black, {7, 7}}, 0).reason, RejectReason::GameNotRunning);
}

TEST_F(SkillFlowTest, G3_SZ5_SkillRejectedWhenNotRunning) {
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), 99999).reason,
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
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);
    controller.confirmSkill(PlayerId::Black);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::nullopt);

    controller.selectSkill(PlayerId::White, SkillId::Bomb);
    controller.confirmSkill(PlayerId::White);
    controller.tick(0);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::nullopt);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).opponentSkillRevealed, std::nullopt);
}

// 開局能量 1、T = 2000：時間 4000 時回到 3 格，剛好夠用一次霸道或摧毀（S2）
constexpr TimeMs kThreeEnergy = 4000;

TEST_F(SkillFlowTest, S1_OpponentSkillRevealedAfterFirstUse) {
    start(SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), kThreeEnergy).accepted);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::optional<SkillId>(SkillId::Dominate));
    EXPECT_EQ(controller.viewFor(PlayerId::Black).opponentSkillRevealed, std::nullopt);  // 白方還沒用
}

TEST_F(SkillFlowTest, S1_RejectedUseDoesNotReveal) {
    start(SkillId::Dominate, SkillId::Bomb);
    EXPECT_FALSE(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), 1000).accepted);
    EXPECT_EQ(controller.viewFor(PlayerId::White).opponentSkillRevealed, std::nullopt);
}

// ---- 透過 submit 使用技能 ----

TEST_F(SkillFlowTest, S3_RejectedWithoutThreeEnergy) {
    start(SkillId::Dominate, SkillId::Bomb);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), kThreeEnergy - 1).reason,
              RejectReason::NoEnergy);
    ASSERT_FALSE(rejections.empty());
    EXPECT_EQ(rejections.back(), RejectReason::NoEnergy);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, 2);  // P3：不扣能量
}

TEST_F(SkillFlowTest, S2_SkillCostsThreeEnergy) {
    start(SkillId::Dominate, SkillId::Bomb);
    controller.tick(kThreeEnergy + 1000);  // 能量 3，進度 1000
    const int changesBefore = stateChangedCount;
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), kThreeEnergy + 1000).accepted);
    EXPECT_EQ(stateChangedCount, changesBefore + 1);
    const PlayerView view = controller.viewFor(PlayerId::Black);
    EXPECT_EQ(view.self.energy, 0);
    EXPECT_DOUBLE_EQ(view.nextEnergyRatio, 0.5);  // E6：進度不變
    EXPECT_EQ(controller.viewFor(PlayerId::White).self.energy, 3);  // 白方不受影響
}

TEST_F(SkillFlowTest, S3_NoCooldownUsableAgainWithEnergy) {
    start(SkillId::Dominate, SkillId::Bomb);
    controller.tick(20000);  // 能量滿 10
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), 20000).accepted);
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), 20000).accepted);
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), 20000).accepted);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, 1);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), 20000).reason, RejectReason::NoEnergy);
}

TEST_F(SkillFlowTest, S1a_UnownedSkillRejected) {
    start(SkillId::Dominate, SkillId::Bomb);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::Black, SkillId::Bomb, Pos{7, 7}), 30000).reason,
              RejectReason::SkillNotOwned);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Dominate), 30000).reason,
              RejectReason::SkillNotOwned);
}

TEST_F(SkillFlowTest, S5_SkillKeepsPlaceInterval) {
    start(SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {0, 0}}, 8000).accepted);  // 能量 5 → 4
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::Black, SkillId::Dominate), 8000).accepted);  // 剛下完子也能用
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.lastPlaceTime, std::optional<TimeMs>(8000));
    EXPECT_EQ(controller.submit(PlaceAction{PlayerId::Black, {1, 0}}, 8499).reason, RejectReason::PlaceCooldown);
}

// 開局能量 1、T = 2000：時間 2000 時回到 2 格，剛好夠用一次炸彈（S2）
constexpr TimeMs kTwoEnergy = 2000;

TEST_F(SkillFlowTest, SB3_S2_BombClearsAreaAndCostsTwo) {
    start(SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {7, 7}}, 0).accepted);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::White, {8, 8}}, 0).accepted);  // 白方能量 1 → 0
    ASSERT_TRUE(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{7, 7}), 2 * kTwoEnergy).accepted);
    const PlayerView view = controller.viewFor(PlayerId::White);
    EXPECT_TRUE(view.board.isEmpty({7, 7}));  // 對手的子
    EXPECT_TRUE(view.board.isEmpty({8, 8}));  // 自己的子也被清掉
    EXPECT_EQ(view.self.energy, 0);           // 2 − 2
    EXPECT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {7, 7}}, 2 * kTwoEnergy).accepted);  // 可以立刻再下
}

TEST_F(SkillFlowTest, SB1_SB2_SB4_BombRejectionsThroughController) {
    start(SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::White, {3, 3}}, 0).accepted);  // 白方能量 0
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{15, 0}), 0).reason,
              RejectReason::OutOfBoard);
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{3, 3}), kTwoEnergy - 1).reason,
              RejectReason::NoEnergy);  // 能量 0
    EXPECT_EQ(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, std::nullopt), 2 * kTwoEnergy).reason,
              RejectReason::InvalidTarget);  // SB5
    EXPECT_EQ(controller.viewFor(PlayerId::White).board.at({3, 3}), Cell::White);
    EXPECT_EQ(controller.viewFor(PlayerId::White).self.energy, 2);  // 被拒絕不扣能量
    EXPECT_TRUE(controller.submit(useSkill(PlayerId::White, SkillId::Bomb, Pos{0, 0}), 2 * kTwoEnergy).accepted);  // 空地也可以（SB1）
}

TEST_F(SkillFlowTest, W3_BombNeverTriggersWin) {
    config.regenInterval = 500;
    GameController game{config};
    int gameOvers = 0;
    QObject::connect(&game, &GameController::gameOver, [&](GameStatus, std::array<int, 2>) { ++gameOvers; });
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
    ASSERT_TRUE(game.submit(SkillAction{PlayerId::Black, SkillId::Bomb, Pos{4, 0}}, 6000).accepted);
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Running);
    EXPECT_EQ(gameOvers, 0);
}
