#include <gtest/gtest.h>

#include <algorithm>

#include "core/config.h"
#include "core/energy_manager.h"
#include "core/player_state.h"

namespace {

MatchConfig configWithInterval(TimeMs regenInterval) {
    MatchConfig config;
    config.regenInterval = regenInterval;
    return config;
}

PlayerState stateWith(int energy, TimeMs progress = 0) {
    PlayerState state;
    state.energy = energy;
    state.regenProgress = progress;
    return state;
}

}  // namespace

TEST(EnergyTest, E2_ProgressGrowsOneEveryMillisecond) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(1);
    energy.advance(state, 0, 1500);
    EXPECT_EQ(state.energy, 1);
    EXPECT_EQ(state.regenProgress, 1500);
}

TEST(EnergyTest, E3_GainOneEnergyWhenProgressReachesInterval) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(1);
    energy.advance(state, 0, 2000);
    EXPECT_EQ(state.energy, 2);
    EXPECT_EQ(state.regenProgress, 0);
}

TEST(EnergyTest, E3_LeftoverProgressIsKept) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(1, 1500);
    energy.advance(state, 0, 1000);
    EXPECT_EQ(state.energy, 2);
    EXPECT_EQ(state.regenProgress, 500);
}

TEST(EnergyTest, E3_CatchUpAfterLongGap) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(1);
    energy.advance(state, 0, 7000);  // 3 格 + 1000
    EXPECT_EQ(state.energy, 4);
    EXPECT_EQ(state.regenProgress, 1000);
}

TEST(EnergyTest, E3_TenSecondGapFillsToMax) {
    const EnergyManager energy(configWithInterval(1000));
    PlayerState state = stateWith(1);
    energy.advance(state, 0, 10000);
    EXPECT_EQ(state.energy, 10);
    EXPECT_EQ(state.regenProgress, 0);
}

TEST(EnergyTest, E3_ResultDoesNotDependOnTickSize) {
    const EnergyManager energy(configWithInterval(1500));
    PlayerState oneStep = stateWith(0);
    energy.advance(oneStep, 0, 7777);

    PlayerState manySteps = stateWith(0);
    for (TimeMs t = 0; t < 7777; t += 50) {
        energy.advance(manySteps, t, std::min<TimeMs>(t + 50, 7777));
    }
    EXPECT_EQ(manySteps.energy, oneStep.energy);
    EXPECT_EQ(manySteps.regenProgress, oneStep.regenProgress);
}

TEST(EnergyTest, E3_NoTimePassedChangesNothing) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(3, 700);
    energy.advance(state, 5000, 5000);
    EXPECT_EQ(state.energy, 3);
    EXPECT_EQ(state.regenProgress, 700);
}

TEST(EnergyTest, E1_E4_EnergyNeverExceedsMax) {
    const EnergyManager energy(configWithInterval(500));
    PlayerState state = stateWith(9, 400);
    energy.advance(state, 0, 1'000'000);
    EXPECT_EQ(state.energy, 10);
}

TEST(EnergyTest, E4_ProgressResetsWhenReachingMax) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(9, 1500);
    energy.advance(state, 0, 2000);  // 500 ms 時就到上限，多出的 1500 不保留
    EXPECT_EQ(state.energy, 10);
    EXPECT_EQ(state.regenProgress, 0);
}

TEST(EnergyTest, E4_ProgressStaysZeroWhileFull) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(10);
    energy.advance(state, 0, 5000);
    EXPECT_EQ(state.energy, 10);
    EXPECT_EQ(state.regenProgress, 0);
}

TEST(EnergyTest, E4_RestartFromZeroAfterConsumingAtMax) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(10);
    energy.advance(state, 0, 3000);
    energy.consume(state);
    EXPECT_EQ(state.energy, 9);
    EXPECT_EQ(state.regenProgress, 0);

    energy.advance(state, 3000, 3800);
    EXPECT_EQ(state.regenProgress, 800);
}

TEST(EnergyTest, E6_ConsumeBelowMaxKeepsProgress) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(5, 1200);
    energy.consume(state);
    EXPECT_EQ(state.energy, 4);
    EXPECT_EQ(state.regenProgress, 1200);
}

TEST(EnergyTest, E1_CannotConsumeAtZero) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(1);
    EXPECT_TRUE(energy.canConsume(state));
    energy.consume(state);
    EXPECT_EQ(state.energy, 0);
    EXPECT_FALSE(energy.canConsume(state));
}

TEST(EnergyTest, E6_NextEnergyRatioIsProgressOverInterval) {
    const EnergyManager energy(configWithInterval(2000));
    EXPECT_DOUBLE_EQ(energy.nextEnergyRatio(stateWith(3, 1000)), 0.5);
    EXPECT_DOUBLE_EQ(energy.nextEnergyRatio(stateWith(3, 0)), 0.0);
    EXPECT_DOUBLE_EQ(energy.nextEnergyRatio(stateWith(0, 1500)), 0.75);
}

TEST(EnergyTest, E6_NextEnergyRatioIsZeroWhenFull) {
    const EnergyManager energy(configWithInterval(2000));
    EXPECT_DOUBLE_EQ(energy.nextEnergyRatio(stateWith(10)), 0.0);
}

// ---- S2：技能一次消耗多格 ----

TEST(EnergyTest, S2_ConsumeThreeAtOnce) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(5, 700);
    EXPECT_TRUE(energy.canConsume(state, 3));
    energy.consume(state, 3);
    EXPECT_EQ(state.energy, 2);
    EXPECT_EQ(state.regenProgress, 700);  // E6：進度不變
}

TEST(EnergyTest, S3_CannotConsumeMoreThanAvailable) {
    const EnergyManager energy(configWithInterval(2000));
    const PlayerState state = stateWith(2);
    EXPECT_FALSE(energy.canConsume(state, 3));
    EXPECT_TRUE(energy.canConsume(state, 2));
}

TEST(EnergyTest, S2_E4_ConsumeThreeAtMaxRestartsFromZero) {
    const EnergyManager energy(configWithInterval(2000));
    PlayerState state = stateWith(10);
    energy.advance(state, 0, 3000);
    energy.consume(state, 3);
    EXPECT_EQ(state.energy, 7);
    EXPECT_EQ(state.regenProgress, 0);
    energy.advance(state, 3000, 5500);
    EXPECT_EQ(state.energy, 8);
    EXPECT_EQ(state.regenProgress, 500);
}
