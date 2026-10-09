#include "ui/board_overlay.h"

#include <algorithm>
#include <numeric>

#include "core/board.h"

namespace {

constexpr TimeMs kMsPerSecond = 1000;

}  // namespace

std::vector<Pos> destroyPreviewCells(Pos center, int radius) {
    std::vector<Pos> cells;
    for (int y = std::max(0, center.y - radius); y <= std::min(Board::kSize - 1, center.y + radius); ++y) {
        for (int x = std::max(0, center.x - radius); x <= std::min(Board::kSize - 1, center.x + radius); ++x) {
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

int zoneRemainingSeconds(const ZoneCell& zone, TimeMs now) {
    const TimeMs left = zone.expiresAt - now;
    return left <= 0 ? 0 : static_cast<int>((left + kMsPerSecond - 1) / kMsPerSecond);
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
