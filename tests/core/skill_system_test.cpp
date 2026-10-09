#include <gtest/gtest.h>

#include "core/board.h"
#include "core/config.h"
#include "core/player_state.h"
#include "core/skill_system.h"

namespace {

PlayerState playerWith(SkillId skill) {
    PlayerState state;
    state.energy = 1;
    state.skill = skill;
    return state;
}

SkillAction accelerate() {
    return SkillAction{PlayerId::Black, SkillId::Accelerate, std::nullopt};
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
};

TEST_F(SkillTest, S4_AccelerateOnCooldownAtMatchStart) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    EXPECT_EQ(state.skillReadyAt, 25000);
    EXPECT_EQ(skills.check(accelerate(), state, board, 0), RejectReason::SkillCooldown);
    EXPECT_EQ(skills.remainingCooldown(state, 0), 25000);
}

TEST_F(SkillTest, S4_BombOnCooldownAtMatchStart) {
    PlayerState state = playerWith(SkillId::Bomb);
    skills.initPlayer(state, 0);
    EXPECT_EQ(state.skillReadyAt, 20000);
    EXPECT_EQ(skills.remainingCooldown(state, 5000), 15000);
}

TEST_F(SkillTest, S4_InitClearsAccelerate) {
    PlayerState state = playerWith(SkillId::Accelerate);
    state.accelerateUntil = 9999;
    skills.initPlayer(state, 0);
    EXPECT_EQ(state.accelerateUntil, 0);
}

TEST_F(SkillTest, S3_RejectedWhileCoolingDown) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    EXPECT_EQ(skills.check(accelerate(), state, board, 24999), RejectReason::SkillCooldown);
}

TEST_F(SkillTest, S3_UsableExactlyWhenCooldownEnds) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    EXPECT_EQ(skills.check(accelerate(), state, board, 25000), std::nullopt);
    EXPECT_EQ(skills.remainingCooldown(state, 25000), 0);
    EXPECT_EQ(skills.remainingCooldown(state, 90000), 0);
}

TEST_F(SkillTest, S1a_UsingUnownedSkillIsRejected) {
    PlayerState bombOwner = playerWith(SkillId::Bomb);
    skills.initPlayer(bombOwner, 0);
    EXPECT_EQ(skills.check(accelerate(), bombOwner, board, 99999), RejectReason::SkillNotOwned);

    PlayerState accelerateOwner = playerWith(SkillId::Accelerate);
    skills.initPlayer(accelerateOwner, 0);
    EXPECT_EQ(skills.check(bombAt({7, 7}), accelerateOwner, board, 99999), RejectReason::SkillNotOwned);
}

TEST_F(SkillTest, SA5_NotOwnedCheckedBeforeCooldown) {
    PlayerState bombOwner = playerWith(SkillId::Bomb);
    skills.initPlayer(bombOwner, 0);
    EXPECT_EQ(skills.check(accelerate(), bombOwner, board, 0), RejectReason::SkillNotOwned);
}

TEST_F(SkillTest, SA1_AccelerateLastsFiveSeconds) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    skills.apply(accelerate(), state, board, 30000);
    EXPECT_EQ(state.accelerateUntil, 35000);
}

TEST_F(SkillTest, S6_CooldownRestartsFromUse) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    skills.apply(accelerate(), state, board, 30000);
    EXPECT_EQ(state.skillReadyAt, 55000);
    EXPECT_EQ(skills.remainingCooldown(state, 40000), 15000);
    EXPECT_EQ(skills.check(accelerate(), state, board, 54999), RejectReason::SkillCooldown);
    EXPECT_EQ(skills.check(accelerate(), state, board, 55000), std::nullopt);
}

TEST_F(SkillTest, S2_S5_UsingSkillKeepsEnergyAndPlaceCooldown) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    state.energy = 3;
    state.regenProgress = 700;
    state.lastPlaceTime = 29500;
    skills.apply(accelerate(), state, board, 30000);
    EXPECT_EQ(state.energy, 3);
    EXPECT_EQ(state.regenProgress, 700);
    EXPECT_EQ(state.lastPlaceTime, std::optional<TimeMs>(29500));
}

TEST_F(SkillTest, S5_PlaceCooldownDoesNotBlockSkill) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    state.lastPlaceTime = 25000;  // 剛下完子
    EXPECT_EQ(skills.check(accelerate(), state, board, 25000), std::nullopt);
}

TEST_F(SkillTest, SA4_UsableAtFullEnergy) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    state.energy = 10;
    EXPECT_EQ(skills.check(accelerate(), state, board, 25000), std::nullopt);
}

