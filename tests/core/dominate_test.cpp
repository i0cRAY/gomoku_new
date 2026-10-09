#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "core/board.h"
#include "core/config.h"
#include "core/player_state.h"
#include "core/skill_system.h"
#include "core/zone_map.h"

namespace {

SkillAction dominate(PlayerId player = PlayerId::Black) {
    return SkillAction{player, SkillId::Dominate, std::nullopt};
}

bool containsPos(const std::vector<ZoneCell>& zones, Pos p) {
    return std::any_of(zones.begin(), zones.end(), [p](const ZoneCell& z) { return z.pos == p; });
}

}  // namespace

// ---- ZoneMap ----

TEST(ZoneMapTest, SZ3_RestrictsOnlyOpponent) {
    ZoneMap zones;
    zones.add({5, 5}, PlayerId::Black, 3000);
    EXPECT_TRUE(zones.isRestricted({5, 5}, PlayerId::White, 0));
    EXPECT_FALSE(zones.isRestricted({5, 5}, PlayerId::Black, 0));  // 不限制自己
    EXPECT_FALSE(zones.isRestricted({5, 6}, PlayerId::White, 0));
}

TEST(ZoneMapTest, SZ3_ExpiresExactlyAtExpiryTime) {
    ZoneMap zones;
    zones.add({5, 5}, PlayerId::Black, 3000);
    EXPECT_TRUE(zones.isRestricted({5, 5}, PlayerId::White, 2999));
    EXPECT_FALSE(zones.isRestricted({5, 5}, PlayerId::White, 3000));  // 剛好到期那一刻可以下
}

TEST(ZoneMapTest, SZ3_OverlappingZonesTimedSeparately) {
    ZoneMap zones;
    zones.add({5, 5}, PlayerId::Black, 3000);
    zones.add({5, 5}, PlayerId::Black, 4500);
    EXPECT_TRUE(zones.isRestricted({5, 5}, PlayerId::White, 3500));
    EXPECT_FALSE(zones.isRestricted({5, 5}, PlayerId::White, 4500));
}

TEST(ZoneMapTest, SZ4_ActiveListsUnexpiredZones) {
    ZoneMap zones;
    zones.add({1, 1}, PlayerId::Black, 1000);
    zones.add({2, 2}, PlayerId::White, 5000);
    const auto active = zones.active(2000);
    ASSERT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0].pos, (Pos{2, 2}));
    EXPECT_EQ(active[0].owner, PlayerId::White);
    EXPECT_EQ(active[0].expiresAt, 5000);
}

TEST(ZoneMapTest, SZ3_RemoveExpiredAndRemoveAtAndClear) {
    ZoneMap zones;
    zones.add({1, 1}, PlayerId::Black, 1000);
    zones.add({2, 2}, PlayerId::Black, 5000);
    zones.add({3, 3}, PlayerId::Black, 5000);
    zones.removeExpired(1000);
    EXPECT_EQ(zones.active(0).size(), 2u);
    zones.removeAt({2, 2});
    EXPECT_FALSE(zones.isRestricted({2, 2}, PlayerId::White, 0));
    EXPECT_TRUE(zones.isRestricted({3, 3}, PlayerId::White, 0));
    zones.clear();
    EXPECT_TRUE(zones.active(0).empty());
}

// ---- 霸道（T08a）----

class DominateTest : public ::testing::Test {
protected:
    void SetUp() override {
        state.energy = 3;
        state.skill = SkillId::Dominate;
        skills.initPlayer(state);
    }

    static constexpr TimeMs kReady = 20000;  // 只是任意的使用時間
    SkillConfig config;
    SkillSystem skills{config};
    Board board;
    ZoneMap zones;
    PlayerState state;
};

TEST_F(DominateTest, S3_NeedsThreeEnergy) {
    EXPECT_EQ(state.dominateCharges, 0);
    EXPECT_EQ(skills.check(dominate(), state, board), std::nullopt);
    state.energy = 2;
    EXPECT_EQ(skills.check(dominate(), state, board), RejectReason::NoEnergy);
}

TEST_F(DominateTest, SZ1_UseGivesThreeCharges) {
    ASSERT_EQ(skills.check(dominate(), state, board), std::nullopt);
    skills.apply(dominate(), state, board, zones, kReady);
    EXPECT_EQ(state.dominateCharges, 3);
    EXPECT_TRUE(zones.active(kReady).empty());  // 使用本身不產生禁區
}

