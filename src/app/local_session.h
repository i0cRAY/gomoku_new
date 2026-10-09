#pragma once

#include <cstdint>

#include "app/game_clock.h"
#include "app/game_controller.h"
#include "app/game_session.h"
#include "core/config.h"

// 本機執行規則的 GameSession：M1、M3 與區網主機端都用它
class LocalSession : public GameSession {
    Q_OBJECT
public:
    // localPlayer：這台電腦的玩家，用來判斷「對手已確定」（M3 兩方都在本機時任選一方）
    LocalSession(MatchConfig, PlayerId localPlayer, QObject* parent = nullptr);

    void selectSkill(PlayerId, SkillId) override;
    void confirmSkill(PlayerId) override;
    void request(const Action&) override;
    PlayerView viewFor(PlayerId) const override;
    void requestRematch() override;     // 本機直接重開（G4、G4a）
    void answerRematch(bool) override;  // 本機沒有邀請，忽略
    void leave() override;              // 中止本局並停止時鐘（G4a、G4b）

    void attachAI(PlayerId, std::uint32_t seed);  // M1：AI 操作這一方
    const GameClock& clock() const { return gameClock; }

private:
    MatchConfig config;
    PlayerId localPlayer;
    GameController controller;
    GameClock gameClock;
};
