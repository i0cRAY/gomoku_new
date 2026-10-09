#pragma once

#include <optional>

#include "core/board.h"
#include "core/config.h"
#include "core/player_state.h"
#include "core/types.h"

// spec S1–S6、SA1–SA5、SB1–SB4
// GAME_NOT_RUNNING 由 GameController 先檢查，這裡從 SKILL_NOT_OWNED 開始
class SkillSystem {
public:
    explicit SkillSystem(SkillConfig);

    void initPlayer(PlayerState&, TimeMs matchStart) const;  // S4：開局時技能處於冷卻中
    std::optional<RejectReason> check(const SkillAction&, const PlayerState&, const Board&, TimeMs now) const;
    void apply(const SkillAction&, PlayerState&, Board&, TimeMs now) const;  // 呼叫前必須先通過 check
    TimeMs remainingCooldown(const PlayerState&, TimeMs now) const;

private:
    TimeMs cooldownOf(SkillId) const;

    SkillConfig config;
};
