#pragma once

#include <QTimer>

#include <memory>

#include "app/local_session.h"
#include "net/network_manager.h"

// 區網主機端（spec M2、N1、N2、N4、N7、N8、N9）：本機玩家為黑方，規則照 LocalSession 在本機執行，
// 加入方（白方）的操作從網路進同一個 GameController::submit（N2）。
// 每次狀態改變與對局中每 200 ms 送出 viewFor(加入方) 的快照（N4、E5）。
class HostSession : public LocalSession {
    Q_OBJECT
public:
    static constexpr PlayerId kHostColor = PlayerId::Black;
    static constexpr PlayerId kRemoteColor = PlayerId::White;

    // network 必須已經握手完成（NetworkManager::connected 之後）
    HostSession(MatchConfig, std::unique_ptr<NetworkManager> network, QObject* parent = nullptr);

    void requestRematch() override;     // 結束後：雙方都按才重開（G4）；對局中：送出邀請（N9）
    void answerRematch(bool) override;  // N9
    void leave() override;              // N8：送出 leave 後中止本局

protected:
    void onTick(TimeMs now) override;

private:
    void onMessage(const Message&);
    void onConnectionEnded(bool opponentLeft);
    void sendSnapshot();
    void restartMatch();
    void clearRematchFlags();
    GameStatus status() const { return controller.viewFor(kHostColor).status; }

    std::unique_ptr<NetworkManager> network;
    QTimer snapshotTimer;
    bool inTick = false;
    bool localRematch = false;    // G4：結束後本機已按「再來一局」
    bool remoteRematch = false;   // G4：結束後對手已按「再來一局」
    bool outgoingInvite = false;  // N9：本機送出、尚未回覆的邀請
    bool incomingInvite = false;  // N9：對手送來、尚未回覆的邀請
};
