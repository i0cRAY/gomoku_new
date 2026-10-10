#include <gtest/gtest.h>

#include "core/board.h"
#include "core/config.h"
#include "core/player_state.h"
#include "core/skill_system.h"
#include "core/zone_map.h"

namespace {

constexpr int kEnough = 3;  // spec S2：夠用任何一項技能

PlayerState playerWith(SkillId skill, int energy = kEnough) {
    PlayerState state;
    state.energy = energy;
    state.skill = skill;
    return state;
}

SkillAction dominate() {
    return SkillAction{PlayerId::Black, SkillId::Dominate, std::nullopt};
}

SkillAction bombAt(Pos target) {
    return SkillAction{PlayerId::Black, SkillId::Bomb, target};
}

}  // namespace

class SkillTest : public ::testing::Test {
protected:
    SkillConfig config;
    SkillSystem skills{config};
    Board board;
    ZoneMap zones;
};

TEST_F(SkillTest, S2_CostDependsOnSkill) {
    EXPECT_EQ(config.costOf(SkillId::Bomb), 2);
    EXPECT_EQ(config.costOf(SkillId::Dominate), 3);
    EXPECT_EQ(config.costOf(SkillId::Destroy), 3);
}

TEST_F(SkillTest, S2_CostOfFollowsConfig) {
    SkillConfig custom;
    custom.bombCost = 5;
    custom.dominateCost = 1;
    custom.destroyCost = 4;
    EXPECT_EQ(custom.costOf(SkillId::Bomb), 5);
    EXPECT_EQ(custom.costOf(SkillId::Dominate), 1);
    EXPECT_EQ(custom.costOf(SkillId::Destroy), 4);
}

TEST_F(SkillTest, S3_NoCooldownUsableAtMatchStartWithEnoughEnergy) {
    PlayerState state = playerWith(SkillId::Dominate);
    skills.initPlayer(state);
    EXPECT_EQ(skills.check(dominate(), state, board), std::nullopt);
}

TEST_F(SkillTest, S3_RejectedWithoutEnoughEnergy) {
    PlayerState state = playerWith(SkillId::Dominate, 2);
    skills.initPlayer(state);
    EXPECT_EQ(skills.check(dominate(), state, board), RejectReason::NoEnergy);
}

TEST_F(SkillTest, S3_UsableAgainRightAfterUse) {
    PlayerState state = playerWith(SkillId::Dominate, 6);
    skills.initPlayer(state);
    skills.apply(dominate(), state, board, zones, 1000);
    EXPECT_EQ(skills.check(dominate(), state, board), std::nullopt);  // 沒有冷卻
}

TEST_F(SkillTest, S2_ApplyDoesNotChargeEnergyItself) {
    PlayerState state = playerWith(SkillId::Dominate, 5);
    skills.apply(dominate(), state, board, zones, 1000);
    EXPECT_EQ(state.energy, 5);  // 扣能量由 GameController 透過 EnergyManager 處理（design §4.5）
}

TEST_F(SkillTest, S1a_UsingUnownedSkillIsRejected) {
    PlayerState bombOwner = playerWith(SkillId::Bomb);
    EXPECT_EQ(skills.check(dominate(), bombOwner, board), RejectReason::SkillNotOwned);
    PlayerState dominateOwner = playerWith(SkillId::Dominate);
    EXPECT_EQ(skills.check(bombAt({7, 7}), dominateOwner, board), RejectReason::SkillNotOwned);
}

TEST_F(SkillTest, S1a_NotOwnedCheckedBeforeEnergy) {
    PlayerState bombOwner = playerWith(SkillId::Bomb, 0);
    EXPECT_EQ(skills.check(dominate(), bombOwner, board), RejectReason::SkillNotOwned);
}

TEST_F(SkillTest, S5_UsingSkillKeepsPlaceInterval) {
    PlayerState state = playerWith(SkillId::Dominate);
    state.lastPlaceTime = 29800;
    skills.apply(dominate(), state, board, zones, 30000);
    EXPECT_EQ(state.lastPlaceTime, std::optional<TimeMs>(29800));
}

TEST_F(SkillTest, S4_InitClearsPerGameState) {
    PlayerState state = playerWith(SkillId::Destroy);
    state.dominateCharges = 2;
    state.destroyUsed = true;
    skills.initPlayer(state);
    EXPECT_EQ(state.dominateCharges, 0);
    EXPECT_FALSE(state.destroyUsed);
}

// ---- 炸彈（T08、T17b：以目標為左上角的 2×2）----

class BombTest : public SkillTest {
protected:
    // 黑方選炸彈、能量 3 格；(3,3)(4,3) 白子、(3,4)(4,4) 黑子，(2,3)、(3,5)、(5,3) 在 2×2 外
    void SetUp() override {
        bomber = playerWith(SkillId::Bomb);
        skills.initPlayer(bomber);
        board.set({3, 3}, Cell::White);
        board.set({4, 3}, Cell::White);
        board.set({3, 4}, Cell::Black);
        board.set({4, 4}, Cell::Black);
        board.set({2, 3}, Cell::White);
        board.set({3, 5}, Cell::Black);
        board.set({5, 3}, Cell::White);
    }

