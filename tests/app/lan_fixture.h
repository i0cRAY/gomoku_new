#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

#include "../qt_wait.h"
#include "app/host_session.h"
#include "app/remote_session.h"

// 用 localhost 連起一組 HostSession（黑方）與 RemoteSession（白方）的整合測試骨架
inline NetConfig fastLanConfig() {
    NetConfig c;
    c.heartbeatInterval = 100;
    c.timeout = 1000;
    c.timeoutCheckInterval = 20;
    c.leaveGracePeriod = 300;
    return c;
}

// 記錄一個 GameSession 發出的 signal
struct SessionLog {
    void attach(GameSession& s) {
        QObject::connect(&s, &GameSession::opponentReady, [this] { ++opponentReady; });
        QObject::connect(&s, &GameSession::rematchRequested, [this] { ++rematchRequested; });
        QObject::connect(&s, &GameSession::rematchDeclined, [this] { ++rematchDeclined; });
        QObject::connect(&s, &GameSession::opponentLeft, [this] { ++opponentLeft; });
        QObject::connect(&s, &GameSession::connectionLost, [this] { ++connectionLost; });
        QObject::connect(&s, &GameSession::linesCleared, [this](std::vector<ClearedLine> l) { cleared.push_back(l); });
        QObject::connect(&s, &GameSession::actionRejected, [this](PlayerId, RejectReason r, std::optional<Pos> p) {
            rejections.push_back({r, p});
        });
        QObject::connect(&s, &GameSession::gameOver,
                         [this](GameStatus st, std::array<int, 2>, std::optional<std::array<int, 2>>) { gameOver.push_back(st); });
    }

    int opponentReady = 0;
    int rematchRequested = 0;
    int rematchDeclined = 0;
    int opponentLeft = 0;
    int connectionLost = 0;
    std::vector<std::vector<ClearedLine>> cleared;
    std::vector<std::pair<RejectReason, std::optional<Pos>>> rejections;
    std::vector<GameStatus> gameOver;
};

class LanFixture : public ::testing::Test {
protected:
    void SetUp() override {
        config.countdown = 0;       // 不必等 3 秒倒數
        config.placeCooldown = 0;   // 連續下子不必等
        config.startEnergy = 10;
        config.mode = MatchMode::ScoreTarget;
        config.targetScore = 5;     // 連一條五子就結束，方便測結束後的流程
        auto hostNet = std::make_unique<NetworkManager>(fastLanConfig());
        auto joinNet = std::make_unique<NetworkManager>(fastLanConfig());
        ASSERT_TRUE(hostNet->host(config, 0));
        std::optional<MatchConfig> joinedConfig;
        std::optional<PlayerId> joinedColor;
        QObject::connect(joinNet.get(), &NetworkManager::connected, [&](MatchConfig c, PlayerId color) {
            joinedConfig = c;
            joinedColor = color;
        });
        joinNet->join(QHostAddress::LocalHost, hostNet->serverPort());
        ASSERT_TRUE(waitUntil([&] { return hostNet->isConnected() && joinedColor.has_value(); }));
        joinNet->disconnect();
        hostNetwork = hostNet.get();
        joinNetwork = joinNet.get();
        host = std::make_unique<HostSession>(config, std::move(hostNet));
        joiner = std::make_unique<RemoteSession>(*joinedConfig, *joinedColor, std::move(joinNet));
        hostLog.attach(*host);
        joinLog.attach(*joiner);
        ASSERT_TRUE(waitUntil([&] { return joinerStatus() == GameStatus::SkillSelect; }));
    }

    GameStatus hostStatus() const { return host->viewFor(PlayerId::Black).status; }
    GameStatus joinerStatus() const { return joiner->viewFor(PlayerId::White).status; }

    void startMatch() {
        host->selectSkill(PlayerId::Black, SkillId::Dominate);
        host->confirmSkill(PlayerId::Black);
        joiner->selectSkill(PlayerId::White, SkillId::Destroy);
        joiner->confirmSkill(PlayerId::White);
        ASSERT_TRUE(waitUntil([&] { return hostStatus() == GameStatus::Running && joinerStatus() == GameStatus::Running; }));
    }

    MatchConfig config;
    std::unique_ptr<HostSession> host;
    std::unique_ptr<RemoteSession> joiner;
    NetworkManager* hostNetwork = nullptr;
    NetworkManager* joinNetwork = nullptr;
    SessionLog hostLog;
    SessionLog joinLog;
};
