#include "lan_fixture.h"

// T22：心跳、斷線、離開與再來一局（localhost 整合測試）

class LanMatchTest : public LanFixture {
protected:
    void finishByJoinerLine() {
        for (int x = 0; x < 5; ++x) {
            joiner->request(PlaceAction{PlayerId::White, {x, 3}});
        }
        ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::WhiteWon && joinerStatus() == GameStatus::WhiteWon; }));
    }
};

TEST_F(LanMatchTest, N6_W5_ConnectionLossAbortsBothWithin5s) {
    startMatch();
    joinNetwork->simulateConnectionLoss();
    QElapsedTimer timer;
    timer.start();
    ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::Aborted && joinerStatus() == GameStatus::Aborted; },
                          5000));
    EXPECT_LT(timer.elapsed(), 5000);
    EXPECT_EQ(hostLog.connectionLost, 1);
    EXPECT_EQ(joinLog.connectionLost, 1);
    EXPECT_EQ(hostLog.opponentLeft, 0);
    EXPECT_EQ(joinLog.opponentLeft, 0);
}

TEST_F(LanMatchTest, N6_HeartbeatTimeoutAbortsBoth) {
    startMatch();
    joinNetwork->stopHeartbeat();  // 加入方的程式卡住，不再送任何訊息
    QElapsedTimer timer;
    timer.start();
    ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::Aborted && joinerStatus() == GameStatus::Aborted; },
                          5000));
    EXPECT_GE(timer.elapsed(), fastLanConfig().timeout - 100);  // 主機等到逾時才判定
    EXPECT_EQ(hostLog.connectionLost, 1);
    EXPECT_EQ(joinLog.connectionLost, 1);
}

TEST_F(LanMatchTest, N8_JoinerLeavesDuringMatch) {
    startMatch();
    joiner->leave();
    ASSERT_TRUE(waitUntil([&] { return hostLog.opponentLeft == 1; }));
    EXPECT_EQ(hostStatus(), GameStatus::Aborted);  // W5：不計勝負
    processEventsFor(2 * fastLanConfig().timeout);
    EXPECT_EQ(hostLog.connectionLost, 0);  // 是「對手已離開」，不是「連線中斷」
    EXPECT_EQ(joinLog.connectionLost, 0);
}

TEST_F(LanMatchTest, N8_HostLeavesDuringMatch) {
    startMatch();
    host->leave();
    ASSERT_TRUE(waitUntil([&] { return joinLog.opponentLeft == 1; }));
    EXPECT_EQ(joinerStatus(), GameStatus::Aborted);
    processEventsFor(2 * fastLanConfig().timeout);
    EXPECT_EQ(joinLog.connectionLost, 0);
}

TEST_F(LanMatchTest, G4b_LeaveDuringSkillSelect) {
    joiner->selectSkill(PlayerId::White, SkillId::Destroy);
    host->leave();
    ASSERT_TRUE(waitUntil([&] { return joinLog.opponentLeft == 1; }));
    EXPECT_EQ(joinerStatus(), GameStatus::Aborted);
}

TEST_F(LanMatchTest, G4_LeaveAfterGameOverKeepsResult) {
    startMatch();
    finishByJoinerLine();
    joiner->leave();
    ASSERT_TRUE(waitUntil([&] { return hostLog.opponentLeft == 1; }));
    EXPECT_EQ(hostStatus(), GameStatus::WhiteWon);  // 已經結束的對局不改成中止
}

TEST_F(LanMatchTest, N9_RematchAccepted) {
    startMatch();
    host->request(PlaceAction{PlayerId::Black, {7, 7}});
    joiner->requestRematch();
    ASSERT_TRUE(waitUntil([&] { return hostLog.rematchRequested == 1; }));
    EXPECT_EQ(hostStatus(), GameStatus::Running);  // 等待回覆期間對局照常進行
    host->answerRematch(true);
    ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::SkillSelect && joinerStatus() == GameStatus::SkillSelect; }));
    EXPECT_TRUE(joiner->viewFor(PlayerId::White).board.isEmpty({7, 7}));
    EXPECT_EQ(joinLog.rematchDeclined, 0);
}

TEST_F(LanMatchTest, N9_RematchAcceptedByJoiner) {
    startMatch();
    host->requestRematch();
    ASSERT_TRUE(waitUntil([&] { return joinLog.rematchRequested == 1; }));
    joiner->answerRematch(true);
    ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::SkillSelect && joinerStatus() == GameStatus::SkillSelect; }));
}

TEST_F(LanMatchTest, N9_RematchDeclinedMatchContinues) {
    startMatch();
    host->requestRematch();
    ASSERT_TRUE(waitUntil([&] { return joinLog.rematchRequested == 1; }));
    joiner->answerRematch(false);
    ASSERT_TRUE(waitUntil([&] { return hostLog.rematchDeclined == 1; }));
    EXPECT_EQ(hostStatus(), GameStatus::Running);
    EXPECT_EQ(joinerStatus(), GameStatus::Running);
    // 拒絕後可以再邀請
    host->requestRematch();
    ASSERT_TRUE(waitUntil([&] { return joinLog.rematchRequested == 2; }));
}

TEST_F(LanMatchTest, N9_OnlyOnePendingInvite) {
    startMatch();
    host->requestRematch();
    host->requestRematch();
    ASSERT_TRUE(waitUntil([&] { return joinLog.rematchRequested == 1; }));
    processEventsFor(100);
    EXPECT_EQ(joinLog.rematchRequested, 1);
}

TEST_F(LanMatchTest, N9_CrossingInvitesRestart) {
    startMatch();
    host->requestRematch();
    joiner->requestRematch();
    ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::SkillSelect && joinerStatus() == GameStatus::SkillSelect; }));
    EXPECT_EQ(hostLog.rematchDeclined + joinLog.rematchDeclined, 0);
}

TEST_F(LanMatchTest, G4_RematchAfterGameNeedsBoth) {
    startMatch();
    finishByJoinerLine();
    host->requestRematch();
    processEventsFor(200);
    EXPECT_EQ(hostStatus(), GameStatus::WhiteWon);  // 只有一方按，還不重開
    EXPECT_EQ(joinLog.rematchRequested, 0);         // 結束後不是 N9 的邀請
    joiner->requestRematch();
    ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::SkillSelect && joinerStatus() == GameStatus::SkillSelect; }));
    // 新的一局照常進行
    startMatch();
    joiner->request(PlaceAction{PlayerId::White, {1, 1}});
    ASSERT_TRUE(waitUntil([&] { return host->viewFor(PlayerId::Black).board.at({1, 1}) == Cell::White; }));
}

TEST_F(LanMatchTest, N9_RematchRequestIgnoredDuringSkillSelect) {
    joiner->requestRematch();
    processEventsFor(100);
    EXPECT_EQ(hostLog.rematchRequested, 0);
}
