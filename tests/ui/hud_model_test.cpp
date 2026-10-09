#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ui/hud_model.h"

namespace {

PlayerView viewWith(int energy, double ratio) {
    PlayerView v;
    v.status = GameStatus::Running;
    v.self.energy = energy;
    v.nextEnergyRatio = ratio;
    return v;
}

}  // namespace

// ---- E6：能量條 ----

TEST(HudModelTest, E6_FilledSegmentsThenPartialSegment) {
    const auto segments = energySegments(viewWith(3, 0.5), 10);
    ASSERT_EQ(segments.size(), 10u);
    EXPECT_DOUBLE_EQ(segments[0], 1.0);
    EXPECT_DOUBLE_EQ(segments[1], 1.0);
    EXPECT_DOUBLE_EQ(segments[2], 1.0);
    EXPECT_DOUBLE_EQ(segments[3], 0.5);
    for (std::size_t i = 4; i < 10; ++i) {
        EXPECT_DOUBLE_EQ(segments[i], 0.0);
    }
}

TEST(HudModelTest, E6_ZeroEnergyStillShowsProgress) {
    const auto segments = energySegments(viewWith(0, 0.25), 10);
    EXPECT_DOUBLE_EQ(segments[0], 0.25);
    EXPECT_DOUBLE_EQ(segments[1], 0.0);
}

TEST(HudModelTest, E6_ConsumingShiftsPartialSegmentLeft) {
    const auto before = energySegments(viewWith(5, 0.6), 10);
    const auto after = energySegments(viewWith(4, 0.6), 10);
    EXPECT_DOUBLE_EQ(before[5], 0.6);
    EXPECT_DOUBLE_EQ(after[4], 0.6);
    EXPECT_DOUBLE_EQ(after[5], 0.0);
}

TEST(HudModelTest, E6_FullEnergyFillsAllWithoutPartial) {
    const auto segments = energySegments(viewWith(10, 0.0), 10);
    for (double s : segments) {
        EXPECT_DOUBLE_EQ(s, 1.0);
    }
}

// ---- U2：下子間隔、冷卻與加速剩餘時間 ----

TEST(HudModelTest, U2_PlaceReadyWhenNoPreviousPlacement) {
    PlayerView v = viewWith(1, 0.0);
    EXPECT_TRUE(isPlaceReady(v, 1000));
}

TEST(HudModelTest, U2_PlaceNotReadyWithinInterval) {
    PlayerView v = viewWith(1, 0.0);
    v.self.lastPlaceTime = 5000;
    v.now = 5999;
    EXPECT_FALSE(isPlaceReady(v, 1000));
    v.now = 6000;
    EXPECT_TRUE(isPlaceReady(v, 1000));
}

TEST(HudModelTest, U2_CooldownSecondsRoundUp) {
    PlayerView v = viewWith(1, 0.0);
    v.self.skillReadyAt = 25000;
    v.now = 12100;
    EXPECT_EQ(skillCooldownSeconds(v), 13);  // 12.9 秒 → 顯示 13
    v.now = 24999;
    EXPECT_EQ(skillCooldownSeconds(v), 1);
    v.now = 25000;
    EXPECT_EQ(skillCooldownSeconds(v), 0);
    v.now = 30000;
    EXPECT_EQ(skillCooldownSeconds(v), 0);
}

TEST(HudModelTest, U2_SA1_AccelerateRemainingSeconds) {
    PlayerView v = viewWith(1, 0.0);
    v.self.accelerateUntil = 35000;
    v.accelerating = true;
    v.now = 31500;
    EXPECT_EQ(accelerateRemainingSeconds(v), 4);
    v.accelerating = false;
    EXPECT_EQ(accelerateRemainingSeconds(v), 0);
}

// ---- U2、U5：文字 ----

TEST(HudModelTest, U5_EveryRejectReasonHasDistinctText) {
    const RejectReason all[] = {RejectReason::GameNotRunning, RejectReason::OutOfBoard, RejectReason::Occupied,
                                RejectReason::PlaceCooldown,  RejectReason::NoEnergy,   RejectReason::SkillNotOwned,
                                RejectReason::SkillCooldown,  RejectReason::InvalidTarget};
    std::set<std::string> texts;
    for (RejectReason r : all) {
        const std::string text = rejectReasonText(r);
        EXPECT_FALSE(text.empty());
        texts.insert(text);
    }
    EXPECT_EQ(texts.size(), std::size(all));
}

TEST(HudModelTest, U2_SkillNames) {
    EXPECT_NE(skillName(SkillId::Accelerate), skillName(SkillId::Bomb));
    EXPECT_FALSE(skillName(SkillId::Accelerate).empty());
}
