#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "../qt_wait.h"
#include "app/host_session.h"

namespace {

NetConfig fastNetConfig() {
    NetConfig c;
    c.heartbeatInterval = 100;
    c.timeout = 1000;
    c.timeoutCheckInterval = 20;
    c.leaveGracePeriod = 300;
    return c;
}

// 主機端用真的 HostSession，加入方用裸的 NetworkManager 直接收發訊息
class HostSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.countdown = 0;       // 不必等 3 秒倒數
        config.placeCooldown = 0;   // 連續下子不必等
        config.startEnergy = 10;
        config.maxEnergy = 10;
        auto hostNet = std::make_unique<NetworkManager>(fastNetConfig());
        ASSERT_TRUE(hostNet->host(config, 0));
        bool hostReady = false;
        QObject::connect(hostNet.get(), &NetworkManager::connected, [&] { hostReady = true; });
        joiner.join(QHostAddress::LocalHost, hostNet->serverPort());
        QObject::connect(&joiner, &NetworkManager::messageReceived, [this](const Message& m) { received.push_back(m); });
        ASSERT_TRUE(waitUntil([&] { return hostReady && joiner.isConnected(); }));
        hostNet->disconnect();
        session = std::make_unique<HostSession>(config, std::move(hostNet));
        QObject::connect(session.get(), &GameSession::opponentReady, [this] { ++opponentReadyCount; });
    }

    template <typename T>
    std::vector<T> receivedOf() const {
        std::vector<T> result;
        for (const Message& m : received) {
            if (const auto* t = std::get_if<T>(&m)) {
                result.push_back(*t);
            }
        }
        return result;
    }

    std::optional<PlayerView> lastSnapshot() const {
        const auto snapshots = receivedOf<SnapshotMessage>();
        if (snapshots.empty()) {
            return std::nullopt;
        }
        return snapshots.back().view;
    }

    void startMatch() {
        session->selectSkill(PlayerId::Black, SkillId::Dominate);
        session->confirmSkill(PlayerId::Black);
        joiner.send(SelectSkillMessage{SkillId::Destroy});
        joiner.send(ConfirmSkillMessage{});
        ASSERT_TRUE(waitUntil([&] { return session->viewFor(PlayerId::Black).status != GameStatus::SkillSelect; }));
        ASSERT_TRUE(waitUntil([&] {
            const auto view = lastSnapshot();
            return view && view->status == GameStatus::Running;
        }));
    }

    MatchConfig config;
    NetworkManager joiner{fastNetConfig()};
    std::unique_ptr<HostSession> session;
    std::vector<Message> received;
    int opponentReadyCount = 0;
};

}  // namespace

TEST_F(HostSessionTest, N4_SendsSnapshotOfJoinerOnConnect) {
    ASSERT_TRUE(waitUntil([&] { return lastSnapshot().has_value(); }));
    EXPECT_EQ(lastSnapshot()->me, PlayerId::White);
    EXPECT_EQ(lastSnapshot()->status, GameStatus::SkillSelect);
}

TEST_F(HostSessionTest, G1a_RemoteSkillSelectionReachesController) {
    joiner.send(SelectSkillMessage{SkillId::Destroy});
    ASSERT_TRUE(waitUntil([&] { return lastSnapshot() && lastSnapshot()->self.skill == SkillId::Destroy; }));
    joiner.send(ConfirmSkillMessage{});
    ASSERT_TRUE(waitUntil([&] { return opponentReadyCount == 1; }));
    EXPECT_EQ(session->viewFor(PlayerId::Black).status, GameStatus::SkillSelect);
    EXPECT_TRUE(receivedOf<OpponentReadyMessage>().empty());  // 主機還沒確定

    session->selectSkill(PlayerId::Black, SkillId::Dominate);
    session->confirmSkill(PlayerId::Black);
    EXPECT_NE(session->viewFor(PlayerId::Black).status, GameStatus::SkillSelect);  // G2：雙方確定後開始倒數
    ASSERT_TRUE(waitUntil([&] { return receivedOf<OpponentReadyMessage>().size() == 1; }));
    // S1：主機選的技能不會出現在加入方的快照裡
    EXPECT_FALSE(lastSnapshot()->opponentSkillRevealed.has_value());
    EXPECT_EQ(lastSnapshot()->self.skill, SkillId::Destroy);
}

