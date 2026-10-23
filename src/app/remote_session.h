#pragma once

#include <QElapsedTimer>
#include <QTimer>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#include "app/game_session.h"
#include "core/config.h"
#include "net/network_manager.h"

// 區網加入方（spec N3、design §4.6）：不執行任何規則，操作轉成網路訊息送給主機，
// 只保存最近一次快照的 PlayerView；viewFor 只能取自己的顏色（不支援 M1a）。
class RemoteSession : public GameSession {
    Q_OBJECT
public:
    static constexpr std::size_t kMaxPendingRequests = 64;  // 記住多少個還沒被拒絕的請求位置（P6 提示用）

    // network 必須已經握手完成；config 與 me 來自 welcome（N7）
    RemoteSession(MatchConfig, PlayerId me, std::unique_ptr<NetworkManager> network, QObject* parent = nullptr);

    void selectSkill(PlayerId, SkillId) override;
    void confirmSkill(PlayerId) override;
    void request(const Action&) override;  // N3：只送出，等主機的快照或 reject
    PlayerView viewFor(PlayerId) const override;
    void requestRematch() override;     // 結束後：送出 rematch（G4）；對局中：送出邀請（N9）
    void answerRematch(bool) override;  // N9
    void leave() override;              // N8

    // design §4.9：兩次快照之間依本機經過的時間往前推，讓能量條、倒數與禁區連續變化。
    // 只推時間，不推測任何規則結果（能量格數、對局狀態都等主機的快照）
    static PlayerView extrapolate(const PlayerView&, TimeMs elapsed, const MatchConfig&);

private:
    void onMessage(const Message&);
    void onSnapshot(const PlayerView&);
    void onConnectionEnded(bool opponentLeft);

    MatchConfig config;
    PlayerId me;
    std::unique_ptr<NetworkManager> network;
    PlayerView latest;
    QElapsedTimer sinceSnapshot;
    QTimer refreshTimer;  // 對局中定時發 stateChanged，讓畫面跟著 extrapolate 更新
    std::uint32_t nextSeq = 1;
    std::map<std::uint32_t, std::optional<Pos>> pendingRequests;  // seq → 下子或技能的目標格
    bool outgoingInvite = false;  // N9
    bool incomingInvite = false;  // N9
};
