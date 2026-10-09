#include <gtest/gtest.h>

#include <variant>

#include "ui/board_input.h"

namespace {

PlayerView runningView(SkillId skill) {
    PlayerView v;
    v.me = PlayerId::Black;
    v.status = GameStatus::Running;
    v.self.skill = skill;
    v.board.set({3, 3}, Cell::White);
    v.board.set({4, 4}, Cell::Black);
    return v;
}

}  // namespace

TEST(BoardInputTest, U1_ClickPlacesStoneNormally) {
    BoardInput input(PlayerId::Black);
    const auto action = input.onBoardClick({7, 7}, runningView(SkillId::Bomb));
    ASSERT_TRUE(action.has_value());
    const auto* place = std::get_if<PlaceAction>(&*action);
    ASSERT_NE(place, nullptr);
    EXPECT_EQ(place->player, PlayerId::Black);
    EXPECT_EQ(place->pos, (Pos{7, 7}));
}

TEST(BoardInputTest, U3_BombSkillKeyEntersTargetingMode) {
    BoardInput input(PlayerId::Black);
    EXPECT_EQ(input.onSkillKey(runningView(SkillId::Bomb)), std::nullopt);
    EXPECT_TRUE(input.isTargeting());
}

TEST(BoardInputTest, U3_ClickInTargetingModeSendsBomb) {
    BoardInput input(PlayerId::Black);
    const PlayerView view = runningView(SkillId::Bomb);
    input.onSkillKey(view);
    const auto action = input.onBoardClick({3, 3}, view);
    ASSERT_TRUE(action.has_value());
    const auto* skill = std::get_if<SkillAction>(&*action);
    ASSERT_NE(skill, nullptr);
    EXPECT_EQ(skill->skill, SkillId::Bomb);
    EXPECT_EQ(skill->target, std::optional<Pos>(Pos{3, 3}));
    EXPECT_FALSE(input.isTargeting());  // 送出後回到一般模式
}

TEST(BoardInputTest, U3_InvalidTargetIsStillSentForRejection) {
    BoardInput input(PlayerId::Black);
    const PlayerView view = runningView(SkillId::Bomb);
    input.onSkillKey(view);
    const auto action = input.onBoardClick({4, 4}, view);  // 自己的子：交給 GameController 拒絕（P6 提示）
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<SkillAction>(*action));
}

TEST(BoardInputTest, U3_CancelLeavesTargetingMode) {
    BoardInput input(PlayerId::Black);
    const PlayerView view = runningView(SkillId::Bomb);
    input.onSkillKey(view);
    input.cancel();
    EXPECT_FALSE(input.isTargeting());
    const auto action = input.onBoardClick({3, 3}, view);
    ASSERT_TRUE(action.has_value());
    EXPECT_TRUE(std::holds_alternative<PlaceAction>(*action));  // 取消後點擊是下子
}

TEST(BoardInputTest, U3_PressingSkillKeyAgainCancelsTargeting) {
    BoardInput input(PlayerId::Black);
    const PlayerView view = runningView(SkillId::Bomb);
    input.onSkillKey(view);
    input.onSkillKey(view);
    EXPECT_FALSE(input.isTargeting());
}

TEST(BoardInputTest, U3_TargetingEndsWhenGameStops) {
    BoardInput input(PlayerId::Black);
    PlayerView view = runningView(SkillId::Bomb);
    input.onSkillKey(view);
    view.status = GameStatus::BlackWon;
    input.onViewChanged(view);
    EXPECT_FALSE(input.isTargeting());
}

// ---- 霸道、摧毀（T13 🔄）----

TEST(BoardInputTest, U4_SZ1_DominateSkillKeySendsImmediately) {
    BoardInput input(PlayerId::Black);
    const auto action = input.onSkillKey(runningView(SkillId::Dominate));
    ASSERT_TRUE(action.has_value());
    const auto* skill = std::get_if<SkillAction>(&*action);
    ASSERT_NE(skill, nullptr);
    EXPECT_EQ(skill->skill, SkillId::Dominate);
    EXPECT_EQ(skill->target, std::nullopt);
    EXPECT_FALSE(input.isTargeting());
}

TEST(BoardInputTest, U3_DestroyEntersTargetingAndSendsDestroy) {
    BoardInput input(PlayerId::White);
    const PlayerView view = runningView(SkillId::Destroy);
    EXPECT_EQ(input.onSkillKey(view), std::nullopt);
    EXPECT_TRUE(input.isTargeting());
    const auto action = input.onBoardClick({9, 2}, view);
    ASSERT_TRUE(action.has_value());
    const auto* skill = std::get_if<SkillAction>(&*action);
    ASSERT_NE(skill, nullptr);
    EXPECT_EQ(skill->player, PlayerId::White);
    EXPECT_EQ(skill->skill, SkillId::Destroy);
    EXPECT_EQ(skill->target, std::optional<Pos>(Pos{9, 2}));
}

TEST(BoardInputTest, U3_NeedsTargetOnlyForBombAndDestroy) {
    EXPECT_TRUE(needsTarget(SkillId::Bomb));
    EXPECT_FALSE(needsTarget(SkillId::Dominate));
    EXPECT_TRUE(needsTarget(SkillId::Destroy));
}
