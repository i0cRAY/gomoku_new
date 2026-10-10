#include "core/ai_engine.h"

#include <algorithm>
#include <array>
#include <iterator>
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

AIEngine::AIEngine(PlayerId self, const AIConfig& ai, const SkillConfig& skill, std::uint32_t seed)
    : self(self),
      reactionTime(ai.reactionTime),
      defenseWeight(ai.defenseWeight),
      rng(seed),
      scores(ai.scores),
      skillEnergyCost(skill.energyCost) {}

SkillId AIEngine::chooseSkill() {
    constexpr SkillId kSkills[] = {SkillId::Bomb, SkillId::Dominate, SkillId::Destroy};  // A2a
    std::uniform_int_distribution<std::size_t> pick(0, std::size(kSkills) - 1);
    return kSkills[pick(rng)];
}

void AIEngine::newGame() {
    lastDecision.reset();
}

// 決策優先順序 A4 → A5 → A6 → A7 → A9；任何下子動作在不能下子時都改為本次不行動
std::optional<Action> AIEngine::decide(const PlayerView& view) {
    if (view.status != GameStatus::Running) {
        return std::nullopt;
    }
    if (lastDecision && view.now - *lastDecision < reactionTime) {
        return std::nullopt;  // A2
    }
    lastDecision = view.now;  // 不行動也算一次決策

    const Board& board = view.board;
    const bool placeable = canPlace(view);
    auto place = [&](std::optional<Pos> pos) -> std::optional<Action> {
        if (!pos || !placeable) {
            return std::nullopt;
        }
        return PlaceAction{self, *pos};
    };

    // A4：自己一步成五
    if (const auto own = fivePoints(board, self); !own.empty()) {
        return place(highestScore(board, own));
    }
    // A5：對手有成五點
    if (const auto threats = fivePoints(board, opponent(self)); !threats.empty()) {
        return respondToFivePoints(view, threats);
    }
    // A6：自己能形成活四
    std::vector<Pos> openFours;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            for (int d = 0; d < kDirectionCount; ++d) {
                if (patternAt(board, {x, y}, self, d) == Pattern::OpenFour) {
                    openFours.push_back({x, y});
                    break;
                }
            }
        }
    }
    if (!openFours.empty()) {
        return place(highestScore(board, openFours));
    }
    // A7：對手有活三
    if (const auto block = openThreeBlock(board)) {
        return place(block);
    }
    // A9
    return placeable ? place(bestPlacement(board)) : std::nullopt;
}

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

bool AIEngine::canPlace(const PlayerView& view) const {
    return view.self.energy >= 1 && view.placeCooldownRemaining <= 0;
}

bool AIEngine::skillReady(const PlayerView& view, SkillId skill) const {
    return view.self.skill == skill && view.self.energy >= skillEnergyCost;  // S3：沒有冷卻，只看能量
}

std::vector<Pos> AIEngine::fivePoints(const Board& board, PlayerId player) const {
    std::vector<Pos> points;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            for (int d = 0; d < kDirectionCount; ++d) {
                if (patternAt(board, {x, y}, player, d) == Pattern::Five) {
                    points.push_back({x, y});
                    break;
                }
            }
        }
    }
    return points;
}

bool AIEngine::Threat::operator<(const Threat& other) const {
    if (fivePoints != other.fivePoints) {
        return fivePoints < other.fivePoints;
    }
    return bestAttack < other.bestAttack - kScoreEpsilon;
}

AIEngine::Threat AIEngine::threatOf(const Board& board) const {
    const PlayerId enemy = opponent(self);
    Threat threat{static_cast<int>(fivePoints(board, enemy).size()), 0.0};
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            if (board.isEmpty({x, y})) {
                threat.bestAttack = std::max(threat.bestAttack, sideScore(board, {x, y}, enemy));
            }
        }
    }
    return threat;
}

// A5：1 個成五點且能下子 → 擋；擋不完或不能下子時，炸彈好了就炸；擋不完又沒有炸彈時擋威脅分降最多的點
std::optional<Action> AIEngine::respondToFivePoints(const PlayerView& view, const std::vector<Pos>& points) {
    const Board& board = view.board;
    const bool placeable = canPlace(view);
    if (points.size() == 1 && placeable) {
        return PlaceAction{self, points.front()};
    }
    if (skillReady(view, SkillId::Bomb)) {
        if (const auto target = bestBombTarget(board, points)) {
            return SkillAction{self, SkillId::Bomb, *target};
        }
    }
    if (!placeable) {
        return std::nullopt;
    }
    std::optional<Pos> best;
    Threat bestThreat{};
    double bestOwnScore = 0.0;
    for (Pos p : points) {
        Board after = board;
        after.set(p, self == PlayerId::Black ? Cell::Black : Cell::White);
        const Threat threat = threatOf(after);
        const double own = score(board, p);
        const bool better = !best || threat < bestThreat ||
                            (!(bestThreat < threat) && own > bestOwnScore + kScoreEpsilon);  // 同分選對自己較好的
        if (better) {
            best = p;
            bestThreat = threat;
            bestOwnScore = own;
        }
    }
    return PlaceAction{self, *best};
}

