#include "app/host_session.h"

#include <utility>
#include <variant>

namespace {

bool isInMatch(GameStatus status) {
    return status == GameStatus::Countdown || status == GameStatus::Running;
}

bool isOver(GameStatus status) {
    return status == GameStatus::BlackWon || status == GameStatus::WhiteWon || status == GameStatus::Draw;
}

}  // namespace

HostSession::HostSession(MatchConfig config, std::unique_ptr<NetworkManager> network, QObject* parent)
    : LocalSession(config, kHostColor, parent), network(std::move(network)) {
    this->network->setParent(nullptr);  // 由 unique_ptr 管理

    connect(this->network.get(), &NetworkManager::messageReceived, this, &HostSession::onMessage);
    connect(this->network.get(), &NetworkManager::opponentLeft, this, [this] { onConnectionEnded(true); });
    connect(this->network.get(), &NetworkManager::disconnected, this, [this] { onConnectionEnded(false); });

    // N4：每次狀態改變就送；tick 造成的變化交給定時快照，除非對局狀態改變
    connect(&controller, &GameController::stateChanged, this, [this] {
        if (!inTick) {
            sendSnapshot();
        }
    });
    connect(&controller, &GameController::linesCleared, this, [this](std::vector<ClearedLine> lines) {
        this->network->send(LinesClearedMessage{std::move(lines)});  // U11
    });
    connect(&controller, &GameController::skillConfirmed, this, [this](PlayerId player) {
        if (player == kHostColor) {
            this->network->send(OpponentReadyMessage{});  // S1：只告知已確定，不透露選了什麼
        }
    });
    connect(&controller, &GameController::gameOver, this, [this] { clearRematchFlags(); });

    snapshotTimer.setInterval(this->network->netConfig().snapshotInterval);
    connect(&snapshotTimer, &QTimer::timeout, this, [this] {
        if (isInMatch(status())) {
            sendSnapshot();
        }
    });
    snapshotTimer.start();
    sendSnapshot();  // 加入方一連上就看到技能選擇畫面
}

void HostSession::onTick(TimeMs now) {
    const GameStatus before = status();
    inTick = true;
    LocalSession::onTick(now);
    inTick = false;
    if (status() != before) {
        sendSnapshot();  // 倒數結束、時間到等狀態改變要馬上送
    }
}

void HostSession::sendSnapshot() {
    network->send(SnapshotMessage{controller.viewFor(kRemoteColor)});  // N4、E5
}

void HostSession::onMessage(const Message& message) {
    if (const auto* m = std::get_if<SelectSkillMessage>(&message)) {
        selectSkill(kRemoteColor, m->skill);
    } else if (std::holds_alternative<ConfirmSkillMessage>(message)) {
        confirmSkill(kRemoteColor);
    } else if (const auto* m = std::get_if<RequestMessage>(&message)) {
        // N2：加入方只能操作白方，不管封包裡寫的是誰
        Action action = m->action;
        std::visit([](auto& a) { a.player = kRemoteColor; }, action);
        const ActionResult result = controller.submit(action, gameClock.now());
        if (!result.accepted && result.reason) {
            network->send(RejectMessage{m->seq, *result.reason});
        }
    } else if (std::holds_alternative<RematchMessage>(message)) {
        if (isOver(status())) {
            remoteRematch = true;  // G4：雙方都按才重開
            if (localRematch) {
                restartMatch();
            }
        }
    } else if (std::holds_alternative<RematchRequestMessage>(message)) {
        if (!isInMatch(status()) || incomingInvite) {
            return;
        }
        if (outgoingInvite) {
            restartMatch();  // N9：雙方同時邀請，等於彼此同意
            return;
        }
        incomingInvite = true;
        emit rematchRequested();
    } else if (const auto* m = std::get_if<RematchReplyMessage>(&message)) {
        if (!outgoingInvite) {
            return;
        }
        outgoingInvite = false;
        if (m->accept) {
            restartMatch();
        } else {
            emit rematchDeclined();
        }
    }
}

void HostSession::requestRematch() {
    const GameStatus current = status();
    if (isOver(current)) {
        localRematch = true;
        network->send(RematchMessage{});
        if (remoteRematch) {
            restartMatch();
        }
    } else if (isInMatch(current) && !outgoingInvite && !incomingInvite) {
        outgoingInvite = true;  // N9：同一時間只有一個邀請
        network->send(RematchRequestMessage{});
    }
}

void HostSession::answerRematch(bool accept) {
    if (!incomingInvite) {
        return;
    }
    incomingInvite = false;
    network->send(RematchReplyMessage{accept});
    if (accept && isInMatch(status())) {
        restartMatch();
    }
}

void HostSession::leave() {
    network->sendLeave();  // N8
    LocalSession::leave();
}

void HostSession::onConnectionEnded(bool opponentLeft) {
    snapshotTimer.stop();
    LocalSession::leave();  // W5：中止本局，不計勝負
    clearRematchFlags();
    if (opponentLeft) {
        emit this->opponentLeft();  // N8
    } else {
        emit connectionLost();  // N6
    }
}

void HostSession::restartMatch() {
    clearRematchFlags();
    LocalSession::requestRematch();  // G4：回到技能選擇，快照隨 stateChanged 送出
}

void HostSession::clearRematchFlags() {
    localRematch = false;
    remoteRematch = false;
    outgoingInvite = false;
    incomingInvite = false;
}
