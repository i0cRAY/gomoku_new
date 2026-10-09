#pragma once

#include <QObject>

#include <optional>
#include <vector>

#include "core/player_view.h"
#include "core/types.h"

// UI 依賴的抽象介面，讓本機、主機、加入方共用同一套畫面（design §4.6）
class GameSession : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    virtual void selectSkill(PlayerId, SkillId) = 0;
    virtual void confirmSkill(PlayerId) = 0;
    virtual void request(const Action&) = 0;  // 時間由實作自己取；結果經由 signals 回報
    virtual PlayerView viewFor(PlayerId) const = 0;
    virtual void requestRematch() = 0;  // spec G4

signals:
    void stateChanged();
    void actionRejected(PlayerId, RejectReason, std::optional<Pos>);
    void gameOver(GameStatus, std::vector<Pos> winningLine);
    void opponentReady();  // 只告知對手已確定技能（spec S1、U7）
};
