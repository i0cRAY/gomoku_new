#include "lan_fixture.h"

// ---- RemoteSession::extrapolate（design §4.9：兩次快照之間依本機時間往前推）----

namespace {

PlayerView runningView() {
    PlayerView v;
    v.me = PlayerId::White;
    v.status = GameStatus::Running;
    v.now = 10000;
    v.self.energy = 4;
    v.nextEnergyRatio = 0.5;
    v.mode = MatchMode::TimeLimit;
    v.timeRemaining = 60000;
    v.placeCooldownRemaining = 300;
    return v;
}

}  // namespace

TEST(RemoteExtrapolateTest, E6_EnergyRatioGrowsWithLocalTime) {
    MatchConfig config;  // 回能間隔 2000
    const PlayerView v = RemoteSession::extrapolate(runningView(), 500, config);
    EXPECT_DOUBLE_EQ(v.nextEnergyRatio, 0.75);
    EXPECT_EQ(v.self.energy, 4);  // 格數等主機的快照
    EXPECT_EQ(v.now, 10500);
}

TEST(RemoteExtrapolateTest, E6_EnergyRatioStopsAtOne) {
    MatchConfig config;
    const PlayerView v = RemoteSession::extrapolate(runningView(), 5000, config);
    EXPECT_DOUBLE_EQ(v.nextEnergyRatio, 1.0);
    EXPECT_EQ(v.self.energy, 4);
}

TEST(RemoteExtrapolateTest, E4_FullEnergyDoesNotAccumulate) {
    MatchConfig config;
    PlayerView full = runningView();
    full.self.energy = config.maxEnergy;
    full.nextEnergyRatio = 0.0;
    EXPECT_DOUBLE_EQ(RemoteSession::extrapolate(full, 500, config).nextEnergyRatio, 0.0);
}

TEST(RemoteExtrapolateTest, U8_TimeAndCooldownCountDown) {
    MatchConfig config;
    const PlayerView v = RemoteSession::extrapolate(runningView(), 400, config);
    EXPECT_EQ(v.timeRemaining, 59600);
    EXPECT_EQ(v.placeCooldownRemaining, 0);
}

TEST(RemoteExtrapolateTest, G2_CountdownCountsDown) {
    MatchConfig config;
    PlayerView v;
    v.status = GameStatus::Countdown;
    v.now = -2000;
    v.countdownRemaining = 2000;
    const PlayerView later = RemoteSession::extrapolate(v, 2500, config);
    EXPECT_EQ(later.countdownRemaining, 0);
    EXPECT_EQ(later.now, 0);
    EXPECT_EQ(later.status, GameStatus::Countdown);  // 開始與否等主機決定
}

TEST(RemoteExtrapolateTest, SZ3_ExpiredZonesDisappear) {
    MatchConfig config;
    PlayerView v = runningView();
    v.zones = {ZoneCell{{1, 1}, PlayerId::Black, 10200}, ZoneCell{{2, 2}, PlayerId::Black, 11000}};
    const PlayerView later = RemoteSession::extrapolate(v, 200, config);  // 剛好到期那一刻不是禁區
    ASSERT_EQ(later.zones.size(), 1u);
    EXPECT_EQ(later.zones[0].pos.x, 2);
}

TEST(RemoteExtrapolateTest, N3_FinishedViewIsUnchanged) {
    MatchConfig config;
    PlayerView v = runningView();
    v.status = GameStatus::WhiteWon;
    const PlayerView later = RemoteSession::extrapolate(v, 1000, config);
    EXPECT_EQ(later.now, v.now);
    EXPECT_DOUBLE_EQ(later.nextEnergyRatio, v.nextEnergyRatio);
}

// ---- 加入方與主機的整合測試 ----

class RemoteSessionTest : public LanFixture {};

TEST_F(RemoteSessionTest, N7_JoinerUsesHostSettings) {
    const PlayerView view = joiner->viewFor(PlayerId::White);
    EXPECT_EQ(view.me, PlayerId::White);
    EXPECT_EQ(view.mode, MatchMode::ScoreTarget);
    EXPECT_EQ(view.targetScore, 5);
}

TEST_F(RemoteSessionTest, U7_OpponentReadyBothWays) {
    joiner->selectSkill(PlayerId::White, SkillId::Destroy);
    joiner->confirmSkill(PlayerId::White);
    ASSERT_TRUE(waitUntil([&] { return hostLog.opponentReady == 1; }));
    EXPECT_EQ(joinLog.opponentReady, 0);
    host->selectSkill(PlayerId::Black, SkillId::Dominate);
    host->confirmSkill(PlayerId::Black);
    ASSERT_TRUE(waitUntil([&] { return joinLog.opponentReady == 1; }));
    ASSERT_TRUE(waitUntil([&] { return joinerStatus() == GameStatus::Running; }));
    EXPECT_EQ(joiner->viewFor(PlayerId::White).self.skill, SkillId::Destroy);
}

