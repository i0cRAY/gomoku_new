#pragma once

#include <QObject>

#include <array>
#include <optional>
#include <vector>

#include "core/board.h"
#include "core/config.h"
#include "core/energy_manager.h"
#include "core/player_state.h"
#include "core/player_view.h"
#include "core/skill_system.h"
#include "core/types.h"

// 唯一可以修改遊戲狀態的地方（spec G、P、W）。雙方完整的 PlayerState 不對外公開，
// 外部一律透過 viewFor 取得 PlayerView（spec E5）。
class GameController : public QObject {
    Q_OBJECT
public:
    explicit GameController(MatchConfig, QObject* parent = nullptr);

    void selectSkill(PlayerId, SkillId);             // 技能選擇階段、尚未確定時才有效（spec S1、G1a）
    void confirmSkill(PlayerId);                     // 未選技能時忽略；雙方都確定後進入倒數，對局時間從 −3000 起算（spec G2）
    ActionResult submit(const Action&, TimeMs now);  // 依 spec P1 / SA5 / SB4 的順序檢查
    void tick(TimeMs now);                           // 推進倒數與能量
    PlayerView viewFor(PlayerId) const;
    void restart();  // G4：對局結束後才有效；保留設定與上一局的技能選擇，其餘全部重置

signals:
    void stateChanged();  // 收到後呼叫 viewFor(自己) 取資料
    void actionRejected(PlayerId, RejectReason, std::optional<Pos>);
    void gameOver(GameStatus, std::vector<Pos> winningLine);
    void skillConfirmed(PlayerId);  // 某位玩家按下確定（只告知是誰，不透露選了什麼，spec S1、U7）

private:
    void advanceTo(TimeMs now);
    void startMatch();
    void initPlayers();  // 開局能量、所選技能、S4 冷卻（對局時間 0 起算）
    ActionResult submitPlace(const PlaceAction&, TimeMs now);
    ActionResult submitSkill(const SkillAction&, TimeMs now);
    ActionResult reject(PlayerId, RejectReason, std::optional<Pos>);
    void finish(GameStatus, std::vector<Pos> line);

    static std::size_t indexOf(PlayerId p) { return static_cast<std::size_t>(p); }
    PlayerState& state(PlayerId p) { return players[indexOf(p)]; }
    const PlayerState& state(PlayerId p) const { return players[indexOf(p)]; }

    MatchConfig config;
    EnergyManager energy;
    SkillSystem skills;
    Board board;
    GameStatus status = GameStatus::SkillSelect;
    std::array<PlayerState, 2> players{};
    std::array<std::optional<SkillId>, 2> selectedSkill{};
    std::array<bool, 2> confirmed{};
    std::array<bool, 2> skillRevealed{};  // 是否已經用過技能（S1：用過後對手才知道）
    TimeMs lastTime = 0;  // 最後一次推進到的對局時間
    std::vector<Pos> winningLine;
};
