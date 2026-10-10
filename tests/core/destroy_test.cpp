#include <gtest/gtest.h>

#include "core/board.h"
#include "core/config.h"
#include "core/player_state.h"
#include "core/skill_system.h"
#include "core/zone_map.h"

namespace {

SkillAction destroyAt(std::optional<Pos> target) {
    return SkillAction{PlayerId::Black, SkillId::Destroy, target};
}

int countCells(const Board& board, Cell cell) {
    int n = 0;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            n += board.at({x, y}) == cell ? 1 : 0;
        }
    }
    return n;
}

}  // namespace

class DestroyTest : public ::testing::Test {
protected:
    void SetUp() override {
        state.energy = 3;
        state.skill = SkillId::Destroy;
        skills.initPlayer(state);
    }

    static constexpr TimeMs kReady = 30000;  // 只是任意的使用時間
    SkillConfig config;
    SkillSystem skills{config};
    Board board;
    ZoneMap zones;
    PlayerState state;
};

TEST_F(DestroyTest, S3_NeedsThreeEnergy) {
    EXPECT_FALSE(state.destroyUsed);
    EXPECT_EQ(skills.check(destroyAt(Pos{7, 7}), state, board), std::nullopt);
    state.energy = 2;
    EXPECT_EQ(skills.check(destroyAt(Pos{7, 7}), state, board), RejectReason::NoEnergy);
}

TEST_F(DestroyTest, SX2_DestroysFiveByFiveIncludingBothColors) {
    board.set({7, 7}, Cell::Black);
    board.set({5, 5}, Cell::White);  // 範圍左上角
    board.set({9, 9}, Cell::Black);  // 範圍右下角
    board.set({4, 7}, Cell::White);  // 範圍外
    skills.apply(destroyAt(Pos{7, 7}), state, board, zones, kReady);
    EXPECT_EQ(countCells(board, Cell::Destroyed), 25);
    EXPECT_EQ(board.at({5, 5}), Cell::Destroyed);
    EXPECT_EQ(board.at({9, 9}), Cell::Destroyed);
    EXPECT_EQ(board.at({7, 7}), Cell::Destroyed);
    EXPECT_EQ(board.at({4, 7}), Cell::White);
    EXPECT_EQ(board.at({10, 7}), Cell::Empty);
    EXPECT_TRUE(state.destroyUsed);
}

TEST_F(DestroyTest, SX2_CornerTargetClippedToBoard) {
    skills.apply(destroyAt(Pos{0, 0}), state, board, zones, kReady);
    EXPECT_EQ(countCells(board, Cell::Destroyed), 9);  // (0..2) × (0..2)
    EXPECT_EQ(board.at({2, 2}), Cell::Destroyed);
    EXPECT_EQ(board.at({3, 0}), Cell::Empty);
}

TEST_F(DestroyTest, SX1_AnyCellCanBeTarget) {
    board.set({7, 7}, Cell::White);
    EXPECT_EQ(skills.check(destroyAt(Pos{7, 7}), state, board), std::nullopt);
    board.set({7, 7}, Cell::Destroyed);
    EXPECT_EQ(skills.check(destroyAt(Pos{7, 7}), state, board), std::nullopt);
}

TEST_F(DestroyTest, SX2_ZonesInsideAreaRemoved) {
    zones.add({7, 8}, PlayerId::White, 99999);
    zones.add({0, 0}, PlayerId::White, 99999);
    skills.apply(destroyAt(Pos{7, 7}), state, board, zones, kReady);
    EXPECT_FALSE(zones.isRestricted({7, 8}, PlayerId::Black, kReady));
    EXPECT_TRUE(zones.isRestricted({0, 0}, PlayerId::Black, kReady));
}

TEST_F(DestroyTest, SX3_SecondUseRejected) {
    skills.apply(destroyAt(Pos{7, 7}), state, board, zones, kReady);
    EXPECT_EQ(skills.check(destroyAt(Pos{0, 0}), state, board), RejectReason::SkillUsedUp);
}

TEST_F(DestroyTest, SX4_CheckOrder) {
    PlayerState other;
    other.skill = SkillId::Bomb;
    skills.initPlayer(other);
    EXPECT_EQ(skills.check(destroyAt(Pos{-1, 0}), other, board), RejectReason::SkillNotOwned);
    state.energy = 0;
    EXPECT_EQ(skills.check(destroyAt(Pos{15, 0}), state, board), RejectReason::OutOfBoard);  // 排在能量之前
    state.destroyUsed = true;
    EXPECT_EQ(skills.check(destroyAt(Pos{15, 0}), state, board), RejectReason::OutOfBoard);  // 排在用完之前
    EXPECT_EQ(skills.check(destroyAt(Pos{7, 7}), state, board), RejectReason::SkillUsedUp);  // 排在能量之前
}

TEST_F(DestroyTest, SX5_MissingTargetIsInvalid) {
    EXPECT_EQ(skills.check(destroyAt(std::nullopt), state, board), RejectReason::InvalidTarget);
    state.energy = 0;
    EXPECT_EQ(skills.check(destroyAt(std::nullopt), state, board), RejectReason::InvalidTarget);  // 排在能量之前
}

TEST(BombOnDestroyedTest, SB1_SB3_DestroyedCellIsValidTargetAndStaysDestroyed) {
    SkillSystem skills{SkillConfig{}};
    PlayerState bomber;
    bomber.energy = 3;
    bomber.skill = SkillId::Bomb;
    skills.initPlayer(bomber);
    Board board;
    ZoneMap zones;
    board.set({3, 3}, Cell::Destroyed);
    board.set({4, 3}, Cell::White);
    const SkillAction bomb{PlayerId::Black, SkillId::Bomb, Pos{3, 3}};
    ASSERT_EQ(skills.check(bomb, bomber, board), std::nullopt);
    skills.apply(bomb, bomber, board, zones, 0);
    EXPECT_EQ(board.at({3, 3}), Cell::Destroyed);
    EXPECT_TRUE(board.isEmpty({4, 3}));
}
