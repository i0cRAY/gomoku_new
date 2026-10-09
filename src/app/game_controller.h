#pragma once

#include <QObject>

#include <array>
#include <optional>
#include <vector>

#include "core/ai_engine.h"
#include "core/board.h"
#include "core/config.h"
#include "core/energy_manager.h"
#include "core/player_state.h"
#include "core/player_view.h"
#include "core/skill_system.h"
#include "core/types.h"
#include "core/zone_map.h"

// 唯一可以修改遊戲狀態的地方（spec G、P、W、SZ、SX）。雙方完整的 PlayerState 不對外公開，
// 外部一律透過 viewFor 取得 PlayerView（spec E5）。
class GameController : public QObject {
    Q_OBJECT
public:
    explicit GameController(MatchConfig, QObject* parent = nullptr);

    void selectSkill(PlayerId, SkillId);             // 技能選擇階段、尚未確定時才有效（spec S1、G1a）
    void confirmSkill(PlayerId);                     // 未選技能時忽略；雙方都確定後進入倒數，對局時間從 −3000 起算（spec G2）
    ActionResult submit(const Action&, TimeMs now);  // 依 spec P1 / SB4 / SZ5 / SX4 的順序檢查
    void tick(TimeMs now);                           // 推進倒數、能量、時限（W7）、禁區到期與 AI
    PlayerView viewFor(PlayerId) const;
    // G4、G4a：任何狀態都可呼叫。保留設定與上一局的技能選擇，其餘全部重置並回到技能選擇
    void restart();
    void abort();  // G4a、G4b、W5：狀態變成 Aborted，不計勝負；已結束時忽略
    // M1：由 AI 操作這一方。技能選擇階段自動選技能並確定（A2a）；對局中每次 tick 決策，
    // 動作一律經過 submit（A1），AI 只拿得到 viewFor(這一方)（A3、E5）
    void attachAI(PlayerId, AIEngine);

signals:
    void stateChanged();  // 收到後呼叫 viewFor(自己) 取資料
    void actionRejected(PlayerId, RejectReason, std::optional<Pos>);
    void linesCleared(std::vector<ClearedLine>);         // W1、U11
    void gameOver(GameStatus, std::array<int, 2> scores);  // U6
    void skillConfirmed(PlayerId);  // 某位玩家按下確定（只告知是誰，不透露選了什麼，spec S1、U7）

private:
    void advanceTo(TimeMs now);
    void startMatch();
    void initPlayers();  // 開局能量、所選技能、S4 冷卻（對局時間 0 起算）
    void letAIChooseSkill(PlayerId);
    void runAIs(TimeMs now);
    ActionResult submitPlace(const PlaceAction&, TimeMs now);
    ActionResult submitSkill(const SkillAction&, TimeMs now);
    ActionResult reject(PlayerId, RejectReason, std::optional<Pos>);
    void clearLines(PlayerId, Pos last);  // W1、W2、W6
    void finish(GameStatus);
    void finishByScore();  // W4、W7、W9
    bool isTimeLimited() const { return config.mode == MatchMode::TimeLimit; }
    std::array<int, 2> scores() const { return {players[0].score, players[1].score}; }

    static std::size_t indexOf(PlayerId p) { return static_cast<std::size_t>(p); }
    PlayerState& state(PlayerId p) { return players[indexOf(p)]; }
    const PlayerState& state(PlayerId p) const { return players[indexOf(p)]; }

    MatchConfig config;
    EnergyManager energy;
    SkillSystem skills;
    Board board;
    ZoneMap zones;
    GameStatus status = GameStatus::SkillSelect;
    std::array<PlayerState, 2> players{};
    std::array<std::optional<SkillId>, 2> selectedSkill{};
    std::array<bool, 2> confirmed{};
    std::array<bool, 2> skillRevealed{};  // 是否已經用過技能（S1：用過後對手才知道）
    TimeMs lastTime = 0;  // 最後一次推進到的對局時間
    std::vector<ClearedLine> lastClearedLines;
    std::array<std::optional<AIEngine>, 2> ais{};
};
