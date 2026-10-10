#pragma once

#include <optional>

#include "core/board.h"
#include "core/config.h"
#include "core/player_state.h"
#include "core/types.h"
#include "core/zone_map.h"

// spec S1–S5、SB1–SB5、SZ1–SZ5、SX1–SX5
// GAME_NOT_RUNNING 由 GameController 先檢查，這裡從 SKILL_NOT_OWNED 開始。
// 能量是否足夠（S3）在這裡檢查，扣能量由 GameController 透過 EnergyManager 處理（design §4.5）
class SkillSystem {
public:
    explicit SkillSystem(SkillConfig);

    void initPlayer(PlayerState&) const;  // 開局：清掉霸道次數、摧毀已用
    std::optional<RejectReason> check(const SkillAction&, const PlayerState&, const Board&) const;
    void apply(const SkillAction&, PlayerState&, Board&, ZoneMap&, TimeMs now) const;  // 呼叫前必須先通過 check
    int costOf(SkillId skill) const { return config.costOf(skill); }  // S2
    // SZ2：成功下子後呼叫；霸道次數 > 0 時扣 1，並在該子上下左右（棋盤內）產生對手的禁區
    void onPlaced(PlayerId, PlayerState&, Pos, ZoneMap&, TimeMs now) const;

private:
    void bombArea(Pos topLeft, Board&) const;
    void destroyArea(Pos center, Board&, ZoneMap&) const;

    SkillConfig config;
};
