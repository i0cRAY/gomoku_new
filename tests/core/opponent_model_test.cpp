#include <gtest/gtest.h>

#include <vector>

#include "core/config.h"
#include "core/opponent_model.h"
#include "core/player_view.h"

// spec A14、A14a：黑方（AI）從自己的 PlayerView 推算白方（對手）的能量與下子間隔

namespace {

// 一連串手寫的 PlayerView：每次修改後呼叫 observeAt
class Feed {
public:
    explicit Feed(MatchConfig config = {}) : config(config), model(PlayerId::Black, config) {
        view.me = PlayerId::Black;
        view.status = GameStatus::Running;
    }

    void observeAt(TimeMs now) {
        view.now = now;
        model.observe(view);
    }
    void white(int x, int y) { view.board.set({x, y}, Cell::White); }
    void black(int x, int y) { view.board.set({x, y}, Cell::Black); }
    void empty(int x, int y) { view.board.set({x, y}, Cell::Empty); }
    // 白方產生禁區的一子：周圍 8 格在 expiresAt 前是禁區（SZ2）
    void whiteWithZone(int x, int y, TimeMs expiresAt) {
        white(x, y);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx != 0 || dy != 0) {
                    view.zones.push_back({{x + dx, y + dy}, PlayerId::White, expiresAt});
                }
            }
        }
    }

    MatchConfig config;
    OpponentModel model;
    PlayerView view;
};

MatchConfig withStartEnergy(int energy) {
    MatchConfig config;
    config.startEnergy = energy;
    return config;
}

}  // namespace

// ---- A14：推算能量（E2–E4） ----

TEST(OpponentModelTest, A14_StartsWithStartEnergy) {
    Feed feed;
    feed.observeAt(0);
    EXPECT_EQ(feed.model.energy(), 3);
}

TEST(OpponentModelTest, A14_E2_E3_RegensOverTime) {
    Feed feed;
    feed.observeAt(0);
    feed.observeAt(4100);
    EXPECT_EQ(feed.model.energy(), 5);
}

TEST(OpponentModelTest, A14_E4_FullEnergyRestartsAfterSpending) {
    Feed feed;
    feed.observeAt(0);
    feed.observeAt(20000);
    EXPECT_EQ(feed.model.energy(), 10);
    feed.white(7, 7);
    feed.observeAt(20000);
    EXPECT_EQ(feed.model.energy(), 9);
    feed.observeAt(21999);
    EXPECT_EQ(feed.model.energy(), 9);
    feed.observeAt(22000);  // 從扣掉那一刻起重新累積
    EXPECT_EQ(feed.model.energy(), 10);
}

// ---- A14：觀察對手的行動 ----

TEST(OpponentModelTest, A14_PlacementCostsOneAndStartsCooldown) {
    Feed feed;
    feed.observeAt(0);
    feed.white(7, 7);
    feed.observeAt(1000);
    EXPECT_EQ(feed.model.energy(), 2);
    EXPECT_EQ(feed.model.placementsWithin(499), 0);  // 下子間隔到 1500
    EXPECT_EQ(feed.model.placementsWithin(500), 1);
}

TEST(OpponentModelTest, A14_P4_NoCooldownBeforeFirstPlacement) {
    Feed feed;
    feed.observeAt(0);
    EXPECT_EQ(feed.model.placementsWithin(0), 1);
}

TEST(OpponentModelTest, A14_StonesOnFirstObservationAreNotCounted) {
    Feed feed;
    feed.white(1, 1);
    feed.white(2, 2);
    feed.observeAt(0);  // 第一次觀察只當作起點
    EXPECT_EQ(feed.model.energy(), 3);
}

TEST(OpponentModelTest, A14_W1_ClearedFiveCountsAsOnePlacement) {
    Feed feed;
    for (int x = 0; x < 4; ++x) {
        feed.white(x, 7);
    }
    feed.observeAt(0);
    // 白方下在 (4,7) 連成五，五子當場被消除，盤面上看不到新棋子
    for (int x = 0; x < 4; ++x) {
        feed.empty(x, 7);
    }
    feed.view.lastClearedLines = {ClearedLine{PlayerId::White, {{0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}}}};
    feed.observeAt(100);
    EXPECT_EQ(feed.model.energy(), 2);
    feed.observeAt(150);  // 同一批消除不重複算
    EXPECT_EQ(feed.model.energy(), 2);
}

TEST(OpponentModelTest, A14_W6_TwoLinesFromOneStoneCountOnce) {
    Feed feed;
    for (int i = 0; i < 4; ++i) {
        feed.white(i, 7);
        feed.white(4, 8 + i);
    }
    feed.observeAt(0);
    for (int i = 0; i < 4; ++i) {
        feed.empty(i, 7);
        feed.empty(4, 8 + i);
    }
    feed.view.lastClearedLines = {ClearedLine{PlayerId::White, {{0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}}},
                                  ClearedLine{PlayerId::White, {{4, 7}, {4, 8}, {4, 9}, {4, 10}, {4, 11}}}};
    feed.observeAt(100);
    EXPECT_EQ(feed.model.energy(), 2);
}

TEST(OpponentModelTest, A14_OwnActionsDoNotChangeEstimate) {
    Feed feed;
    for (int x = 0; x < 4; ++x) {
        feed.black(x, 3);
    }
    feed.observeAt(0);
    feed.black(9, 9);
    for (int x = 0; x < 4; ++x) {
        feed.empty(x, 3);
    }
    feed.view.lastClearedLines = {ClearedLine{PlayerId::Black, {{0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}}}};
    feed.view.board.set({12, 12}, Cell::Destroyed);  // 自己的摧毀
    feed.view.self.destroyUsed = true;
    feed.view.zones.push_back({{8, 8}, PlayerId::Black, 3000});  // 自己的霸道禁區
    feed.observeAt(100);
    EXPECT_EQ(feed.model.energy(), 3);
    EXPECT_EQ(feed.model.placementsWithin(0), 1);  // 對手還沒下過子，沒有間隔
}