// 候選：與成五點在同一條線上、能和它共同構成連五的對手棋子；選炸掉後威脅分降最多的那顆
std::optional<Pos> AIEngine::bestBombTarget(const Board& board, const std::vector<Pos>& points) const {
    const PlayerId enemy = opponent(self);
    const Cell enemyStone = enemy == PlayerId::Black ? Cell::Black : Cell::White;
    std::vector<Pos> candidates;
    for (Pos point : points) {
        for (int d = 0; d < kDirectionCount; ++d) {
            if (patternAt(board, point, enemy, d) != Pattern::Five) {
                continue;
            }
            const Direction dir = kDirections[static_cast<std::size_t>(d)];
            for (int sign : {1, -1}) {
                Pos p{point.x + dir.dx * sign, point.y + dir.dy * sign};
                while (board.inBounds(p) && board.at(p) == enemyStone) {
                    if (std::find(candidates.begin(), candidates.end(), p) == candidates.end()) {
                        candidates.push_back(p);
                    }
                    p = Pos{p.x + dir.dx * sign, p.y + dir.dy * sign};
                }
            }
        }
    }

    std::optional<Pos> best;
    Threat bestThreat{};
    for (Pos c : candidates) {
        Board after = board;
        after.set(c, Cell::Empty);
        const Threat threat = threatOf(after);
        if (!best || threat < bestThreat) {
            best = c;
            bestThreat = threat;
        }
    }
    return best;
}

// A7：對手的「活四點」（下了會成活四的空格）代表一個活三。同一條線上的活四點視為同一個活三，
// 候選擋點是下了之後讓該線所有活四點都失效的空格（兩端端點、跳三中間的空格），
// 全部活三的候選一起比較，選對自己評分最高的一格
std::optional<Pos> AIEngine::openThreeBlock(const Board& board) const {
    const PlayerId enemy = opponent(self);
    const Cell ownStone = self == PlayerId::Black ? Cell::Black : Cell::White;

    struct Group {
        int dir;
        int lineKey;
        std::vector<Pos> points;
    };
    std::vector<Group> groups;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            for (int d = 0; d < kDirectionCount; ++d) {
                if (patternAt(board, {x, y}, enemy, d) != Pattern::OpenFour) {
                    continue;
                }
                const Direction dir = kDirections[static_cast<std::size_t>(d)];
                // 同一條線上的格子有相同的 key：橫 y、直 x、右下斜 x − y、右上斜 x + y
                const int key = dir.dy == 0 ? y : dir.dx == 0 ? x : dir.dy > 0 ? x - y : x + y;
                auto it = std::find_if(groups.begin(), groups.end(),
                                       [&](const Group& g) { return g.dir == d && g.lineKey == key; });
                if (it == groups.end()) {
                    groups.push_back({d, key, {}});
                    it = groups.end() - 1;
                }
                it->points.push_back({x, y});
            }
        }
    }

    std::vector<Pos> candidates;
    for (const Group& g : groups) {
        const Direction dir = kDirections[static_cast<std::size_t>(g.dir)];
        for (Pos point : g.points) {
            for (int k = -kReach; k <= kReach; ++k) {
                const Pos c{point.x + dir.dx * k, point.y + dir.dy * k};
                if (!board.inBounds(c) || !board.isEmpty(c) ||
                    std::find(candidates.begin(), candidates.end(), c) != candidates.end()) {
                    continue;
                }
                Board after = board;
                after.set(c, ownStone);
                const bool blocksAll = std::none_of(g.points.begin(), g.points.end(), [&](Pos e) {
                    return patternAt(after, e, enemy, g.dir) == Pattern::OpenFour;
                });
                if (blocksAll) {
                    candidates.push_back(c);
                }
            }
        }
    }
    return highestScore(board, candidates);
}

std::optional<Pos> AIEngine::highestScore(const Board& board, const std::vector<Pos>& candidates) const {
    std::optional<Pos> best;
    double bestScore = 0.0;
    for (Pos p : candidates) {
        const double s = score(board, p);
        if (!best || s > bestScore + kScoreEpsilon) {
            best = p;
            bestScore = s;
        }
    }
    return best;
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
    if (std::count(line.begin(), line.end(), LineCell::Own) == 1) {
        return Pattern::None;  // 只有中心一子：最多是「一子」，不算棋型（加速全盤掃描）
    }
    return twoLevel(line);
}