TEST_F(RemoteSessionTest, N3_RequestWaitsForHost) {
    startMatch();
    joiner->request(PlaceAction{PlayerId::White, {4, 4}});
    EXPECT_TRUE(joiner->viewFor(PlayerId::White).board.isEmpty({4, 4}));  // 還沒收到主機的結果
    ASSERT_TRUE(waitUntil([&] { return joiner->viewFor(PlayerId::White).board.at({4, 4}) == Cell::White; }));
    EXPECT_EQ(host->viewFor(PlayerId::Black).board.at({4, 4}), Cell::White);
    EXPECT_EQ(joiner->viewFor(PlayerId::White).self.energy, config.startEnergy - 1);
}

TEST_F(RemoteSessionTest, P6_RejectReportsReasonAndCell) {
    startMatch();
    host->request(PlaceAction{PlayerId::Black, {6, 6}});
    ASSERT_TRUE(waitUntil([&] { return joiner->viewFor(PlayerId::White).board.at({6, 6}) == Cell::Black; }));
    joiner->request(PlaceAction{PlayerId::White, {6, 6}});
    ASSERT_TRUE(waitUntil([&] { return joinLog.rejections.size() == 1; }));
    EXPECT_EQ(joinLog.rejections[0].first, RejectReason::Occupied);
    ASSERT_TRUE(joinLog.rejections[0].second.has_value());
    EXPECT_EQ(joinLog.rejections[0].second->x, 6);
    EXPECT_EQ(joinLog.rejections[0].second->y, 6);
}

TEST_F(RemoteSessionTest, E5_JoinerSeesOnlyOwnEnergy) {
    startMatch();
    host->request(PlaceAction{PlayerId::Black, {0, 0}});
    host->request(PlaceAction{PlayerId::Black, {2, 0}});
    ASSERT_TRUE(waitUntil([&] { return joiner->viewFor(PlayerId::White).board.at({2, 0}) == Cell::Black; }));
    EXPECT_EQ(joiner->viewFor(PlayerId::White).self.energy, config.startEnergy);
    EXPECT_EQ(host->viewFor(PlayerId::Black).self.energy, config.startEnergy - 2);
}

TEST_F(RemoteSessionTest, S1_OpponentSkillRevealedAfterUse) {
    startMatch();
    EXPECT_FALSE(joiner->viewFor(PlayerId::White).opponentSkillRevealed.has_value());
    host->request(SkillAction{PlayerId::Black, SkillId::Dominate, std::nullopt});
    ASSERT_TRUE(waitUntil([&] { return joiner->viewFor(PlayerId::White).opponentSkillRevealed.has_value(); }));
    EXPECT_EQ(*joiner->viewFor(PlayerId::White).opponentSkillRevealed, SkillId::Dominate);
}

TEST_F(RemoteSessionTest, SX2_JoinerDestroyIsVisibleToBoth) {
    startMatch();
    joiner->request(SkillAction{PlayerId::White, SkillId::Destroy, Pos{7, 7}});
    ASSERT_TRUE(waitUntil([&] { return joiner->viewFor(PlayerId::White).board.at({7, 7}) == Cell::Destroyed; }));
    EXPECT_EQ(host->viewFor(PlayerId::Black).board.at({5, 5}), Cell::Destroyed);
    EXPECT_EQ(*host->viewFor(PlayerId::Black).opponentSkillRevealed, SkillId::Destroy);
}

TEST_F(RemoteSessionTest, U11_W8_LinesClearedAndGameOverReachJoiner) {
    startMatch();
    for (int x = 0; x < 5; ++x) {
        joiner->request(PlaceAction{PlayerId::White, {x, 3}});
    }
    ASSERT_TRUE(waitUntil([&] { return joinLog.cleared.size() == 1 && !joinLog.gameOver.empty(); }));
    EXPECT_EQ(joinLog.cleared[0][0].stones.size(), 5u);
    EXPECT_EQ(joinLog.gameOver, std::vector<GameStatus>{GameStatus::WhiteWon});
    EXPECT_EQ(joinerStatus(), GameStatus::WhiteWon);
    EXPECT_EQ(joiner->viewFor(PlayerId::White).scores[1], 5);
    EXPECT_EQ(hostStatus(), GameStatus::WhiteWon);
}
