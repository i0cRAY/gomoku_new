#include "core/energy_manager.h"

#include <algorithm>
#include <cassert>

EnergyManager::EnergyManager(const MatchConfig& config)
    : regenInterval(config.regenInterval),
      maxEnergy(config.maxEnergy),
      accelerateMultiplier(config.skill.accelerateRegenMultiplier) {
    assert(regenInterval > 0);
}

void EnergyManager::advance(PlayerState& state, TimeMs from, TimeMs to) const {
    if (to <= from) {
        return;
    }
    if (state.energy >= maxEnergy) {
        state.regenProgress = 0;  // E4
        return;
    }

    state.regenProgress += effectiveElapsed(state, from, to);  // E2、SA2
    const TimeMs gained = state.regenProgress / regenInterval;  // E3：一次補足所有應回的格數
    const int missing = maxEnergy - state.energy;
    if (gained >= missing) {
        state.energy = maxEnergy;
        state.regenProgress = 0;  // E4
    } else {
        state.energy += static_cast<int>(gained);
        state.regenProgress -= gained * regenInterval;
    }
}

// 區間內加速的部分以倍率計算，其餘照常（SA2）。
// 加速一定是在推進到使用當下之後才開始，所以區間內的加速段只會在開頭。
TimeMs EnergyManager::effectiveElapsed(const PlayerState& state, TimeMs from, TimeMs to) const {
    const TimeMs elapsed = to - from;
    if (state.accelerateUntil == 0 || state.accelerateUntil <= from) {  // 0 表示沒有加速
        return elapsed;
    }
    const TimeMs accelerated = std::min(to, state.accelerateUntil) - from;
    return elapsed + accelerated * (accelerateMultiplier - 1);
}

bool EnergyManager::canConsume(const PlayerState& state) const {
    return state.energy >= 1;
}

void EnergyManager::consume(PlayerState& state) const {
    assert(canConsume(state));
    --state.energy;  // E6：進度不變；滿格時進度本來就是 0（E4）
}

double EnergyManager::nextEnergyRatio(const PlayerState& state) const {
    if (state.energy >= maxEnergy) {
        return 0.0;
    }
    return std::clamp(static_cast<double>(state.regenProgress) / static_cast<double>(regenInterval), 0.0, 1.0);
}
