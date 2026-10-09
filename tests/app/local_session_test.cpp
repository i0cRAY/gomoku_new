#include <gtest/gtest.h>

#include <vector>

#include "app/game_clock.h"
#include "app/local_session.h"

// ---- GameClock ----

TEST(GameClockTest, G2_NowStartsFromGivenOrigin) {
    GameClock clock;
    EXPECT_FALSE(clock.isRunning());
    clock.start(-3000);
    EXPECT_TRUE(clock.isRunning());
    const TimeMs now = clock.now();
    EXPECT_GE(now, -3000);
    EXPECT_LT(now, -2000);  // 不依賴精確時間，只確認起點正確
    clock.stop();
    EXPECT_FALSE(clock.isRunning());
}

TEST(GameClockTest, T11_TickIntervalIs50Ms) {
    EXPECT_EQ(GameClock::kTickInterval, 50);
}

// ---- LocalSession ----

class LocalSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        QObject::connect(&session, &GameSession::stateChanged, [this] { ++stateChangedCount; });
        QObject::connect(&session, &GameSession::opponentReady, [this] { ++opponentReadyCount; });
        QObject::connect(&session, &GameSession::actionRejected,
                         [this](PlayerId, RejectReason r, std::optional<Pos>) { rejections.push_back(r); });
    }

    MatchConfig config;
    LocalSession session{config, PlayerId::Black};
    int stateChangedCount = 0;
    int opponentReadyCount = 0;
    std::vector<RejectReason> rejections;
};

TEST_F(LocalSessionTest, G1a_ForwardsSelectionAndState) {
    session.selectSkill(PlayerId::Black, SkillId::Bomb);
    EXPECT_GE(stateChangedCount, 1);
    EXPECT_EQ(session.viewFor(PlayerId::Black).self.skill, SkillId::Bomb);
    EXPECT_EQ(session.viewFor(PlayerId::Black).status, GameStatus::SkillSelect);
}

TEST_F(LocalSessionTest, U7_OpponentReadyOnlyForOpponent) {
    session.selectSkill(PlayerId::Black, SkillId::Bomb);
    session.selectSkill(PlayerId::White, SkillId::Dominate);
    session.confirmSkill(PlayerId::Black);
    EXPECT_EQ(opponentReadyCount, 0);  // 自己確定不算
    session.confirmSkill(PlayerId::White);
    EXPECT_EQ(opponentReadyCount, 1);
}

TEST_F(LocalSessionTest, G2_ClockStartsWhenCountdownBegins) {
    EXPECT_FALSE(session.clock().isRunning());
    session.selectSkill(PlayerId::Black, SkillId::Bomb);
    session.selectSkill(PlayerId::White, SkillId::Dominate);
    session.confirmSkill(PlayerId::Black);
    EXPECT_FALSE(session.clock().isRunning());
    session.confirmSkill(PlayerId::White);
    EXPECT_TRUE(session.clock().isRunning());
    EXPECT_LT(session.clock().now(), 0);  // 從 −3000 起算
    EXPECT_EQ(session.viewFor(PlayerId::Black).status, GameStatus::Countdown);
}

TEST_F(LocalSessionTest, G3_RequestDuringCountdownIsRejected) {
    session.selectSkill(PlayerId::Black, SkillId::Bomb);
    session.selectSkill(PlayerId::White, SkillId::Dominate);
    session.confirmSkill(PlayerId::Black);
    session.confirmSkill(PlayerId::White);
    session.request(PlaceAction{PlayerId::Black, {7, 7}});
    ASSERT_EQ(rejections.size(), 1u);
    EXPECT_EQ(rejections[0], RejectReason::GameNotRunning);
}

TEST_F(LocalSessionTest, G4a_RematchDuringCountdownGoesBackToSkillSelect) {
    session.selectSkill(PlayerId::Black, SkillId::Bomb);
    session.selectSkill(PlayerId::White, SkillId::Dominate);
    session.confirmSkill(PlayerId::Black);
    session.confirmSkill(PlayerId::White);
    session.requestRematch();
    EXPECT_EQ(session.viewFor(PlayerId::Black).status, GameStatus::SkillSelect);
    EXPECT_FALSE(session.clock().isRunning());

    session.confirmSkill(PlayerId::Black);  // 預設上一局的技能，直接確定就能再開始
    session.confirmSkill(PlayerId::White);
    EXPECT_EQ(session.viewFor(PlayerId::Black).status, GameStatus::Countdown);
    EXPECT_TRUE(session.clock().isRunning());
}

TEST_F(LocalSessionTest, G4a_LeaveAbortsAndStopsClock) {
    session.selectSkill(PlayerId::Black, SkillId::Bomb);
    session.selectSkill(PlayerId::White, SkillId::Dominate);
    session.confirmSkill(PlayerId::Black);
    session.confirmSkill(PlayerId::White);
    session.leave();
    EXPECT_EQ(session.viewFor(PlayerId::Black).status, GameStatus::Aborted);
    EXPECT_FALSE(session.clock().isRunning());
}

TEST_F(LocalSessionTest, N9_AnswerRematchIsNoOpLocally) {
    session.answerRematch(true);
    EXPECT_EQ(session.viewFor(PlayerId::Black).status, GameStatus::SkillSelect);
}