TEST_F(SkillTest, SA5_AccelerateIgnoresTarget) {
    PlayerState state = playerWith(SkillId::Accelerate);
    skills.initPlayer(state, 0);
    const SkillAction withTarget{PlayerId::Black, SkillId::Accelerate, Pos{-5, 99}};
    EXPECT_EQ(skills.check(withTarget, state, board, 25000), std::nullopt);
}

// ---- 炸彈（T08）----

class BombTest : public SkillTest {
protected:
    // 黑方選炸彈、冷卻已好；棋盤上 (3,3) 是白子、(4,4) 是黑子
    void SetUp() override {
        bomber = playerWith(SkillId::Bomb);
        skills.initPlayer(bomber, 0);
        board.set({3, 3}, Cell::White);
        board.set({4, 4}, Cell::Black);
    }

    static constexpr TimeMs kReady = 20000;
    PlayerState bomber;
};

TEST_F(BombTest, SB3_BombRemovesOpponentStone) {
    ASSERT_EQ(skills.check(bombAt({3, 3}), bomber, board, kReady), std::nullopt);
    skills.apply(bombAt({3, 3}), bomber, board, kReady);
    EXPECT_TRUE(board.isEmpty({3, 3}));
    EXPECT_EQ(board.at({4, 4}), Cell::Black);  // 其他格子不受影響
}

TEST_F(BombTest, SB3_WhiteCanBombBlackStone) {
    PlayerState whiteBomber = playerWith(SkillId::Bomb);
    skills.initPlayer(whiteBomber, 0);
    const SkillAction action{PlayerId::White, SkillId::Bomb, Pos{4, 4}};
    ASSERT_EQ(skills.check(action, whiteBomber, board, kReady), std::nullopt);
    skills.apply(action, whiteBomber, board, kReady);
    EXPECT_TRUE(board.isEmpty({4, 4}));
}

TEST_F(BombTest, S6_BombRestartsCooldown) {
    skills.apply(bombAt({3, 3}), bomber, board, 30000);
    EXPECT_EQ(bomber.skillReadyAt, 50000);
}

TEST_F(BombTest, S2_BombDoesNotUseEnergy) {
    bomber.energy = 0;
    EXPECT_EQ(skills.check(bombAt({3, 3}), bomber, board, kReady), std::nullopt);
    skills.apply(bombAt({3, 3}), bomber, board, kReady);
    EXPECT_EQ(bomber.energy, 0);
}

TEST_F(BombTest, SB2_EmptyTargetIsInvalid) {
    EXPECT_EQ(skills.check(bombAt({0, 0}), bomber, board, kReady), RejectReason::InvalidTarget);
}

TEST_F(BombTest, SB2_OwnStoneIsInvalid) {
    EXPECT_EQ(skills.check(bombAt({4, 4}), bomber, board, kReady), RejectReason::InvalidTarget);
}

TEST_F(BombTest, SB2_OutOfBoardTarget) {
    EXPECT_EQ(skills.check(bombAt({-1, 3}), bomber, board, kReady), RejectReason::OutOfBoard);
    EXPECT_EQ(skills.check(bombAt({3, 15}), bomber, board, kReady), RejectReason::OutOfBoard);
}

TEST_F(BombTest, SB5_MissingTargetIsInvalid) {
    const SkillAction noTarget{PlayerId::Black, SkillId::Bomb, std::nullopt};
    EXPECT_EQ(skills.check(noTarget, bomber, board, kReady), RejectReason::InvalidTarget);
}

TEST_F(BombTest, SB4_NotOwnedBeforeOutOfBoard) {
    PlayerState accelerateOwner = playerWith(SkillId::Accelerate);
    skills.initPlayer(accelerateOwner, 0);
    EXPECT_EQ(skills.check(bombAt({-1, -1}), accelerateOwner, board, kReady), RejectReason::SkillNotOwned);
}

TEST_F(BombTest, SB4_OutOfBoardBeforeCooldown) {
    EXPECT_EQ(skills.check(bombAt({15, 15}), bomber, board, 0), RejectReason::OutOfBoard);
}

TEST_F(BombTest, SB4_SB5_MissingTargetCheckedWhereOutOfBoardIs) {
    const SkillAction noTarget{PlayerId::Black, SkillId::Bomb, std::nullopt};
    EXPECT_EQ(skills.check(noTarget, bomber, board, 0), RejectReason::InvalidTarget);  // 排在冷卻之前
}

TEST_F(BombTest, SB4_CooldownBeforeInvalidTarget) {
    EXPECT_EQ(skills.check(bombAt({0, 0}), bomber, board, 0), RejectReason::SkillCooldown);
    EXPECT_EQ(skills.check(bombAt({4, 4}), bomber, board, 0), RejectReason::SkillCooldown);
}
