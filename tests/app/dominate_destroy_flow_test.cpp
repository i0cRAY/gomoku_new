#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "app/game_controller.h"

namespace {

PlaceAction black(int x, int y) {
    return PlaceAction{PlayerId::Black, Pos{x, y}};
}

PlaceAction white(int x, int y) {
    return PlaceAction{PlayerId::White, Pos{x, y}};
}

SkillAction dominate(PlayerId p) {
    return SkillAction{p, SkillId::Dominate, std::nullopt};
}

SkillAction destroyAt(PlayerId p, Pos target) {
    return SkillAction{p, SkillId::Destroy, target};
}

bool hasZone(const PlayerView& view, Pos p, PlayerId owner) {
    return std::any_of(view.zones.begin(), view.zones.end(),
                       [&](const ZoneCell& z) { return z.pos == p && z.owner == owner; });
}

}  // namespace

class SkillFlowNewTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.regenInterval = 500;
    }

    void start(SkillId blackSkill, SkillId whiteSkill) {
        game = std::make_unique<GameController>(config);
        game->selectSkill(PlayerId::Black, blackSkill);
        game->selectSkill(PlayerId::White, whiteSkill);
        game->confirmSkill(PlayerId::Black);
        game->confirmSkill(PlayerId::White);
        game->tick(0);
    }

    // 開局能量 1、T = 500：這兩個時間點能量都已回滿，夠用技能（S2）
    static constexpr TimeMs kEnoughEnergyAt = 20000;
    static constexpr TimeMs kLaterEnoughEnergyAt = 30000;
    MatchConfig config;
    std::unique_ptr<GameController> game;
};

// ---- 霸道 ----

