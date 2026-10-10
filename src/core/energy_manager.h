#pragma once

#include "core/config.h"
#include "core/player_state.h"
#include "core/types.h"

// spec E1–E4、E6、S2
class EnergyManager {
public:
    explicit EnergyManager(const MatchConfig&);  // 使用 regenInterval、maxEnergy

    // 把 state 從 from 推進到 to，套用回能規則。結果與呼叫頻率無關（E3）
    void advance(PlayerState&, TimeMs from, TimeMs to) const;
    bool canConsume(const PlayerState&, int amount = 1) const;  // 下子 1 格、技能依 SkillConfig::costOf（S2）
    void consume(PlayerState&, int amount = 1) const;            // 呼叫前必須先通過 canConsume
    // 下一格能量的累積比例 0.0–1.0（= regenProgress / T）；能量已滿回傳 0（E6）
    double nextEnergyRatio(const PlayerState&) const;

private:
    TimeMs regenInterval;
    int maxEnergy;
};
