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

// ---- U2、U5：文字 ----

TEST(HudModelTest, U5_EveryRejectReasonHasDistinctText) {
    const RejectReason all[] = {RejectReason::GameNotRunning, RejectReason::OutOfBoard, RejectReason::Occupied,
                                RejectReason::PlaceCooldown,  RejectReason::NoEnergy,   RejectReason::SkillNotOwned,
                                RejectReason::InvalidTarget,  RejectReason::DestroyedCell,
                                RejectReason::RestrictedZone, RejectReason::SkillUsedUp};
    std::set<std::string> texts;
    for (RejectReason r : all) {
        const std::string text = rejectReasonText(r);
        EXPECT_FALSE(text.empty());
        texts.insert(text);
    }
    EXPECT_EQ(texts.size(), std::size(all));
}

TEST(HudModelTest, U2_SkillNames) {
    const SkillId all[] = {SkillId::Bomb, SkillId::Dominate, SkillId::Destroy};
    std::set<std::string> names;
    for (SkillId s : all) {
        EXPECT_FALSE(skillName(s).empty());
        names.insert(skillName(s));
    }
    EXPECT_EQ(names.size(), std::size(all));
}

// ---- U2：霸道次數、摧毀已用（T13 🔄）----

TEST(HudModelTest, U2_SZ1_DominateChargesText) {
    PlayerView v = viewWith(1, 0.0);
    v.self.skill = SkillId::Dominate;
    EXPECT_EQ(skillDetailText(v), "");
    v.self.dominateCharges = 2;
    EXPECT_EQ(skillDetailText(v), "霸道：還有 2 子");
}

TEST(HudModelTest, U2_SX3_DestroyUsedText) {
    PlayerView v = viewWith(5, 0.0);
    v.self.skill = SkillId::Destroy;
    EXPECT_EQ(skillDetailText(v), "每局一次");
    v.self.destroyUsed = true;
    EXPECT_EQ(skillDetailText(v), "已使用");
    EXPECT_FALSE(isSkillAvailable(v, 3));  // 能量夠也不能再用
}

TEST(HudModelTest, U2_S3_SkillAvailabilityDependsOnEnergy) {
    PlayerView v = viewWith(2, 0.0);
    v.self.skill = SkillId::Bomb;
    EXPECT_FALSE(isSkillAvailable(v, 3));
    v.self.energy = 3;
    EXPECT_TRUE(isSkillAvailable(v, 3));
}

TEST(HudModelTest, U2_S3_SkillStatusText) {
    PlayerView v = viewWith(2, 0.0);
    v.self.skill = SkillId::Bomb;
    EXPECT_EQ(skillStatusText(v, 3), "能量不足（需要 3 格）");
    v.self.energy = 4;
    EXPECT_EQ(skillStatusText(v, 3), "可使用（消耗 3 格）");
    v.self.skill = SkillId::Destroy;
    v.self.destroyUsed = true;
    EXPECT_EQ(skillStatusText(v, 3), "");  // detail 已顯示「已使用」
}

TEST(HudModelTest, U3_TargetingPromptDependsOnSkill) {
    const SkillConfig config;
    EXPECT_NE(targetingPrompt(SkillId::Bomb, config), targetingPrompt(SkillId::Destroy, config));
    EXPECT_NE(targetingPrompt(SkillId::Destroy, config).find("5×5"), std::string::npos);
}

TEST(HudModelTest, U2_SX3_SkillButtonDisabledAfterDestroyUsed) {
    PlayerView v = viewWith(0, 0.0);
    v.self.skill = SkillId::Destroy;
    EXPECT_TRUE(isSkillButtonEnabled(v));  // 能量不足仍可按，按了會提示原因（U5）
    v.self.destroyUsed = true;
    EXPECT_FALSE(isSkillButtonEnabled(v));
    v.self.skill = SkillId::Bomb;
    EXPECT_TRUE(isSkillButtonEnabled(v));
}

// ---- U8：計分板 ----

TEST(HudModelTest, U8_ScoreText) {
    PlayerView v;
    v.scores = {5, 11};
    EXPECT_EQ(scoreText(v), "黑 5 : 11 白");
}

TEST(HudModelTest, U8_TimeLimitShowsRemainingClock) {
    PlayerView v;
    v.mode = MatchMode::TimeLimit;
    v.timeRemaining = 179001;
    EXPECT_EQ(matchInfoText(v), "剩餘 3:00");
    v.timeRemaining = 61000;
    EXPECT_EQ(matchInfoText(v), "剩餘 1:01");
    v.timeRemaining = 9000;
    EXPECT_EQ(matchInfoText(v), "剩餘 0:09");
    v.timeRemaining = 0;
    EXPECT_EQ(matchInfoText(v), "剩餘 0:00");
}

TEST(HudModelTest, U8_ScoreTargetShowsTarget) {
    PlayerView v;
    v.mode = MatchMode::ScoreTarget;
    v.targetScore = 25;
    EXPECT_EQ(matchInfoText(v), "先得 25 分獲勝");
}
