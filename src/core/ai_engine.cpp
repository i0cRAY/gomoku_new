#include "core/ai_engine.h"

#include <array>
#include <vector>

namespace {

struct Direction {
    int dx;
    int dy;
};

constexpr std::array<Direction, AIEngine::kDirectionCount> kDirections{{{1, 0}, {0, 1}, {1, 1}, {1, -1}}};

constexpr int kWinLength = 5;
constexpr int kReach = kWinLength - 1;           // 連五只會用到中心左右各 4 格
constexpr int kLineLength = 2 * kReach + 1;
constexpr int kCenter = kReach;
constexpr double kScoreEpsilon = 1e-9;  // 比較同分用

enum class LineCell { Own, Empty, Blocked };
using Line = std::array<LineCell, kLineLength>;

// 通過中心的連續己方棋子數
int runThroughCenter(const Line& line) {
    int run = 1;
    for (int i = kCenter - 1; i >= 0 && line[i] == LineCell::Own; --i) {
        ++run;
    }
    for (int i = kCenter + 1; i < kLineLength && line[i] == LineCell::Own; ++i) {
        ++run;
    }
    return run;
}

// 成五點：再下一子就能形成通過中心的連五的空格數
int fivePointCount(Line line) {
    int count = 0;
    for (int i = 0; i < kLineLength; ++i) {
        if (line[i] != LineCell::Empty) {
            continue;
        }
        line[i] = LineCell::Own;
        if (runThroughCenter(line) >= kWinLength) {
            ++count;
        }
        line[i] = LineCell::Empty;
    }
    return count;
}

// 連五、活四（2 個以上成五點）、衝四（1 個成五點）
Pattern fourLevel(const Line& line) {
    if (runThroughCenter(line) >= kWinLength) {
        return Pattern::Five;
    }
    const int points = fivePointCount(line);
    if (points >= 2) {
        return Pattern::OpenFour;
    }
    return points == 1 ? Pattern::Four : Pattern::None;
}

// 活三：再下一子能變成活四；眠三：再下一子最多只能變成衝四
Pattern threeLevel(Line line) {
    if (const Pattern four = fourLevel(line); four != Pattern::None) {
        return four;
    }
    Pattern best = Pattern::None;
    for (int i = 0; i < kLineLength; ++i) {
        if (line[i] != LineCell::Empty) {
            continue;
        }
        line[i] = LineCell::Own;
        const Pattern next = fourLevel(line);
        line[i] = LineCell::Empty;
        if (next == Pattern::OpenFour) {
            return Pattern::OpenThree;
        }
        if (next == Pattern::Four) {
            best = Pattern::Three;
        }
    }
    return best;
}

// 活二：再下一子能變成活三；眠二：再下一子最多只能變成眠三
Pattern twoLevel(Line line) {
    if (const Pattern three = threeLevel(line); three != Pattern::None) {
        return three;
    }
    Pattern best = Pattern::None;
    for (int i = 0; i < kLineLength; ++i) {
        if (line[i] != LineCell::Empty) {
            continue;
        }
        line[i] = LineCell::Own;
        const Pattern next = threeLevel(line);
        line[i] = LineCell::Empty;
        if (next == Pattern::OpenThree) {
            return Pattern::OpenTwo;
        }
        if (next == Pattern::Three) {
            best = Pattern::Two;
        }
    }
    return best;
}

}  // namespace

AIEngine::AIEngine(PlayerId self, TimeMs reactionTime, double defenseWeight, std::uint32_t seed,
                   PatternScores scores)
    : self(self), reactionTime(reactionTime), defenseWeight(defenseWeight), rng(seed), scores(scores) {}

int AIEngine::patternScore(Pattern pattern, const PatternScores& scores) {
    switch (pattern) {
        case Pattern::Five: return scores.five;
        case Pattern::OpenFour: return scores.openFour;
        case Pattern::Four: return scores.four;
        case Pattern::OpenThree: return scores.openThree;
        case Pattern::Three: return scores.three;
        case Pattern::OpenTwo: return scores.openTwo;
        case Pattern::Two: return scores.two;
        case Pattern::None: return 0;
    }
    return 0;
}

double AIEngine::score(const Board& board, Pos pos) const {
    return sideScore(board, pos, self) + sideScore(board, pos, opponent(self)) * defenseWeight;
}

std::optional<Pos> AIEngine::bestPlacement(const Board& board) {
    const Pos center{Board::kSize / 2, Board::kSize / 2};
    std::vector<Pos> best;
    double bestScore = 0.0;
    bool boardEmpty = true;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            const Pos p{x, y};
            if (!board.isEmpty(p)) {
                boardEmpty = false;
                continue;
            }
            const double s = score(board, p);
            if (best.empty() || s > bestScore + kScoreEpsilon) {
                best = {p};
                bestScore = s;
            } else if (s > bestScore - kScoreEpsilon) {
                best.push_back(p);
            }
        }
    }
    if (boardEmpty) {
        return center;  // A12
    }
    if (best.empty()) {
        return std::nullopt;
    }
    std::uniform_int_distribution<std::size_t> pick(0, best.size() - 1);  // A11
    return best[pick(rng)];
}

// 某一方假設下在 pos 時，四個方向的棋型分數總和；兩個以上方向達到活三或更強時加上雙活三分數
double AIEngine::sideScore(const Board& board, Pos pos, PlayerId player) const {
    double total = 0.0;
    int strongDirections = 0;
    for (int d = 0; d < kDirectionCount; ++d) {
        const Pattern pattern = patternAt(board, pos, player, d);
        total += patternScore(pattern, scores);
        if (pattern == Pattern::Five || pattern == Pattern::OpenFour || pattern == Pattern::Four ||
            pattern == Pattern::OpenThree) {
            ++strongDirections;
        }
    }
    if (strongDirections >= 2) {
        total += scores.doubleThreeBonus;
    }
    return total;
}

Pattern AIEngine::patternAt(const Board& board, Pos pos, PlayerId player, int dirIndex) {
    if (!board.inBounds(pos) || !board.isEmpty(pos)) {
        return Pattern::None;
    }
    const Cell own = player == PlayerId::Black ? Cell::Black : Cell::White;
    const Direction d = kDirections[static_cast<std::size_t>(dirIndex)];

    Line line{};
    for (int k = -kReach; k <= kReach; ++k) {
        const Pos p{pos.x + d.dx * k, pos.y + d.dy * k};
        LineCell& cell = line[static_cast<std::size_t>(k + kCenter)];
        if (k == 0) {
            cell = LineCell::Own;
        } else if (!board.inBounds(p)) {
            cell = LineCell::Blocked;
        } else if (board.at(p) == own) {
            cell = LineCell::Own;
        } else if (board.isEmpty(p)) {
            cell = LineCell::Empty;
        } else {
            cell = LineCell::Blocked;
        }
    }
    return twoLevel(line);
}
