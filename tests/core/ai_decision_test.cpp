#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <utility>
#include <variant>

#include "board_test_util.h"
#include "core/ai_engine.h"
#include "core/config.h"
#include "core/player_view.h"

namespace {

constexpr TimeMs kReaction = 350;  // spec A2：固定反應時間

struct ViewSpec {
    std::vector<std::string_view> rows;
    SkillId skill = SkillId::Dominate;
    int energy = 3;  // 夠用技能（S2），但不到使用霸道的 4（A8a）
    TimeMs cooldownRemaining = 0;  // 0：下子間隔已過
    std::vector<Pos> zones = {};   // 對手（白）的禁區，尚未到期
    int dominateCharges = 0;
    bool destroyUsed = false;
    TimeMs now = 30000;
};

// 黑方（X）的視角；O 是對手
PlayerView viewOf(const ViewSpec& spec) {
    PlayerView v;
    v.me = PlayerId::Black;
    v.board = boardFromRows(spec.rows);
    v.status = GameStatus::Running;
    v.now = spec.now;
    v.self.energy = spec.energy;
    v.self.skill = spec.skill;
    v.placeCooldownRemaining = spec.cooldownRemaining;
    v.self.dominateCharges = spec.dominateCharges;
    v.self.destroyUsed = spec.destroyUsed;
    for (Pos p : spec.zones) {
        v.zones.push_back({p, PlayerId::White, spec.now + 1000});
    }
    return v;
}

AIEngine engine(std::uint32_t seed = 1) {
    return AIEngine(PlayerId::Black, AIConfig{}, SkillConfig{}, seed);
}

std::optional<Pos> placedAt(const std::optional<Action>& action) {
    if (!action) {
        return std::nullopt;
    }
    if (const auto* place = std::get_if<PlaceAction>(&*action)) {
        return place->pos;
    }
    return std::nullopt;
}

const SkillAction* skillOf(const std::optional<Action>& action) {
    return action ? std::get_if<SkillAction>(&*action) : nullptr;
}

std::set<std::pair<int, int>> cells(std::initializer_list<std::pair<int, int>> list) {
    return std::set<std::pair<int, int>>(list);
}

bool placedIn(const std::optional<Action>& action, const std::set<std::pair<int, int>>& allowed) {
    const auto pos = placedAt(action);
    return pos && allowed.count({pos->x, pos->y}) == 1;
}

bool usedSkill(const std::optional<Action>& action, SkillId skill) {
    const SkillAction* s = skillOf(action);
    return s && s->skill == skill;
}

// 第 7 列放一排棋子（避免碰到上下邊界）
ViewSpec rowSpec(std::string_view row) {
    return ViewSpec{{"", "", "", "", "", "", "", row}};
}

}  // namespace

// ---- A4：自己一步成五 ----

TEST(AIDecisionTest, A4_CompletesOwnFive) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", ".OXXXX..."}}));
    EXPECT_EQ(placedAt(action), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A4_WinBeatsBlocking) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", ".OXXXX...", "", "..OOOO..."}}));
    EXPECT_EQ(placedAt(action), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A4_NoActionWhenCannotPlace) {
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "", "", "", "", ".OXXXX..."}};
    spec.energy = 0;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);
}

// ---- A5：對手有成五點 ----

TEST(AIDecisionTest, A5_BlocksSingleFivePoint) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", ".XOOOO..."}}));
    EXPECT_EQ(placedAt(action), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A5_DoubleThreatWithBombBombsThreatStone) {
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "", "", "", "", "..OOOO..."}};
    spec.skill = SkillId::Bomb;
    const auto action = ai.decide(viewOf(spec));
    const SkillAction* bomb = skillOf(action);
    ASSERT_NE(bomb, nullptr);
    EXPECT_EQ(bomb->skill, SkillId::Bomb);
    ASSERT_TRUE(bomb->target.has_value());
    EXPECT_EQ(bomb->target->y, 7);
    EXPECT_GE(bomb->target->x, 2);
    EXPECT_LE(bomb->target->x, 5);
}

