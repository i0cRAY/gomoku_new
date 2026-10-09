#include "core/energy_manager.h"

#include <algorithm>
#include <cassert>

EnergyManager::EnergyManager(const MatchConfig& config)
    : regenInterval(config.regenInterval), maxEnergy(config.maxEnergy) {
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

    state.regenProgress += to - from;  // E2
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

bool EnergyManager::canConsume(const PlayerState& state, int amount) const {
    return state.energy >= amount;
}

void EnergyManager::consume(PlayerState& state, int amount) const {
    assert(canConsume(state, amount));
    state.energy -= amount;  // E6：進度不變；滿格時進度本來就是 0（E4）
}

double EnergyManager::nextEnergyRatio(const PlayerState& state) const {
    if (state.energy >= maxEnergy) {
        return 0.0;
    }
    return std::clamp(static_cast<double>(state.regenProgress) / static_cast<double>(regenInterval), 0.0, 1.0);
}
