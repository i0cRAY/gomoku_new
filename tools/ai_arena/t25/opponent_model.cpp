#include "t25/opponent_model.h"

#include <algorithm>
#include <set>

namespace t25 {

namespace {

Cell stoneOf(PlayerId p) {
    return p == PlayerId::Black ? Cell::Black : Cell::White;
}

bool sameLines(const std::vector<ClearedLine>& a, const std::vector<ClearedLine>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const ClearedLine& x, const ClearedLine& y) {
        return x.owner == y.owner && x.stones == y.stones;
    });
}

}  // namespace

OpponentModel::OpponentModel(PlayerId self, const MatchConfig& config)
    : self(self),
      other(opponent(self)),
      energyRules(config),
      regenInterval(config.regenInterval),
      placeCooldown(config.placeCooldown),
      startEnergy(config.startEnergy),
      skills(config.skill) {
    reset();
}

void OpponentModel::reset() {
    started = false;
    lastTime = 0;
    estimate = PlayerState{};
    estimate.energy = startEnergy;  // spec §4：開局能量，對局時間 0 起算
    lastBoard.clear();
    lastCleared.clear();
    revealed.reset();
    lastZoneExpiry.reset();
}

void OpponentModel::observe(const PlayerView& view) {
    if (view.status != GameStatus::Running) {
        return;
    }
    energyRules.advance(estimate, lastTime, view.now);  // E2–E4
    lastTime = std::max(lastTime, view.now);

    if (started) {
        countSkillReveal(view);
        countZoneStones(view);
        const int placements = countPlacements(view);
        for (int i = 0; i < placements; ++i) {
            spend(1);  // 每子 1 格（§5）
            estimate.lastPlaceTime = view.now;
        }
    } else {
        started = true;  // 起點：之前的事不算
        revealed = view.opponentSkillRevealed;
        for (const ZoneCell& zone : view.zones) {
            if (zone.owner == other) {
                lastZoneExpiry = std::max(lastZoneExpiry.value_or(zone.expiresAt), zone.expiresAt);
            }
        }
    }
    lastBoard = view.board;
    lastCleared = view.lastClearedLines;
}

// 對手第一次用技能時才會揭露（S1）：揭露的那一刻就是一次使用
void OpponentModel::countSkillReveal(const PlayerView& view) {
    if (revealed || !view.opponentSkillRevealed) {
        return;
    }
    revealed = view.opponentSkillRevealed;
    spend(skills.costOf(*revealed));
    if (*revealed == SkillId::Dominate) {
        estimate.dominateCharges += skills.dominateStones;  // SZ1
    } else if (*revealed == SkillId::Destroy) {
        estimate.destroyUsed = true;  // SX3
    }
}

// SZ2：對手每一顆帶霸道的子產生一批同時到期的禁區。次數推算為 0 卻又出現，代表中間又用了一次霸道
void OpponentModel::countZoneStones(const PlayerView& view) {
    std::set<TimeMs> newStones;
    for (const ZoneCell& zone : view.zones) {
        if (zone.owner == other && (!lastZoneExpiry || zone.expiresAt > *lastZoneExpiry)) {
            newStones.insert(zone.expiresAt);
        }
    }
    for (TimeMs expiresAt : newStones) {
        if (estimate.dominateCharges == 0) {
            spend(skills.dominateCost);
            estimate.dominateCharges += skills.dominateStones;
        }
        --estimate.dominateCharges;
        lastZoneExpiry = expiresAt;
    }
}

// 新出現的對手棋子，加上當場連成五被消除的那一子（W1；W6 同一子的多條連線只算一次）
int OpponentModel::countPlacements(const PlayerView& view) const {
    const Cell stone = stoneOf(other);
    int count = 0;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            const Pos p{x, y};
            count += view.board.at(p) == stone && lastBoard.at(p) != stone ? 1 : 0;
        }
    }
    if (!sameLines(view.lastClearedLines, lastCleared)) {
        const bool clearedNewStone = std::any_of(
            view.lastClearedLines.begin(), view.lastClearedLines.end(), [&](const ClearedLine& line) {
                return line.owner == other && std::any_of(line.stones.begin(), line.stones.end(),
                                                          [&](Pos p) { return lastBoard.at(p) != stone; });
            });
        count += clearedNewStone ? 1 : 0;
    }
    return count;
}

// 推算偏低時（看到行動但推算能量不夠）停在 0；E4：從滿格扣掉時進度本來就是 0
void OpponentModel::spend(int amount) {
    estimate.energy = std::max(0, estimate.energy - amount);
}

int OpponentModel::placementsWithin(TimeMs within) const {
    PlayerState s = estimate;
    const TimeMs end = lastTime + within;
    TimeMs simTime = lastTime;
    TimeMs next = s.lastPlaceTime ? std::max(lastTime, *s.lastPlaceTime + placeCooldown) : lastTime;  // P4
    int count = 0;
    while (next <= end) {
        energyRules.advance(s, simTime, next);
        simTime = next;
        if (s.energy >= 1) {
            --s.energy;
            ++count;
            next = simTime + placeCooldown;
        } else {
            next = simTime + (regenInterval - s.regenProgress);  // 下一格能量回來的時間
        }
    }
    return count;
}

bool OpponentModel::mayDestroy() const {
    return !revealed || (*revealed == SkillId::Destroy && !estimate.destroyUsed);
}

bool OpponentModel::canRespondWithin(TimeMs within, bool countDestroy) const {
    if (placementsWithin(within) >= 1) {
        return true;
    }
    return countDestroy && mayDestroy() && estimate.energy >= skills.destroyCost;
}

}  // namespace t25
