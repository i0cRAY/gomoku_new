#include "core/skill_system.h"

#include <algorithm>

SkillSystem::SkillSystem(SkillConfig config) : config(config) {}

void SkillSystem::initPlayer(PlayerState& state, TimeMs matchStart) const {
    state.skillReadyAt = matchStart + cooldownOf(state.skill);
    state.accelerateUntil = 0;
}

namespace {

Cell stoneOf(PlayerId player) {
    return player == PlayerId::Black ? Cell::Black : Cell::White;
}

}  // namespace

// 加速：SKILL_NOT_OWNED → SKILL_COOLDOWN（SA5，忽略目標座標）
// 炸彈：SKILL_NOT_OWNED → OUT_OF_BOARD（沒有目標時為 INVALID_TARGET）→ SKILL_COOLDOWN → INVALID_TARGET（SB4、SB5）
std::optional<RejectReason> SkillSystem::check(const SkillAction& action, const PlayerState& state,
                                               const Board& board, TimeMs now) const {
    if (action.skill != state.skill) {
        return RejectReason::SkillNotOwned;  // S1a
    }
    if (action.skill == SkillId::Bomb) {
        if (!action.target) {
            return RejectReason::InvalidTarget;  // SB5
        }
        if (!board.inBounds(*action.target)) {
            return RejectReason::OutOfBoard;  // SB2
        }
    }
    if (now < state.skillReadyAt) {
        return RejectReason::SkillCooldown;  // S3：剛好到期那一刻可用
    }
    if (action.skill == SkillId::Bomb && board.at(*action.target) != stoneOf(opponent(action.player))) {
        return RejectReason::InvalidTarget;  // SB2：空格或自己的棋子
    }
    return std::nullopt;
}

void SkillSystem::apply(const SkillAction& action, PlayerState& state, Board& board, TimeMs now) const {
    state.skillReadyAt = now + cooldownOf(action.skill);  // S6
    if (action.skill == SkillId::Accelerate) {
        state.accelerateUntil = now + config.accelerateDuration;  // SA1
    } else {
        board.set(*action.target, Cell::Empty);  // SB3
    }
}

TimeMs SkillSystem::remainingCooldown(const PlayerState& state, TimeMs now) const {
    return std::max<TimeMs>(0, state.skillReadyAt - now);
}

TimeMs SkillSystem::cooldownOf(SkillId skill) const {
    return skill == SkillId::Accelerate ? config.accelerateCooldown : config.bombCooldown;
}