TEST(AIDecisionTest, A5_DoubleThreatWithDominateStillBlocksOne) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", "..OOOO..."}}));
    EXPECT_TRUE(placedIn(action, cells({{1, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A5_DoubleThreatWithBombButNoEnergyForItBlocksOne) {
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "", "", "", "", "..OOOO..."}};
    spec.skill = SkillId::Bomb;
    spec.energy = 2;  // 能下子，但不夠用炸彈
    const auto action = ai.decide(viewOf(spec));
    EXPECT_TRUE(placedIn(action, cells({{1, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A5_CannotPlaceButBombReadyBombs) {
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "", "", "", "", ".XOOOO..."}};
    spec.skill = SkillId::Bomb;
    spec.cooldownRemaining = 300;  // 下子間隔未過
    const auto action = ai.decide(viewOf(spec));
    const SkillAction* bomb = skillOf(action);
    ASSERT_NE(bomb, nullptr);
    EXPECT_EQ(bomb->skill, SkillId::Bomb);
}

TEST(AIDecisionTest, A5_CannotPlaceAndNoBombDoesNothing) {
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "", "", "", "", ".XOOOO..."}};
    spec.energy = 0;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);
}

TEST(AIDecisionTest, A5_BlockChoiceMinimisesThreat) {
    // 兩個成五點：(6,7) 只擋橫向；(6,9) 是另一條線。擋任何一個都剩 1 個成五點，所以兩者都可以，
    // 但結果一定是成五點之一
    AIEngine ai = engine();
    const auto action =
        ai.decide(viewOf({{"", "", "", "", "", "", "", ".XOOOO...", "", ".XOOOO..."}}));
    EXPECT_TRUE(placedIn(action, cells({{6, 7}, {6, 9}})));
}

// ---- A6：自己能形成活四 ----

TEST(AIDecisionTest, A6_MakesOpenFour) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", "...XXX..."}}));
    EXPECT_TRUE(placedIn(action, cells({{2, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A6_OpenFourBeatsBlockingOpenThree) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", "...XXX...", "", "...OOO..."}}));
    EXPECT_TRUE(placedIn(action, cells({{2, 7}, {6, 7}})));
}

// ---- A7：對手有活三 ----

TEST(AIDecisionTest, A7_BlocksOpenThreeAtEnd) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", "...OOO..."}}));
    EXPECT_TRUE(placedIn(action, cells({{2, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A7_JumpThreeCanBeBlockedInTheGap) {
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "", "", "..OO.O..."}}));
    EXPECT_TRUE(placedIn(action, cells({{1, 7}, {4, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A7_PrefersBlockThatHelpsSelf) {
    // 兩端都能擋；(6,7) 同時讓自己直向形成活三，評分較高
    AIEngine ai = engine();
    const auto action = ai.decide(viewOf({{"", "", "", "", "", "......X", "......X", "...OOO..."}}));
    EXPECT_EQ(placedAt(action), std::optional<Pos>(Pos{6, 7}));
}

// ---- A9：沒有威脅時不浪費技能 ----

TEST(AIDecisionTest, A9_BombOwnerPlacesWhenNoThreat) {
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "", "", "", "", ".....X..."}};
    spec.skill = SkillId::Bomb;
    const auto action = ai.decide(viewOf(spec));
    EXPECT_TRUE(placedAt(action).has_value());
}

// ---- A9：評分 ----

TEST(AIDecisionTest, A9_A12_EmptyBoardPlaysCenter) {
    AIEngine ai = engine();
    EXPECT_EQ(placedAt(ai.decide(viewOf({{""}}))), std::optional<Pos>(Pos{7, 7}));
}

TEST(AIDecisionTest, A9_NoActionWhenCannotPlace) {
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "", "", "", "", ".....X..."}};
    spec.energy = 0;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);
}

// ---- A5、A5a：成五點在對手禁區內視為不能擋 ----

TEST(AIDecisionTest, A5_A5a_SingleFivePointInZoneAndNoSkillDoesNothing) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".XOOOO...");
    spec.zones = {{6, 7}};
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);  // 不往下做 A9
}

TEST(AIDecisionTest, A5_A5a_SingleFivePointInZoneBombs) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".XOOOO...");
    spec.skill = SkillId::Bomb;
    spec.zones = {{6, 7}};
    EXPECT_TRUE(usedSkill(ai.decide(viewOf(spec)), SkillId::Bomb));
}

