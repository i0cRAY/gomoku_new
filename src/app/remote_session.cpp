#include "app/remote_session.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "app/game_clock.h"

namespace {

bool isInMatch(GameStatus status) {
    return status == GameStatus::Countdown || status == GameStatus::Running;
}

bool isOver(GameStatus status) {
    return status == GameStatus::BlackWon || status == GameStatus::WhiteWon || status == GameStatus::Draw ||
           status == GameStatus::Aborted;
}

std::optional<Pos> targetOf(const Action& action) {
    if (const auto* place = std::get_if<PlaceAction>(&action)) {
        return place->pos;
    }
    return std::get<SkillAction>(action).target;
}

}  // namespace

RemoteSession::RemoteSession(MatchConfig config, PlayerId me, std::unique_ptr<NetworkManager> network,
                             QObject* parent)
    : GameSession(parent), config(config), me(me), network(std::move(network)) {
    this->network->setParent(nullptr);  // 由 unique_ptr 管理

    // 收到第一張快照之前先顯示技能選擇
    latest.me = me;
    latest.mode = config.mode;
    latest.targetScore = config.targetScore;
    latest.timeRemaining = config.timeLimit;
    sinceSnapshot.start();

    connect(this->network.get(), &NetworkManager::messageReceived, this, &RemoteSession::onMessage);
    connect(this->network.get(), &NetworkManager::opponentLeft, this, [this] { onConnectionEnded(true); });
    connect(this->network.get(), &NetworkManager::disconnected, this, [this] { onConnectionEnded(false); });

    refreshTimer.setInterval(static_cast<int>(GameClock::kTickInterval));
    connect(&refreshTimer, &QTimer::timeout, this, &GameSession::stateChanged);
}

PlayerView RemoteSession::extrapolate(const PlayerView& view, TimeMs elapsed, const MatchConfig& config) {
    PlayerView v = view;
    if (elapsed <= 0) {
        return v;
    }
    if (v.status == GameStatus::Countdown) {
        v.countdownRemaining = std::max<TimeMs>(0, v.countdownRemaining - elapsed);  // G2
        v.now = std::min<TimeMs>(0, v.now + elapsed);
        return v;
    }
    if (v.status != GameStatus::Running) {
        return v;
    }
    v.now += elapsed;
    v.placeCooldownRemaining = std::max<TimeMs>(0, v.placeCooldownRemaining - elapsed);
    if (v.mode == MatchMode::TimeLimit) {
        v.timeRemaining = std::max<TimeMs>(0, v.timeRemaining - elapsed);
    }
    if (v.self.energy < config.maxEnergy) {
        // E6：下一格繼續往右長，但格數要等主機的快照（滿了就停在 1）
        v.nextEnergyRatio = std::min(1.0, v.nextEnergyRatio + static_cast<double>(elapsed) / config.regenInterval);
    }
    const TimeMs now = v.now;
    v.zones.erase(std::remove_if(v.zones.begin(), v.zones.end(), [now](const ZoneCell& z) { return now >= z.expiresAt; }),
                  v.zones.end());  // SZ3：到期那一刻起就不是禁區
    return v;
}

void RemoteSession::selectSkill(PlayerId player, SkillId skill) {
    if (player == me) {
        network->send(SelectSkillMessage{skill});
    }
}

void RemoteSession::confirmSkill(PlayerId player) {
    if (player == me) {
        network->send(ConfirmSkillMessage{});
    }
}

void RemoteSession::request(const Action& action) {
    Action own = action;
    std::visit([this](auto& a) { a.player = me; }, own);
    const std::uint32_t seq = nextSeq++;
    pendingRequests[seq] = targetOf(own);
    if (pendingRequests.size() > kMaxPendingRequests) {
        pendingRequests.erase(pendingRequests.begin());  // 被接受的請求不會有回覆，只留最近的
    }
    network->send(RequestMessage{seq, own});  // N3：不先改畫面
}

PlayerView RemoteSession::viewFor(PlayerId) const {
    return extrapolate(latest, sinceSnapshot.elapsed(), config);
}

void RemoteSession::requestRematch() {
    if (isOver(latest.status)) {
        network->send(RematchMessage{});  // G4：主機在雙方都按了之後重開
    } else if (isInMatch(latest.status) && !outgoingInvite && !incomingInvite) {
        outgoingInvite = true;  // N9
        network->send(RematchRequestMessage{});
    }
}

void RemoteSession::answerRematch(bool accept) {
    if (!incomingInvite) {
        return;
    }
    incomingInvite = false;
    network->send(RematchReplyMessage{accept});  // 同意時由主機重開，快照隨後送來
}

void RemoteSession::leave() {
    network->sendLeave();  // N8
    refreshTimer.stop();
    if (!isOver(latest.status)) {
        latest.status = GameStatus::Aborted;
    }
}

void RemoteSession::onMessage(const Message& message) {
    if (const auto* m = std::get_if<SnapshotMessage>(&message)) {
        onSnapshot(m->view);
    } else if (std::holds_alternative<OpponentReadyMessage>(message)) {
        emit opponentReady();
    } else if (const auto* m = std::get_if<RejectMessage>(&message)) {
        std::optional<Pos> pos;
        if (const auto it = pendingRequests.find(m->seq); it != pendingRequests.end()) {
            pos = it->second;
            pendingRequests.erase(it);
        }
        emit actionRejected(me, m->reason, pos);  // P6、U5
    } else if (const auto* m = std::get_if<LinesClearedMessage>(&message)) {
        emit linesCleared(m->lines);  // U11
    } else if (std::holds_alternative<RematchRequestMessage>(message)) {
        if (!isInMatch(latest.status) || incomingInvite || outgoingInvite) {
            return;  // 雙方同時邀請時由主機當作彼此同意
        }
        incomingInvite = true;
        emit rematchRequested();
    } else if (const auto* m = std::get_if<RematchReplyMessage>(&message)) {
        if (!outgoingInvite) {
            return;
        }
        outgoingInvite = false;
        if (!m->accept) {
            emit rematchDeclined();
        }
    }
}

void RemoteSession::onSnapshot(const PlayerView& view) {
    const GameStatus before = latest.status;
    latest = view;
    latest.me = me;
    sinceSnapshot.restart();
    if (view.status != before && (view.status == GameStatus::SkillSelect || isOver(view.status))) {
        outgoingInvite = false;
        incomingInvite = false;
    }
    if (isInMatch(view.status)) {
        refreshTimer.start();
    } else {
        refreshTimer.stop();
    }
    emit stateChanged();
    if (isOver(view.status) && !isOver(before)) {
        emit gameOver(view.status, view.scores, view.boardScores);  // U6
    }
}

void RemoteSession::onConnectionEnded(bool opponentLeft) {
    refreshTimer.stop();
    outgoingInvite = false;
    incomingInvite = false;
    if (!isOver(latest.status)) {
        latest.status = GameStatus::Aborted;  // W5：不計勝負
    }
    if (opponentLeft) {
        emit this->opponentLeft();  // N8
    } else {
        emit connectionLost();  // N6
    }
    emit stateChanged();
}