TEST_F(DominateTest, SZ1_TargetIsIgnored) {
    const SkillAction withTarget{PlayerId::Black, SkillId::Dominate, Pos{-3, 99}};
    EXPECT_EQ(skills.check(withTarget, state, board), std::nullopt);
}

TEST_F(DominateTest, SZ2_PlacingConsumesChargeAndCreatesCrossZone) {
    skills.apply(dominate(), state, board, zones, kReady);
    skills.onPlaced(PlayerId::Black, state, {7, 7}, zones, 21000);
    EXPECT_EQ(state.dominateCharges, 2);
    const auto active = zones.active(21000);
    ASSERT_EQ(active.size(), 4u);
    for (Pos p : {Pos{7, 6}, Pos{7, 8}, Pos{6, 7}, Pos{8, 7}}) {
        EXPECT_TRUE(containsPos(active, p));
        EXPECT_TRUE(zones.isRestricted(p, PlayerId::White, 21000));
    }
    EXPECT_FALSE(containsPos(active, {7, 7}));
    EXPECT_FALSE(containsPos(active, {8, 8}));  // 斜角不算
    for (const ZoneCell& z : active) {
        EXPECT_EQ(z.owner, PlayerId::Black);
        EXPECT_EQ(z.expiresAt, 21000 + config.zoneDuration);
    }
}

TEST_F(DominateTest, SZ2_CornerZoneClippedToBoard) {
    skills.apply(dominate(), state, board, zones, kReady);
    skills.onPlaced(PlayerId::Black, state, {0, 0}, zones, 21000);
    const auto active = zones.active(21000);
    ASSERT_EQ(active.size(), 2u);
    EXPECT_TRUE(containsPos(active, {1, 0}));
    EXPECT_TRUE(containsPos(active, {0, 1}));
}

TEST_F(DominateTest, SZ2_OnlyThreeStonesCreateZones) {
    skills.apply(dominate(), state, board, zones, kReady);
    skills.onPlaced(PlayerId::Black, state, {2, 2}, zones, 21000);
    skills.onPlaced(PlayerId::Black, state, {6, 6}, zones, 22000);
    skills.onPlaced(PlayerId::Black, state, {10, 10}, zones, 23000);
    EXPECT_EQ(state.dominateCharges, 0);
    skills.onPlaced(PlayerId::Black, state, {13, 2}, zones, 24000);
    EXPECT_EQ(state.dominateCharges, 0);
    EXPECT_FALSE(zones.isRestricted({13, 3}, PlayerId::White, 24000));
    EXPECT_TRUE(zones.isRestricted({10, 11}, PlayerId::White, 24000));
}

TEST_F(DominateTest, SZ2_NoChargesNoZone) {
    skills.onPlaced(PlayerId::Black, state, {7, 7}, zones, 1000);
    EXPECT_TRUE(zones.active(1000).empty());
}

TEST_F(DominateTest, SZ1_ReuseResetsChargesWithoutStacking) {
    skills.apply(dominate(), state, board, zones, kReady);
    skills.onPlaced(PlayerId::Black, state, {7, 7}, zones, 21000);
    ASSERT_EQ(state.dominateCharges, 2);
    ASSERT_EQ(skills.check(dominate(), state, board), std::nullopt);  // 沒有冷卻，能量夠就能再用
    skills.apply(dominate(), state, board, zones, 22000);
    EXPECT_EQ(state.dominateCharges, 3);
}

TEST_F(DominateTest, SZ5_CheckOrder) {
    PlayerState other;
    other.skill = SkillId::Bomb;
    skills.initPlayer(other);
    EXPECT_EQ(skills.check(dominate(), other, board), RejectReason::SkillNotOwned);  // 能量不足也先報沒有技能
    state.energy = 0;
    EXPECT_EQ(skills.check(dominate(), state, board), RejectReason::NoEnergy);
}

TEST_F(DominateTest, S2_ApplyLeavesEnergyToCaller) {
    skills.apply(dominate(), state, board, zones, kReady);
    EXPECT_EQ(state.energy, 3);  // 扣能量由 GameController 處理
}