    static constexpr TimeMs kNow = 5000;
    PlayerState bomber;
};

TEST_F(BombTest, SB3_ClearsBothSidesInTwoByTwoFromTopLeft) {
    ASSERT_EQ(skills.check(bombAt({3, 3}), bomber, board), std::nullopt);
    skills.apply(bombAt({3, 3}), bomber, board, zones, kNow);
    for (Pos p : {Pos{3, 3}, Pos{4, 3}, Pos{3, 4}, Pos{4, 4}}) {
        EXPECT_TRUE(board.isEmpty(p)) << p.x << "," << p.y;
    }
    EXPECT_EQ(board.at({2, 3}), Cell::White);  // 左邊、下面、右邊都在範圍外
    EXPECT_EQ(board.at({3, 5}), Cell::Black);
    EXPECT_EQ(board.at({5, 3}), Cell::White);
}

TEST_F(BombTest, SB3_WhiteBombAlsoClearsOwnStones) {
    PlayerState whiteBomber = playerWith(SkillId::Bomb);
    skills.initPlayer(whiteBomber);
    const SkillAction action{PlayerId::White, SkillId::Bomb, Pos{3, 3}};
    ASSERT_EQ(skills.check(action, whiteBomber, board), std::nullopt);
    skills.apply(action, whiteBomber, board, zones, kNow);
    EXPECT_TRUE(board.isEmpty({3, 3}));  // 白方自己的子
    EXPECT_TRUE(board.isEmpty({4, 4}));
}

TEST_F(BombTest, SB3_DestroyedCellsAndZonesStay) {
    board.set({4, 3}, Cell::Destroyed);
    zones.add({4, 4}, PlayerId::White, kNow + 3000);
    skills.apply(bombAt({3, 3}), bomber, board, zones, kNow);
    EXPECT_EQ(board.at({4, 3}), Cell::Destroyed);
    EXPECT_TRUE(zones.isRestricted({4, 4}, PlayerId::Black, kNow));
}

TEST_F(BombTest, SB3_ClippedAtBoardEdge) {
    board.set({14, 14}, Cell::White);
    board.set({13, 14}, Cell::White);
    ASSERT_EQ(skills.check(bombAt({14, 14}), bomber, board), std::nullopt);
    skills.apply(bombAt({14, 14}), bomber, board, zones, kNow);
    EXPECT_TRUE(board.isEmpty({14, 14}));
    EXPECT_EQ(board.at({13, 14}), Cell::White);  // 目標是左上角，左邊不在範圍內
}

TEST_F(BombTest, SB1_AnyCellOnBoardIsValidTarget) {
    EXPECT_EQ(skills.check(bombAt({0, 0}), bomber, board), std::nullopt);  // 空格（範圍內沒有棋子）
    EXPECT_EQ(skills.check(bombAt({3, 4}), bomber, board), std::nullopt);  // 自己的子
    board.set({9, 9}, Cell::Destroyed);
    EXPECT_EQ(skills.check(bombAt({9, 9}), bomber, board), std::nullopt);  // 已摧毀的格子
}

TEST_F(BombTest, S3_BombNeedsTwoEnergy) {
    bomber.energy = 1;
    EXPECT_EQ(skills.check(bombAt({3, 3}), bomber, board), RejectReason::NoEnergy);
    bomber.energy = 2;
    EXPECT_EQ(skills.check(bombAt({3, 3}), bomber, board), std::nullopt);
}

TEST_F(BombTest, SB2_OutOfBoardTarget) {
    EXPECT_EQ(skills.check(bombAt({-1, 3}), bomber, board), RejectReason::OutOfBoard);
    EXPECT_EQ(skills.check(bombAt({3, 15}), bomber, board), RejectReason::OutOfBoard);
}

TEST_F(BombTest, SB5_MissingTargetIsInvalid) {
    const SkillAction noTarget{PlayerId::Black, SkillId::Bomb, std::nullopt};
    EXPECT_EQ(skills.check(noTarget, bomber, board), RejectReason::InvalidTarget);
}

TEST_F(BombTest, SB4_NotOwnedBeforeOutOfBoard) {
    PlayerState dominateOwner = playerWith(SkillId::Dominate);
    EXPECT_EQ(skills.check(bombAt({-1, -1}), dominateOwner, board), RejectReason::SkillNotOwned);
}

TEST_F(BombTest, SB4_OutOfBoardBeforeEnergy) {
    bomber.energy = 0;
    EXPECT_EQ(skills.check(bombAt({15, 15}), bomber, board), RejectReason::OutOfBoard);
}

TEST_F(BombTest, SB4_SB5_MissingTargetCheckedWhereOutOfBoardIs) {
    bomber.energy = 0;
    const SkillAction noTarget{PlayerId::Black, SkillId::Bomb, std::nullopt};
    EXPECT_EQ(skills.check(noTarget, bomber, board), RejectReason::InvalidTarget);  // 排在能量之前
}

TEST_F(BombTest, SB4_EnergyIsLastCheck) {
    bomber.energy = 1;
    EXPECT_EQ(skills.check(bombAt({0, 0}), bomber, board), RejectReason::NoEnergy);
}
