#include "core/skill_system.h"

#include <algorithm>

SkillSystem::SkillSystem(SkillConfig config) : config(config) {}

void SkillSystem::initPlayer(PlayerState& state, TimeMs matchStart) const {
    state.skillReadyAt = matchStart + cooldownOf(state.skill);
    state.accelerateUntil = 0;
}

std::optional<RejectReason> SkillSystem::check(const SkillAction& action, const PlayerState& state,
                                               const Board& /*board*/, TimeMs now) const {
    if (action.skill != state.skill) {
        return RejectReason::SkillNotOwned;  // S1a
    }
    if (now < state.skillReadyAt) {
        return RejectReason::SkillCooldown;  // S3：剛好到期那一刻可用
    }
    return std::nullopt;  // SA5：加速忽略目標座標
}

void SkillSystem::apply(const SkillAction& action, PlayerState& state, Board& /*board*/, TimeMs now) const {
    state.skillReadyAt = now + cooldownOf(action.skill);  // S6
    if (action.skill == SkillId::Accelerate) {
        state.accelerateUntil = now + config.accelerateDuration;  // SA1
    }
}

TimeMs SkillSystem::remainingCooldown(const PlayerState& state, TimeMs now) const {
    return std::max<TimeMs>(0, state.skillReadyAt - now);
}

TimeMs SkillSystem::cooldownOf(SkillId skill) const {
    return skill == SkillId::Accelerate ? config.accelerateCooldown : config.bombCooldown;
}
