#include <gtest/gtest.h>

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
    int energy = 5;  // ≥ 3：夠用技能（S2）
    TimeMs cooldownRemaining = 0;  // 0：下子間隔已過
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