TEST(AIDecisionTest, A5_A5a_SingleFivePointInZoneDestroys) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".XOOOO...");
    spec.skill = SkillId::Destroy;
    spec.zones = {{6, 7}};
    EXPECT_TRUE(usedSkill(ai.decide(viewOf(spec)), SkillId::Destroy));
}

TEST(AIDecisionTest, A5_A5a_DoubleThreatBlocksPointOutsideZone) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("..OOOO...");
    spec.zones = {{1, 7}};
    EXPECT_EQ(placedAt(ai.decide(viewOf(spec))), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A5a_ExpiredZoneDoesNotBlock) {
    AIEngine ai = engine();
    PlayerView view = viewOf(rowSpec(".XOOOO..."));
    view.zones = {{{6, 7}, PlayerId::White, view.now}};  // 剛好到期（SZ3）
    EXPECT_EQ(placedAt(ai.decide(view)), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A5a_OwnZoneDoesNotRestrictSelf) {
    AIEngine ai = engine();
    PlayerView view = viewOf(rowSpec(".XOOOO..."));
    view.zones = {{{6, 7}, PlayerId::Black, view.now + 1000}};  // 自己產生的禁區（SZ3）
    EXPECT_EQ(placedAt(ai.decide(view)), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A4_A5a_OwnFivePointInZoneIsSkipped) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("#XXXX....");
    spec.zones = {{5, 7}};
    const auto action = ai.decide(viewOf(spec));
    ASSERT_TRUE(placedAt(action).has_value());
    EXPECT_NE(*placedAt(action), (Pos{5, 7}));
}

TEST(AIDecisionTest, A6_A5a_OpenFourPointInZoneIsSkipped) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("...XXX...");
    spec.zones = {{2, 7}};
    EXPECT_EQ(placedAt(ai.decide(viewOf(spec))), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A7_A5a_BlockCandidatesExcludeZone) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("...OOO...");
    spec.zones = {{2, 7}};
    EXPECT_EQ(placedAt(ai.decide(viewOf(spec))), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A9_A5a_NeverPicksZoneCell) {
    // 最高分的幾格都在禁區內，換任何 seed 都不會選它們
    const std::vector<Pos> zone{{2, 7}, {3, 7}, {4, 7}, {7, 7}, {8, 7}, {9, 7}};
    for (std::uint32_t seed = 0; seed < 20; ++seed) {
        AIEngine ai = engine(seed);
        ViewSpec spec = rowSpec(".....XX..");
        spec.zones = zone;
        const auto pos = placedAt(ai.decide(viewOf(spec)));
        ASSERT_TRUE(pos.has_value());
        EXPECT_EQ(std::find(zone.begin(), zone.end(), *pos), zone.end()) << "seed " << seed;
    }
}

// ---- A12：中央不能下時改用評分 ----

TEST(AIDecisionTest, A12_DestroyedCenterNotPlayed) {
    for (std::uint32_t seed = 0; seed < 10; ++seed) {
        AIEngine ai = engine(seed);
        ViewSpec spec = rowSpec(".......#");  // 只有中央 (7,7) 已摧毀，沒有棋子
        const auto pos = placedAt(ai.decide(viewOf(spec)));
        ASSERT_TRUE(pos.has_value());
        EXPECT_NE(*pos, (Pos{7, 7}));
    }
}

TEST(AIDecisionTest, A12_A5a_CenterInZoneNotPlayed) {
    for (std::uint32_t seed = 0; seed < 10; ++seed) {
        AIEngine ai = engine(seed);
        ViewSpec spec{{""}};
        spec.zones = {{7, 7}};
        const auto pos = placedAt(ai.decide(viewOf(spec)));
        ASSERT_TRUE(pos.has_value());
        EXPECT_NE(*pos, (Pos{7, 7}));
    }
}

// ---- A5b：摧毀 ----

TEST(AIDecisionTest, A5b_DoubleThreatDestroysBestArea) {
    // 威脅棋子在 (2..5, 7)。中心 (3,5)、(4,5) 都涵蓋 5 顆白子，但 (3,5) 也涵蓋黑子 (1,5)，所以選 (4,5)
    AIEngine ai = engine();
    ViewSpec spec{{"", "", "", "....O", "", ".X", "", "..OOOO..."}};
    spec.skill = SkillId::Destroy;
    const SkillAction* destroy = skillOf(ai.decide(viewOf(spec)));
    ASSERT_NE(destroy, nullptr);
    EXPECT_EQ(destroy->skill, SkillId::Destroy);
    EXPECT_EQ(destroy->target, std::optional<Pos>(Pos{4, 5}));
}

TEST(AIDecisionTest, A5b_TargetAlwaysCoversThreatStone) {
    // 遠處有一大群白子，但不在威脅棋型裡，摧毀範圍仍要涵蓋威脅棋子
    AIEngine ai = engine();
    ViewSpec spec{{"..........OOO", "..........OOO", "..........OOO", "", "", "", "", "..OOOO..."}};
    spec.skill = SkillId::Destroy;
    const SkillAction* destroy = skillOf(ai.decide(viewOf(spec)));
    ASSERT_NE(destroy, nullptr);
    ASSERT_TRUE(destroy->target.has_value());
    EXPECT_GE(destroy->target->x, 0);
    EXPECT_LE(destroy->target->x, 7);
    EXPECT_GE(destroy->target->y, 5);
    EXPECT_LE(destroy->target->y, 9);
}

TEST(AIDecisionTest, A5b_TieBrokenReproduciblyWithSeed) {
    for (std::uint32_t seed : {1u, 2u, 7u}) {
        AIEngine a = engine(seed);
        AIEngine b = engine(seed);
        ViewSpec spec = rowSpec("..OOOO...");
        spec.skill = SkillId::Destroy;
        const SkillAction* x = skillOf(a.decide(viewOf(spec)));
        const SkillAction* y = skillOf(b.decide(viewOf(spec)));
        ASSERT_NE(x, nullptr);
        ASSERT_NE(y, nullptr);
        EXPECT_EQ(x->target, y->target);
    }
}

TEST(AIDecisionTest, A5b_CannotPlaceDestroys) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".XOOOO...");
    spec.skill = SkillId::Destroy;
    spec.cooldownRemaining = 300;
    EXPECT_TRUE(usedSkill(ai.decide(viewOf(spec)), SkillId::Destroy));
}

