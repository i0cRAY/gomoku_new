#include "core/rule_checker.h"

#include <array>
#include <utility>

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

bool isEmpty(const Board& board, Pos p) {
    return board.inBounds(p) && board.at(p) == Cell::Empty;
}

}  // namespace

std::vector<std::vector<Pos>> RuleChecker::findLines(const Board& board, Pos last, int minLength) {
    std::vector<std::vector<Pos>> lines;
    const Cell color = board.at(last);
    if (color != Cell::Black && color != Cell::White) {
        return lines;
    }
    for (const Direction d : kDirections) {
        std::vector<Pos> line{last};
        collect(board, last, color, d.dx, d.dy, line);
        collect(board, last, color, -d.dx, -d.dy, line);
        if (static_cast<int>(line.size()) >= minLength) {
            lines.push_back(std::move(line));
        }
    }
    return lines;
}

int RuleChecker::boardScore(const Board& board, PlayerId player, const BoardScoreConfig& config) {
    const Cell color = player == PlayerId::Black ? Cell::Black : Cell::White;
    int total = 0;
    for (const Direction d : kDirections) {
        for (int y = 0; y < Board::kSize; ++y) {
            for (int x = 0; x < Board::kSize; ++x) {
                const Pos start{x, y};
                const Pos before{x - d.dx, y - d.dy};
                // 只從每段連續子的起點開始算，同一段只算一次
                if (board.at(start) != color || (board.inBounds(before) && board.at(before) == color)) {
                    continue;
                }
                int length = 0;
                Pos after = start;
                while (board.inBounds(after) && board.at(after) == color) {
                    ++length;
                    after = Pos{after.x + d.dx, after.y + d.dy};
                }
                const int openEnds = (isEmpty(board, before) ? 1 : 0) + (isEmpty(board, after) ? 1 : 0);
                if (length == 4 && openEnds == 2) {
                    total += config.openFour;
                } else if (length == 4 && openEnds == 1) {
                    total += config.closedFour;
                } else if (length == 3 && openEnds == 2) {
                    total += config.openThree;
                }
            }
        }
    }
    return total;
}
