#include <gtest/gtest.h>

#include <QTcpServer>
#include <QTcpSocket>

#include <memory>
#include <optional>
#include <vector>

#include "../qt_wait.h"
#include "net/network_manager.h"

namespace {

// 縮短心跳與逾時，讓測試跑得快
NetConfig fastConfig() {
    NetConfig c;
    c.heartbeatInterval = 100;
    c.timeout = 500;
    c.timeoutCheckInterval = 20;
    c.leaveGracePeriod = 300;
    return c;
}

// 記錄一個 NetworkManager 發出的所有 signal
struct Recorder {
    explicit Recorder(NetworkManager& net) {
        QObject::connect(&net, &NetworkManager::connected, [this](MatchConfig c, PlayerId color) {
            config = c;
            this->color = color;
        });
        QObject::connect(&net, &NetworkManager::versionMismatch, [this](int v) { mismatch = v; });
        QObject::connect(&net, &NetworkManager::messageReceived, [this](const Message& m) { messages.push_back(m); });
        QObject::connect(&net, &NetworkManager::opponentLeft, [this] { ++leftCount; });
        QObject::connect(&net, &NetworkManager::disconnected, [this] { ++disconnectedCount; });
    }

    std::optional<MatchConfig> config;
    std::optional<PlayerId> color;
    std::optional<int> mismatch;
    std::vector<Message> messages;
    int leftCount = 0;
    int disconnectedCount = 0;
};

class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        match.regenInterval = 3500;
        match.mode = MatchMode::ScoreTarget;
        match.targetScore = 30;
        ASSERT_TRUE(host.host(match, 0));
    }

    void connectJoiner() {
        joiner.join(QHostAddress::LocalHost, host.serverPort());
        ASSERT_TRUE(waitUntil([&] { return hostLog.color && joinerLog.color; }));
    }

    MatchConfig match;
    NetworkManager host{fastConfig()};
    NetworkManager joiner{fastConfig()};
    Recorder hostLog{host};
    Recorder joinerLog{joiner};
};

// 用一般 socket 假裝成加入方，送出 hello 後就不再說話
std::unique_ptr<QTcpSocket> rawJoin(quint16 port, int version) {
    auto socket = std::make_unique<QTcpSocket>();
    socket->connectToHost(QHostAddress::LocalHost, port);
    EXPECT_TRUE(waitUntil([&] { return socket->state() == QAbstractSocket::ConnectedState; }));
    socket->write(encodeMessage(HelloMessage{version}));
    return socket;
}

}  // namespace

TEST(NetConfigTest, N1_N4_N6_DefaultValues) {
    EXPECT_EQ(kDefaultPort, 50505);
    const NetConfig c;
    EXPECT_EQ(c.heartbeatInterval, 1000);
    EXPECT_EQ(c.timeout, 5000);
    EXPECT_EQ(c.snapshotInterval, 200);
}

TEST_F(NetworkManagerTest, N7_JoinerReceivesHostConfig) {
    connectJoiner();
    EXPECT_EQ(*hostLog.color, PlayerId::Black);
    EXPECT_EQ(*joinerLog.color, PlayerId::White);
    EXPECT_EQ(joinerLog.config->regenInterval, 3500);
    EXPECT_EQ(joinerLog.config->mode, MatchMode::ScoreTarget);
    EXPECT_EQ(joinerLog.config->targetScore, 30);
    EXPECT_TRUE(host.isConnected());
    EXPECT_TRUE(joiner.isConnected());
}

TEST_F(NetworkManagerTest, N1_OnlyOneOpponent) {
    connectJoiner();
    NetworkManager third{fastConfig()};
    Recorder thirdLog{third};
    third.join(QHostAddress::LocalHost, host.serverPort());
    EXPECT_TRUE(waitUntil([&] { return thirdLog.disconnectedCount > 0; }));
    EXPECT_FALSE(thirdLog.color.has_value());
    EXPECT_TRUE(host.isConnected());
    EXPECT_EQ(hostLog.disconnectedCount, 0);
}

TEST_F(NetworkManagerTest, N1_SecondConnectionBeforeHandshakeIsClosed) {
    auto first = std::make_unique<QTcpSocket>();
    first->connectToHost(QHostAddress::LocalHost, host.serverPort());
    ASSERT_TRUE(waitUntil([&] { return first->state() == QAbstractSocket::ConnectedState; }));
    processEventsFor(50);
    QTcpSocket second;
    second.connectToHost(QHostAddress::LocalHost, host.serverPort());
    EXPECT_TRUE(waitUntil([&] { return second.state() == QAbstractSocket::UnconnectedState; }));
}

TEST_F(NetworkManagerTest, N5_HostRejectsDifferentVersion) {
    auto socket = rawJoin(host.serverPort(), kProtocolVersion + 1);
    // 主機回 welcome 告知自己的版本，然後關閉連線
    ASSERT_TRUE(waitUntil([&] { return socket->canReadLine(); }));
    const auto reply = decodeMessage(socket->readLine());
    ASSERT_TRUE(reply && std::holds_alternative<WelcomeMessage>(*reply));
    EXPECT_EQ(std::get<WelcomeMessage>(*reply).version, kProtocolVersion);
    EXPECT_TRUE(waitUntil([&] { return socket->state() == QAbstractSocket::UnconnectedState; }));
    EXPECT_FALSE(hostLog.color.has_value());
    EXPECT_FALSE(host.isConnected());

    // 版本相同的加入方之後還是可以連進來
    connectJoiner();
}

