#include <gtest/gtest.h>

#include "core/board.h"
#include "core/config.h"
#include "core/player_state.h"
#include "core/skill_system.h"
#include "core/zone_map.h"

namespace {

constexpr int kEnough = 3;  // spec S2：技能消耗 3 格

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

TEST_F(SkillTest, S2_EnergyCostIsThree) {
    EXPECT_EQ(config.energyCost, 3);
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

// ---- 炸彈（T08）----

class BombTest : public SkillTest {
protected:
    // 黑方選炸彈、能量 3 格；棋盤上 (3,3) 是白子、(4,4) 是黑子
    void SetUp() override {
        bomber = playerWith(SkillId::Bomb);
        skills.initPlayer(bomber);
        board.set({3, 3}, Cell::White);
        board.set({4, 4}, Cell::Black);
    }

    static constexpr TimeMs kNow = 5000;
    PlayerState bomber;
};

TEST_F(BombTest, SB3_BombRemovesOpponentStone) {
    ASSERT_EQ(skills.check(bombAt({3, 3}), bomber, board), std::nullopt);
    skills.apply(bombAt({3, 3}), bomber, board, zones, kNow);
    EXPECT_TRUE(board.isEmpty({3, 3}));
    EXPECT_EQ(board.at({4, 4}), Cell::Black);  // 其他格子不受影響
}

TEST_F(BombTest, SB3_WhiteCanBombBlackStone) {
    PlayerState whiteBomber = playerWith(SkillId::Bomb);
    skills.initPlayer(whiteBomber);
    const SkillAction action{PlayerId::White, SkillId::Bomb, Pos{4, 4}};
    ASSERT_EQ(skills.check(action, whiteBomber, board), std::nullopt);
    skills.apply(action, whiteBomber, board, zones, kNow);
    EXPECT_TRUE(board.isEmpty({4, 4}));
}

TEST_F(BombTest, S3_BombNeedsThreeEnergy) {
    bomber.energy = 2;
    EXPECT_EQ(skills.check(bombAt({3, 3}), bomber, board), RejectReason::NoEnergy);
}

TEST_F(BombTest, SB2_EmptyTargetIsInvalid) {
    EXPECT_EQ(skills.check(bombAt({0, 0}), bomber, board), RejectReason::InvalidTarget);
}

TEST_F(BombTest, SB2_OwnStoneIsInvalid) {
    EXPECT_EQ(skills.check(bombAt({4, 4}), bomber, board), RejectReason::InvalidTarget);
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

TEST_F(BombTest, SB4_EnergyBeforeInvalidTarget) {
    bomber.energy = 2;
    EXPECT_EQ(skills.check(bombAt({0, 0}), bomber, board), RejectReason::NoEnergy);
    EXPECT_EQ(skills.check(bombAt({4, 4}), bomber, board), RejectReason::NoEnergy);
}
