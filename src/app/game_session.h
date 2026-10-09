#pragma once

#include <QObject>

#include <array>
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
    virtual void requestRematch() = 0;             // spec G4、G4a（區網對局中為送出邀請，N9）
    virtual void answerRematch(bool accept) = 0;   // spec N9，只有區網會用到
    virtual void leave() = 0;                      // spec G4、G4a、G4b：回主選單（區網送出 leave，N8）

signals:
    void stateChanged();
    void actionRejected(PlayerId, RejectReason, std::optional<Pos>);
    void linesCleared(std::vector<ClearedLine>);         // W1、U11
    void gameOver(GameStatus, std::array<int, 2> scores);  // U6
    void opponentReady();     // 只告知對手已確定技能（spec S1、U7）
    void rematchRequested();  // 對手在對局中提出重開（N9）
    void rematchDeclined();   // 對手拒絕了我的邀請（N9）
    void opponentLeft();      // N8
};