TEST_F(HostSessionTest, N2_RemoteRequestGoesThroughController) {
    startMatch();
    joiner.send(RequestMessage{7, PlaceAction{PlayerId::White, {3, 4}}});
    ASSERT_TRUE(waitUntil([&] { return session->viewFor(PlayerId::Black).board.at({3, 4}) == Cell::White; }));
    ASSERT_TRUE(waitUntil([&] { return lastSnapshot()->board.at({3, 4}) == Cell::White; }));
    EXPECT_EQ(lastSnapshot()->self.energy, config.startEnergy - 1);
    EXPECT_TRUE(receivedOf<RejectMessage>().empty());
}

TEST_F(HostSessionTest, N2_JoinerCanOnlyActAsWhite) {
    startMatch();
    joiner.send(RequestMessage{1, PlaceAction{PlayerId::Black, {5, 5}}});
    ASSERT_TRUE(waitUntil([&] { return !session->viewFor(PlayerId::Black).board.isEmpty({5, 5}); }));
    EXPECT_EQ(session->viewFor(PlayerId::Black).board.at({5, 5}), Cell::White);
    EXPECT_EQ(session->viewFor(PlayerId::Black).self.energy, config.startEnergy);  // 主機的能量沒被扣
}

TEST_F(HostSessionTest, N2_RejectCarriesSeq) {
    startMatch();
    session->request(PlaceAction{PlayerId::Black, {6, 6}});
    joiner.send(RequestMessage{42, PlaceAction{PlayerId::White, {6, 6}}});  // P5：主機先下的成功
    ASSERT_TRUE(waitUntil([&] { return receivedOf<RejectMessage>().size() == 1; }));
    EXPECT_EQ(receivedOf<RejectMessage>()[0].seq, 42u);
    EXPECT_EQ(receivedOf<RejectMessage>()[0].reason, RejectReason::Occupied);
}

TEST_F(HostSessionTest, N2_HostOwnRejectionIsNotSent) {
    startMatch();
    session->request(PlaceAction{PlayerId::Black, {20, 20}});
    processEventsFor(100);
    EXPECT_TRUE(receivedOf<RejectMessage>().empty());
}

TEST_F(HostSessionTest, N4_SnapshotHidesHostPrivateState) {
    startMatch();
    session->request(PlaceAction{PlayerId::Black, {0, 0}});
    session->request(PlaceAction{PlayerId::Black, {1, 0}});
    ASSERT_TRUE(waitUntil([&] { return lastSnapshot()->board.at({1, 0}) == Cell::Black; }));
    EXPECT_EQ(lastSnapshot()->me, PlayerId::White);
    EXPECT_EQ(lastSnapshot()->self.energy, config.startEnergy);  // 是加入方自己的能量，不是主機的
}

TEST_F(HostSessionTest, N4_PeriodicSnapshotsEvery200Ms) {
    startMatch();
    processEventsFor(50);
    const std::size_t before = receivedOf<SnapshotMessage>().size();
    processEventsFor(1000);
    const std::size_t count = receivedOf<SnapshotMessage>().size() - before;
    EXPECT_GE(count, 3u);
    EXPECT_LE(count, 7u);  // 不是每 50 ms 的 tick 都送
}

TEST_F(HostSessionTest, U11_LinesClearedSentToJoiner) {
    startMatch();
    for (int x = 0; x < 5; ++x) {
        joiner.send(RequestMessage{static_cast<std::uint32_t>(x), PlaceAction{PlayerId::White, {x, 2}}});
    }
    ASSERT_TRUE(waitUntil([&] { return receivedOf<LinesClearedMessage>().size() == 1; }));
    const auto lines = receivedOf<LinesClearedMessage>()[0].lines;
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].owner, PlayerId::White);
    EXPECT_EQ(lines[0].stones.size(), 5u);
    ASSERT_TRUE(waitUntil([&] { return lastSnapshot()->scores[1] == 5; }));
}

TEST_F(HostSessionTest, N4_StatusChangeInTickIsSentImmediately) {
    session->selectSkill(PlayerId::Black, SkillId::Dominate);
    session->confirmSkill(PlayerId::Black);
    joiner.send(SelectSkillMessage{SkillId::Destroy});
    joiner.send(ConfirmSkillMessage{});
    // 倒數為 0：第一次 tick 就進入對局，不必等定時快照
    ASSERT_TRUE(waitUntil([&] { return lastSnapshot() && lastSnapshot()->status == GameStatus::Running; }, 150));
}