TEST(OpponentModelTest, A14_DestroyCountsOnce) {
    Feed feed;
    feed.observeAt(0);
    EXPECT_TRUE(feed.model.mayDestroy());
    feed.view.board.set({7, 7}, Cell::Destroyed);
    feed.view.opponentSkillRevealed = SkillId::Destroy;
    feed.observeAt(100);
    EXPECT_EQ(feed.model.energy(), 2);
    EXPECT_FALSE(feed.model.mayDestroy());  // SX3：每局只有一次
    feed.observeAt(150);
    EXPECT_EQ(feed.model.energy(), 2);
}

TEST(OpponentModelTest, A14_DominateRevealAndHiddenRecast) {
    MatchConfig config = withStartEnergy(10);
    config.regenInterval = 10000;  // 這段時間內不回能
    Feed feed(config);
    feed.observeAt(0);
    feed.view.opponentSkillRevealed = SkillId::Dominate;
    feed.observeAt(100);
    EXPECT_EQ(feed.model.energy(), 9);  // 第一次揭露：霸道 1 次，得到 3 次霸道次數
    EXPECT_FALSE(feed.model.mayDestroy());
    feed.whiteWithZone(2, 2, 3600);
    feed.observeAt(600);
    feed.whiteWithZone(6, 2, 4200);
    feed.observeAt(1200);
    feed.whiteWithZone(10, 2, 4800);
    feed.observeAt(1800);
    EXPECT_EQ(feed.model.energy(), 6);  // 3 次下子，次數用完
    feed.whiteWithZone(2, 10, 5400);
    feed.observeAt(2400);
    EXPECT_EQ(feed.model.energy(), 4);  // 次數是 0 又產生禁區：中間又用了一次霸道
}

TEST(OpponentModelTest, A14_EstimateNeverGoesNegative) {
    Feed feed(withStartEnergy(0));
    feed.observeAt(0);
    feed.white(7, 7);
    feed.observeAt(100);  // 推算是 0 卻看到下子：推算偏低，停在 0
    EXPECT_EQ(feed.model.energy(), 0);
}

TEST(OpponentModelTest, A14_ResetStartsOver) {
    Feed feed;
    feed.observeAt(0);
    feed.white(7, 7);
    feed.view.opponentSkillRevealed = SkillId::Dominate;
    feed.observeAt(1000);
    feed.model.reset();
    feed.view = PlayerView{};
    feed.view.status = GameStatus::Running;
    feed.observeAt(0);
    EXPECT_EQ(feed.model.energy(), 3);
    EXPECT_TRUE(feed.model.mayDestroy());
    EXPECT_EQ(feed.model.placementsWithin(0), 1);
}

// ---- A14a：對手的應對能力 ----

TEST(OpponentModelTest, A14a_PlacementsWithinCountsRegenAndCooldown) {
    Feed feed(withStartEnergy(0));
    feed.observeAt(0);
    EXPECT_EQ(feed.model.placementsWithin(1999), 0);
    EXPECT_EQ(feed.model.placementsWithin(2000), 1);  // 回能剛好在時限內
    EXPECT_EQ(feed.model.placementsWithin(3999), 1);  // 第二格還沒回來
    EXPECT_EQ(feed.model.placementsWithin(4000), 2);
}

TEST(OpponentModelTest, A14a_PlacementsWithinLimitedByCooldown) {
    Feed feed(withStartEnergy(10));
    feed.observeAt(0);
    EXPECT_EQ(feed.model.placementsWithin(0), 1);
    EXPECT_EQ(feed.model.placementsWithin(499), 1);
    EXPECT_EQ(feed.model.placementsWithin(500), 2);  // 每 500 ms 一子
    EXPECT_EQ(feed.model.placementsWithin(1000), 3);
}

TEST(OpponentModelTest, A14a_PlacementsWithinDoesNotChangeEstimate) {
    Feed feed;
    feed.observeAt(0);
    feed.model.placementsWithin(5000);
    EXPECT_EQ(feed.model.energy(), 3);
}

TEST(OpponentModelTest, A14a_MayDestroyUntilSkillRevealed) {
    Feed feed;
    feed.observeAt(0);
    EXPECT_TRUE(feed.model.mayDestroy());  // 還沒揭露技能，可能是摧毀
    feed.view.opponentSkillRevealed = SkillId::Dominate;
    feed.observeAt(100);
    EXPECT_FALSE(feed.model.mayDestroy());
    // 揭露的是摧毀時，揭露就代表用過了（S1、SX3），見 A14_DestroyCountsOnce
}

TEST(OpponentModelTest, A14a_CanRespondCountsDestroyOnlyWhenAsked) {
    Feed feed(withStartEnergy(2));
    feed.observeAt(0);
    feed.white(7, 7);
    feed.observeAt(0);  // 能量剩 1，下子間隔到 500
    EXPECT_FALSE(feed.model.canRespondWithin(400, false));
    EXPECT_TRUE(feed.model.canRespondWithin(400, true));  // 摧毀不受下子間隔限制（S5）
    EXPECT_TRUE(feed.model.canRespondWithin(500, false));
}

TEST(OpponentModelTest, A14a_NoEnergyCannotRespond) {
    Feed feed(withStartEnergy(0));
    feed.observeAt(0);
    EXPECT_FALSE(feed.model.canRespondWithin(850, true));
    EXPECT_TRUE(feed.model.canRespondWithin(2000, true));
}
