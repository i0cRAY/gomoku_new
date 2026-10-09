#include "core/rule_checker.h"

#include <array>

namespace {

struct Direction {
    int dx;
    int dy;
};

// 橫、直、右下斜、右上斜
constexpr std::array<Direction, 4> kDirections{{{1, 0}, {0, 1}, {1, 1}, {1, -1}}};

// 從 last 往 (dx, dy) 方向走，把同色棋子加進 out（不含 last 本身）
void collect(const Board& board, Pos last, Cell color, int dx, int dy, std::vector<Pos>& out) {
    Pos p{last.x + dx, last.y + dy};
    while (board.inBounds(p) && board.at(p) == color) {
        out.push_back(p);
        p = Pos{p.x + dx, p.y + dy};
    }
}

}  // namespace

std::optional<std::vector<Pos>> RuleChecker::findFive(const Board& board, Pos last) {
    const Cell color = board.at(last);
    if (color == Cell::Empty) {
        return std::nullopt;
    }

    std::vector<Pos> winning;
    for (const Direction d : kDirections) {
        std::vector<Pos> line;
        collect(board, last, color, d.dx, d.dy, line);
        collect(board, last, color, -d.dx, -d.dy, line);
        if (static_cast<int>(line.size()) + 1 >= kWinLength) {
            winning.insert(winning.end(), line.begin(), line.end());
        }
    }

    if (winning.empty()) {
        return std::nullopt;
    }
    winning.push_back(last);
    return winning;
}
