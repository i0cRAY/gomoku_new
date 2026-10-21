#include "ui/board_overlay.h"

#include <algorithm>
#include <numeric>

#include "core/board.h"

namespace {

constexpr TimeMs kMsPerSecond = 1000;

}  // namespace

std::optional<SkillArea> skillArea(SkillId skill, const SkillConfig& config) {
    switch (skill) {
        case SkillId::Bomb: return SkillArea{0, config.bombSize - 1};
        case SkillId::Destroy: return SkillArea{config.destroyRadius, config.destroyRadius};
        case SkillId::Dominate: return std::nullopt;
    }
    return std::nullopt;
}

std::vector<Pos> areaCells(Pos target, SkillArea area) {
    std::vector<Pos> cells;
    for (int y = std::max(0, target.y - area.before); y <= std::min(Board::kSize - 1, target.y + area.after); ++y) {
        for (int x = std::max(0, target.x - area.before); x <= std::min(Board::kSize - 1, target.x + area.after);
             ++x) {
            cells.push_back({x, y});
        }
    }
    return cells;
}

double zoneOpacity(const ZoneCell& zone, TimeMs now, TimeMs duration) {
    if (duration <= 0) {
        return 0.0;
    }
    return std::clamp(static_cast<double>(zone.expiresAt - now) / static_cast<double>(duration), 0.0, 1.0);
}

std::string clearedScoreText(const std::vector<ClearedLine>& lines) {
    if (lines.empty()) {
        return {};
    }
    const std::size_t total = std::accumulate(lines.begin(), lines.end(), std::size_t{0},
                                              [](std::size_t sum, const ClearedLine& l) { return sum + l.stones.size(); });
    return "+" + std::to_string(total);
}

std::optional<Pos> clearedLabelCell(const std::vector<ClearedLine>& lines) {
    if (lines.empty() || lines.front().stones.empty()) {
        return std::nullopt;
    }
    std::vector<Pos> stones = lines.front().stones;
    std::sort(stones.begin(), stones.end(), [](Pos a, Pos b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
    return stones[stones.size() / 2];
}

std::vector<Pos> newlyDestroyedCells(const Board& before, const Board& after) {
    std::vector<Pos> cells;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            if (after.at({x, y}) == Cell::Destroyed && before.at({x, y}) != Cell::Destroyed) {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

std::optional<int> explosionFrame(TimeMs elapsed) {
    if (elapsed < 0 || elapsed >= kExplosionDurationMs) {
        return std::nullopt;
    }
    return static_cast<int>(elapsed * kExplosionFps / kMsPerSecond);
}

int laughFrame(TimeMs elapsed) {
    const TimeMs inLoop = ((elapsed % kLaughLoopMs) + kLaughLoopMs) % kLaughLoopMs;
    return static_cast<int>(inLoop * kLaughFps / kMsPerSecond);
}

bool laughActive(const PlayerView& view) {
    return view.status == GameStatus::Running && !view.zones.empty();
}
