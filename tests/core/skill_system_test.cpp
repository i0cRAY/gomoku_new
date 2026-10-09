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