TEST_F(SkillFlowNewTest, SZ2_PlacementAfterDominateCreatesZoneForOpponent) {
    start(SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(game->submit(dominate(PlayerId::Black), kEnoughEnergyAt).accepted);
    EXPECT_EQ(game->viewFor(PlayerId::Black).self.dominateCharges, 3);
    ASSERT_TRUE(game->submit(black(7, 7), kEnoughEnergyAt).accepted);
    EXPECT_EQ(game->viewFor(PlayerId::Black).self.dominateCharges, 2);

    EXPECT_EQ(game->submit(white(7, 6), kEnoughEnergyAt + 100).reason, RejectReason::RestrictedZone);
    EXPECT_EQ(game->submit(white(6, 7), kEnoughEnergyAt + 2999).reason, RejectReason::RestrictedZone);
    EXPECT_TRUE(game->submit(white(6, 7), kEnoughEnergyAt + 3000).accepted);  // 剛好到期可以下
}

TEST_F(SkillFlowNewTest, SZ3_ZoneDoesNotRestrictOwner) {
    start(SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(game->submit(dominate(PlayerId::Black), kEnoughEnergyAt).accepted);
    ASSERT_TRUE(game->submit(black(7, 7), kEnoughEnergyAt).accepted);
    EXPECT_TRUE(game->submit(black(7, 8), kEnoughEnergyAt + 1000).accepted);
}

TEST_F(SkillFlowNewTest, SZ2_ScoringStoneStillCreatesZone) {
    start(SkillId::Dominate, SkillId::Bomb);
    TimeMs t = 0;
    for (int x = 0; x < 4; ++x, t += 1000) {
        ASSERT_TRUE(game->submit(black(x, 5), t).accepted);
    }
    ASSERT_TRUE(game->submit(dominate(PlayerId::Black), kEnoughEnergyAt).accepted);
    ASSERT_TRUE(game->submit(black(4, 5), kEnoughEnergyAt).accepted);  // 連五被消除
    ASSERT_EQ(game->viewFor(PlayerId::Black).scores[0], 5);
    EXPECT_TRUE(game->viewFor(PlayerId::Black).board.isEmpty({4, 5}));
    EXPECT_EQ(game->submit(white(4, 6), kEnoughEnergyAt + 100).reason, RejectReason::RestrictedZone);
}

TEST_F(SkillFlowNewTest, SZ4_E5_ZonesPublicChargesPrivate) {
    start(SkillId::Dominate, SkillId::Dominate);
    ASSERT_TRUE(game->submit(dominate(PlayerId::Black), kEnoughEnergyAt).accepted);
    ASSERT_TRUE(game->submit(black(7, 7), kEnoughEnergyAt).accepted);
    const PlayerView whiteView = game->viewFor(PlayerId::White);
    EXPECT_TRUE(hasZone(whiteView, {7, 8}, PlayerId::Black));
    EXPECT_TRUE(hasZone(game->viewFor(PlayerId::Black), {7, 8}, PlayerId::Black));
    EXPECT_EQ(whiteView.self.dominateCharges, 0);  // 只有自己的次數
    EXPECT_EQ(whiteView.opponentSkillRevealed, std::optional<SkillId>(SkillId::Dominate));  // 只有名稱
}

TEST_F(SkillFlowNewTest, SZ3_ZonesDisappearFromViewAfterExpiry) {
    start(SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(game->submit(dominate(PlayerId::Black), kEnoughEnergyAt).accepted);
    ASSERT_TRUE(game->submit(black(7, 7), kEnoughEnergyAt).accepted);
    game->tick(kEnoughEnergyAt + 3000);
    EXPECT_TRUE(game->viewFor(PlayerId::White).zones.empty());
}

// ---- 摧毀 ----

TEST_F(SkillFlowNewTest, SX2_DestroyThroughSubmit) {
    start(SkillId::Destroy, SkillId::Bomb);
    ASSERT_TRUE(game->submit(white(7, 7), 0).accepted);
    ASSERT_TRUE(game->submit(destroyAt(PlayerId::Black, {7, 7}), kLaterEnoughEnergyAt).accepted);
    const PlayerView view = game->viewFor(PlayerId::White);
    EXPECT_EQ(view.board.at({7, 7}), Cell::Destroyed);
    EXPECT_EQ(view.board.at({5, 9}), Cell::Destroyed);
    EXPECT_EQ(view.board.at({4, 7}), Cell::Empty);  // 5×5 範圍外
    EXPECT_EQ(view.opponentSkillRevealed, std::optional<SkillId>(SkillId::Destroy));
    EXPECT_TRUE(game->viewFor(PlayerId::Black).self.destroyUsed);
    EXPECT_EQ(game->submit(white(5, 5), kLaterEnoughEnergyAt).reason, RejectReason::DestroyedCell);
}

TEST_F(SkillFlowNewTest, SX3_SecondDestroyRejected) {
    start(SkillId::Destroy, SkillId::Bomb);
    ASSERT_TRUE(game->submit(destroyAt(PlayerId::Black, {2, 2}), kLaterEnoughEnergyAt).accepted);
    EXPECT_EQ(game->submit(destroyAt(PlayerId::Black, {12, 12}), kLaterEnoughEnergyAt * 3).reason, RejectReason::SkillUsedUp);
}

TEST_F(SkillFlowNewTest, SX4_GameNotRunningFirst) {
    game = std::make_unique<GameController>(config);
    EXPECT_EQ(game->submit(destroyAt(PlayerId::Black, {-1, 0}), 0).reason, RejectReason::GameNotRunning);
}

TEST_F(SkillFlowNewTest, SX2_DestroyClearsZonesInArea) {
    start(SkillId::Dominate, SkillId::Destroy);
    ASSERT_TRUE(game->submit(dominate(PlayerId::Black), kEnoughEnergyAt).accepted);
    ASSERT_TRUE(game->submit(black(7, 7), kEnoughEnergyAt).accepted);
    ASSERT_TRUE(game->submit(destroyAt(PlayerId::White, {7, 7}), kEnoughEnergyAt + 100).accepted);
    EXPECT_TRUE(game->viewFor(PlayerId::White).zones.empty());
}

TEST_F(SkillFlowNewTest, W4_DestroyedCellsAreNotEmptyForFullBoard) {
    start(SkillId::Destroy, SkillId::Bomb);
    ASSERT_TRUE(game->submit(destroyAt(PlayerId::Black, {7, 7}), kLaterEnoughEnergyAt).accepted);
    EXPECT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::Running);
}