TEST(NetworkManagerJoinTest, N5_JoinerReportsVersionMismatch) {
    QTcpServer fakeHost;
    ASSERT_TRUE(fakeHost.listen(QHostAddress::LocalHost, 0));
    NetworkManager joiner{fastConfig()};
    Recorder log{joiner};
    joiner.join(QHostAddress::LocalHost, fakeHost.serverPort());
    ASSERT_TRUE(waitUntil([&] { return fakeHost.hasPendingConnections(); }));
    QTcpSocket* peer = fakeHost.nextPendingConnection();
    ASSERT_TRUE(waitUntil([&] { return peer->canReadLine(); }));
    const auto hello = decodeMessage(peer->readLine());
    ASSERT_TRUE(hello && std::holds_alternative<HelloMessage>(*hello));
    EXPECT_EQ(std::get<HelloMessage>(*hello).version, kProtocolVersion);

    peer->write(encodeMessage(WelcomeMessage{kProtocolVersion + 1, MatchConfig{}, PlayerId::White}));
    ASSERT_TRUE(waitUntil([&] { return log.mismatch.has_value(); }));
    EXPECT_EQ(*log.mismatch, kProtocolVersion + 1);
    EXPECT_FALSE(log.color.has_value());
    EXPECT_FALSE(joiner.isConnected());
    processEventsFor(100);
    EXPECT_EQ(log.disconnectedCount, 0);  // 版本不同只顯示版本提示
}

TEST(NetworkManagerJoinTest, N6_JoinerReportsUnreachableHost) {
    QTcpServer probe;
    ASSERT_TRUE(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 port = probe.serverPort();
    probe.close();  // 沒有人在聽的埠
    NetworkManager joiner{fastConfig()};
    Recorder log{joiner};
    joiner.join(QHostAddress::LocalHost, port);
    EXPECT_TRUE(waitUntil([&] { return log.disconnectedCount == 1; }));
}

TEST_F(NetworkManagerTest, N4_MessagesFlowBothWays) {
    connectJoiner();
    joiner.send(SelectSkillMessage{SkillId::Destroy});
    host.send(OpponentReadyMessage{});
    ASSERT_TRUE(waitUntil([&] { return hostLog.messages.size() == 1 && joinerLog.messages.size() == 1; }));
    ASSERT_TRUE(std::holds_alternative<SelectSkillMessage>(hostLog.messages[0]));
    EXPECT_EQ(std::get<SelectSkillMessage>(hostLog.messages[0]).skill, SkillId::Destroy);
    EXPECT_TRUE(std::holds_alternative<OpponentReadyMessage>(joinerLog.messages[0]));
}

TEST_F(NetworkManagerTest, N6_HeartbeatKeepsConnectionAlive) {
    connectJoiner();
    processEventsFor(3 * fastConfig().timeout);  // 沒有遊戲訊息，只有心跳
    EXPECT_TRUE(host.isConnected());
    EXPECT_TRUE(joiner.isConnected());
    EXPECT_EQ(hostLog.disconnectedCount, 0);
    EXPECT_EQ(joinerLog.disconnectedCount, 0);
    EXPECT_TRUE(hostLog.messages.empty());  // ping 不會往上傳
}

TEST_F(NetworkManagerTest, N6_SilentPeerTimesOut) {
    auto socket = rawJoin(host.serverPort(), kProtocolVersion);
    ASSERT_TRUE(waitUntil([&] { return hostLog.color.has_value(); }));
    QElapsedTimer timer;
    timer.start();
    ASSERT_TRUE(waitUntil([&] { return hostLog.disconnectedCount == 1; }, 3000));
    EXPECT_GE(timer.elapsed(), fastConfig().timeout - 100);
    EXPECT_FALSE(host.isConnected());
}

TEST_F(NetworkManagerTest, N6_SocketLossDetectedByBothSides) {
    connectJoiner();
    joiner.simulateConnectionLoss();
    EXPECT_EQ(joinerLog.disconnectedCount, 1);
    EXPECT_TRUE(waitUntil([&] { return hostLog.disconnectedCount == 1; }));
    EXPECT_EQ(hostLog.leftCount, 0);
}

TEST_F(NetworkManagerTest, N8_LeaveIsNotDisconnect) {
    connectJoiner();
    joiner.send(SelectSkillMessage{SkillId::Dominate});
    joiner.sendLeave();
    EXPECT_FALSE(joiner.isConnected());
    ASSERT_TRUE(waitUntil([&] { return hostLog.leftCount == 1; }));
    EXPECT_EQ(hostLog.messages.size(), 1u);  // leave 前送的訊息照樣收到
    processEventsFor(2 * fastConfig().timeout);
    EXPECT_EQ(hostLog.disconnectedCount, 0);
    EXPECT_EQ(joinerLog.disconnectedCount, 0);
    EXPECT_EQ(joinerLog.leftCount, 0);
}

TEST_F(NetworkManagerTest, N4_MalformedLineIsIgnored) {
    auto socket = rawJoin(host.serverPort(), kProtocolVersion);
    ASSERT_TRUE(waitUntil([&] { return hostLog.color.has_value(); }));
    socket->write("{not json\n");
    socket->write("{\"type\":\"unknown\"}\n");
    socket->write(encodeMessage(ConfirmSkillMessage{}));
    ASSERT_TRUE(waitUntil([&] { return hostLog.messages.size() == 1; }));
    EXPECT_TRUE(std::holds_alternative<ConfirmSkillMessage>(hostLog.messages[0]));
    EXPECT_TRUE(host.isConnected());
}