TEST(AIDecisionTest, A5b_SingleBlockablePointIsBlockedNotDestroyed) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".XOOOO...");
    spec.skill = SkillId::Destroy;
    EXPECT_EQ(placedAt(ai.decide(viewOf(spec))), std::optional<Pos>(Pos{6, 7}));
}

TEST(AIDecisionTest, A5b_SX3_UsedDestroyIsNotTriedAgain) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("..OOOO...");
    spec.skill = SkillId::Destroy;
    spec.destroyUsed = true;
    EXPECT_TRUE(placedIn(ai.decide(viewOf(spec)), cells({{1, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A5b_DestroyNeedsEnergy) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("..OOOO...");
    spec.skill = SkillId::Destroy;
    spec.energy = 2;
    EXPECT_TRUE(placedIn(ai.decide(viewOf(spec)), cells({{1, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A5b_DestroyOnlyUsedForA5) {
    for (std::string_view row : {"...OOO...", ".....X...", "...XXX..."}) {  // A7、A9、A6
        AIEngine ai = engine();
        ViewSpec spec = rowSpec(row);
        spec.skill = SkillId::Destroy;
        spec.energy = 10;
        const auto action = ai.decide(viewOf(spec));
        EXPECT_TRUE(placedAt(action).has_value()) << row;
    }
}

// ---- A8a：霸道 ----

TEST(AIDecisionTest, A8a_UsesDominateWithNoChargesAndEnoughEnergy) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".....X...");
    spec.energy = 4;
    EXPECT_TRUE(usedSkill(ai.decide(viewOf(spec)), SkillId::Dominate));
}

TEST(AIDecisionTest, A8a_NeedsEnergyToSpareForAPlacement) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".....X...");
    spec.energy = 3;  // 夠用霸道，但用完就不能馬上下子
    EXPECT_TRUE(placedAt(ai.decide(viewOf(spec))).has_value());
}

TEST(AIDecisionTest, A8a_NotUsedWhileChargesRemain) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".....X...");
    spec.energy = 10;
    spec.dominateCharges = 1;
    EXPECT_TRUE(placedAt(ai.decide(viewOf(spec))).has_value());
}

TEST(AIDecisionTest, A8a_EnergyThresholdFollowsSkillCost) {
    SkillConfig skill;
    skill.energyCost = 5;
    AIEngine ai(PlayerId::Black, AIConfig{}, skill, 1);
    ViewSpec spec = rowSpec(".....X...");
    spec.energy = 5;
    EXPECT_TRUE(placedAt(ai.decide(viewOf(spec))).has_value());
    spec.now += kReaction;
    spec.energy = 6;
    EXPECT_TRUE(usedSkill(ai.decide(viewOf(spec)), SkillId::Dominate));
}

TEST(AIDecisionTest, A8a_OpenThreeBlockComesFirst) {
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("...OOO...");
    spec.energy = 10;
    EXPECT_TRUE(placedIn(ai.decide(viewOf(spec)), cells({{2, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A8a_UsedEvenIfCannotPlace) {
    // 使用技能不受下子間隔影響（S5）
    AIEngine ai = engine();
    ViewSpec spec = rowSpec(".....X...");
    spec.energy = 4;
    spec.cooldownRemaining = 300;
    EXPECT_TRUE(usedSkill(ai.decide(viewOf(spec)), SkillId::Dominate));
}

// ---- S1a：每種技能的 AI 只用自己的技能 ----

TEST(AIDecisionTest, S1a_AIOnlyUsesOwnSkill) {
    // A8a 的條件成立時，選炸彈、摧毀的 AI 不會用霸道
    for (SkillId skill : {SkillId::Bomb, SkillId::Destroy}) {
        AIEngine ai = engine();
        ViewSpec spec = rowSpec(".....X...");
        spec.skill = skill;
        spec.energy = 10;
        EXPECT_TRUE(placedAt(ai.decide(viewOf(spec))).has_value());
    }
    // 擋不完時，選霸道的 AI 不會用炸彈或摧毀
    AIEngine ai = engine();
    ViewSpec spec = rowSpec("..OOOO...");
    spec.energy = 10;
    spec.dominateCharges = 3;
    EXPECT_TRUE(placedIn(ai.decide(viewOf(spec)), cells({{1, 7}, {6, 7}})));
    // 選炸彈的 AI 擋不完時用炸彈，不用摧毀
    AIEngine bomber = engine();
    ViewSpec bombSpec = rowSpec("..OOOO...");
    bombSpec.skill = SkillId::Bomb;
    EXPECT_TRUE(usedSkill(bomber.decide(viewOf(bombSpec)), SkillId::Bomb));
}

// ---- A2：反應時間 ----

TEST(AIDecisionTest, A2_OneDecisionPerReactionTime) {
    AIEngine ai = engine();
    ViewSpec spec{{""}};
    spec.now = 0;
    EXPECT_TRUE(ai.decide(viewOf(spec)).has_value());  // 開局立即決策
    spec.now = kReaction - 1;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);
    spec.now = kReaction;
    EXPECT_TRUE(ai.decide(viewOf(spec)).has_value());
}

TEST(AIDecisionTest, A2_NoActionStillCountsAsDecision) {
    AIEngine ai = engine();
    ViewSpec spec{{""}};
    spec.now = 0;
    spec.energy = 0;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);  // 不行動，但算一次決策
    spec.now = 300;
    spec.energy = 1;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);  // 反應時間未到
    spec.now = kReaction;
    EXPECT_TRUE(ai.decide(viewOf(spec)).has_value());
}

TEST(AIDecisionTest, A2_ReactionTimeComesFromConfig) {
    EXPECT_EQ(AIConfig{}.reactionTime, kReaction);
    AIConfig config;
    config.reactionTime = 600;
    AIEngine ai(PlayerId::Black, config, SkillConfig{}, 1);
    ViewSpec spec{{""}};
    spec.now = 0;
    ASSERT_TRUE(ai.decide(viewOf(spec)).has_value());
    spec.now = config.reactionTime - 1;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);
    spec.now = config.reactionTime;
    EXPECT_TRUE(ai.decide(viewOf(spec)).has_value());
}

// ---- A1：下子間隔從 PlayerView 讀取 ----

TEST(AIDecisionTest, A1_NoPlacementWhileCooldownRemaining) {
    AIEngine ai = engine();
    ViewSpec spec{{""}};
    spec.cooldownRemaining = 1;
    EXPECT_EQ(ai.decide(viewOf(spec)), std::nullopt);
    spec.now += kReaction;
    spec.cooldownRemaining = 0;
    EXPECT_TRUE(placedIn(ai.decide(viewOf(spec)), cells({{7, 7}})));
}

// ---- S2、A5：技能耗能取自 SkillConfig ----

TEST(AIDecisionTest, S2_A5_BombEnergyCostComesFromConfig) {
    SkillConfig skill;
    skill.energyCost = 4;
    AIEngine ai(PlayerId::Black, AIConfig{}, skill, 1);
    ViewSpec spec{{"", "", "", "", "", "", "", "..OOOO..."}};
    spec.skill = SkillId::Bomb;
    spec.energy = 3;  // 預設耗能 3 夠用，但這裡要 4
    const auto action = ai.decide(viewOf(spec));
    EXPECT_EQ(skillOf(action), nullptr);
    EXPECT_TRUE(placedIn(action, cells({{1, 7}, {6, 7}})));
}

TEST(AIDecisionTest, A2_NewGameResetsReactionTimer) {
    AIEngine ai = engine();
    ViewSpec spec{{""}};
    spec.now = 90000;
    ASSERT_TRUE(ai.decide(viewOf(spec)).has_value());
    ai.newGame();
    spec.now = 0;
    EXPECT_TRUE(ai.decide(viewOf(spec)).has_value());
}

TEST(AIDecisionTest, A1_NoDecisionWhenNotRunning) {
    AIEngine ai = engine();
    PlayerView view = viewOf({{""}});
    view.status = GameStatus::Countdown;
    EXPECT_EQ(ai.decide(view), std::nullopt);
}

// ---- A2a：技能選擇 ----

TEST(AIDecisionTest, A2a_ChooseSkillReproducibleWithSeed) {
    for (std::uint32_t seed : {1u, 2u, 3u, 99u}) {
        AIEngine a = engine(seed);
        AIEngine b = engine(seed);
        EXPECT_EQ(a.chooseSkill(), b.chooseSkill());
    }
}

TEST(AIDecisionTest, A2a_ChooseSkillUsesAllThreeSkills) {
    std::set<SkillId> seen;
    for (std::uint32_t seed = 0; seed < 30; ++seed) {
        seen.insert(engine(seed).chooseSkill());
    }
    EXPECT_EQ(seen.size(), 3u);
}

// ---- A3、E5：只吃得到 PlayerView ----

TEST(AIDecisionTest, A3_E5_DecideOnlyTakesOwnView) {
    // decide 的唯一參數是 PlayerView，型別上沒有對手的 PlayerState 可讀
    static_assert(std::is_invocable_r_v<std::optional<Action>, decltype(&AIEngine::decide), AIEngine&,
                                        const PlayerView&>);
    SUCCEED();
}
