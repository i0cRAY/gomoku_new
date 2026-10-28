#pragma once

#include <optional>
#include <vector>

#include "core/board.h"
#include "core/config.h"
#include "core/energy_manager.h"
#include "core/player_state.h"
#include "core/player_view.h"
#include "core/types.h"

namespace t25 {

// spec §11：T25 完成時（commit 0b18323）OpponentModel 的凍結複本，只給 T25AI 用，之後不修改（design §4.8）。
// spec A14、A14a：只用自己 PlayerView 裡的公開資訊，推算對手的能量與下子間隔（design §4.8）。
// 拿不到對手的 PlayerState（E5），只能比對前後兩次觀察到的盤面、消除的連線、禁區與揭露的技能
class OpponentModel {
public:
    OpponentModel(PlayerId self, const MatchConfig&);

    // A14：每次 tick 呼叫。第一次觀察只記下起點，當時盤面上的棋子不算對手的行動
    void observe(const PlayerView&);
    void reset();  // 新的一局

    int energy() const { return estimate.energy; }  // 推算的目前能量（E2–E4）
    // A14a：從最後一次觀察起 within 毫秒內（含），對手最多能下幾子；反應時間當作 0
    int placementsWithin(TimeMs within) const;
    // 對手還沒揭露技能，或揭露的是摧毀但還沒用過
    bool mayDestroy() const;
    // A14a：within 內能下至少一子，或（countDestroy 時）可能還能用摧毀且能量夠（S5：摧毀不受下子間隔限制）
    bool canRespondWithin(TimeMs within, bool countDestroy) const;

private:
    void spend(int amount);
    void countSkillReveal(const PlayerView&);
    void countZoneStones(const PlayerView&);
    int countPlacements(const PlayerView&) const;

    PlayerId self;
    PlayerId other;
    EnergyManager energyRules;
    TimeMs regenInterval;
    TimeMs placeCooldown;
    int startEnergy;
    SkillConfig skills;

    bool started = false;
    TimeMs lastTime = 0;
    PlayerState estimate;  // 只用 energy、regenProgress、lastPlaceTime、dominateCharges、destroyUsed
    Board lastBoard;
    std::vector<ClearedLine> lastCleared;
    std::optional<SkillId> revealed;
    std::optional<TimeMs> lastZoneExpiry;  // 已經算過的對手禁區中最晚的到期時間
};

}  // namespace t25
