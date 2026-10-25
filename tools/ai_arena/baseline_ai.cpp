#include "baseline_ai.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>
#include <vector>

namespace baseline {

namespace {

struct Direction {
    int dx;
    int dy;
};

constexpr std::array<Direction, BaselineAI::kDirectionCount> kDirections{{{1, 0}, {0, 1}, {1, 1}, {1, -1}}};

constexpr int kWinLength = 5;
constexpr int kReach = kWinLength - 1;           // 連五只會用到中心左右各 4 格
constexpr int kLineLength = 2 * kReach + 1;
constexpr int kCenter = kReach;
constexpr double kScoreEpsilon = 1e-9;  // 比較同分用
constexpr int kPlaceEnergy = 1;         // spec §5：每子消耗

bool contains(const std::vector<Pos>& cells, Pos p) {
    return std::find(cells.begin(), cells.end(), p) != cells.end();
}

std::vector<Pos> without(std::vector<Pos> cells, const std::vector<Pos>& excluded) {
    cells.erase(std::remove_if(cells.begin(), cells.end(), [&](Pos p) { return contains(excluded, p); }),
                cells.end());
    return cells;
}

bool isStone(Cell c) {
    return c == Cell::Black || c == Cell::White;
}

Cell stoneOf(PlayerId player) {
    return player == PlayerId::Black ? Cell::Black : Cell::White;
}

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

BaselineAI::BaselineAI(PlayerId self, const AIConfig& ai, const SkillConfig& skill, std::uint32_t seed)
    : self(self),
      reactionTime(ai.reactionTime),
      defenseWeight(ai.defenseWeight),
      rng(seed),
      scores(ai.scores),
      skills(skill),
      destroyMinGain(ai.destroyMinGain),
      useSkills(ai.useSkills) {}

SkillId BaselineAI::chooseSkill() {
    constexpr SkillId kSkills[] = {SkillId::Dominate, SkillId::Destroy};  // A2a；炸彈暫停使用（S1b）
    std::uniform_int_distribution<std::size_t> pick(0, std::size(kSkills) - 1);
    return kSkills[pick(rng)];
}

void BaselineAI::newGame() {
    lastDecision.reset();
}

// 決策優先順序 A4 → A5 → A8a → A6 → A7 → A8b → A9（含 A9a）；任何下子動作在不能下子時都改為本次不行動。
// 所有「下在某格」的候選都先排除對手的禁區（A5a）
std::optional<Action> BaselineAI::decide(const PlayerView& view) {
    if (view.status != GameStatus::Running) {
        return std::nullopt;
    }
    if (lastDecision && view.now - *lastDecision < reactionTime) {
        return std::nullopt;  // A2
    }
    lastDecision = view.now;  // 不行動也算一次決策

    const Board& board = view.board;
    const std::vector<Pos> zones = opponentZones(view);
    const bool placeable = canPlace(view);
    auto place = [&](std::optional<Pos> pos) -> std::optional<Action> {
        if (!pos || !placeable) {
            return std::nullopt;
        }
        return PlaceAction{self, *pos};
    };

    // A4：自己一步成五
    if (const auto own = without(fivePoints(board, self), zones); !own.empty()) {
        return place(highestScore(board, own));
    }
    // A5：對手有成五點（成五點在禁區內也算威脅，只是擋不了）
    if (const auto threats = fivePoints(board, opponent(self)); !threats.empty()) {
        return respondToFivePoints(view, threats, zones);
    }
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
    const std::vector<Pos> fours = without(openFours, zones);
    // A8a：自己有活三以上時先開霸道，之後做活四的那一子就會產生禁區（技能不受下子間隔影響，S5）
    if (!fours.empty() && view.self.dominateCharges == 0 && skillReady(view, SkillId::Dominate)) {
        return SkillAction{self, SkillId::Dominate, std::nullopt};
    }
    // A6：自己能形成活四
    if (!fours.empty()) {
        return place(highestScore(board, fours));
    }
    // A7：對手有活三 → 擋（不用炸彈）
    if (const auto block = openThreeBlock(board, zones)) {
        return place(block);
    }
    // A8b：差值夠大時主動摧毀
    if (skillReadyWithSpare(view, SkillId::Destroy)) {
        if (const auto area = bestDestroyArea(board, std::nullopt); area && area->gain >= destroyMinGain) {
            return SkillAction{self, SkillId::Destroy, area->target};
        }
    }
    // A9、A9a
    if (!placeable || !keepsReserve(view)) {
        return std::nullopt;
    }
    return place(bestPlacement(board, zones));
}

int BaselineAI::patternScore(Pattern pattern, const PatternScores& scores) {
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

double BaselineAI::score(const Board& board, Pos pos) const {
    return sideScore(board, pos, self) + sideScore(board, pos, opponent(self)) * defenseWeight;
}

std::optional<Pos> BaselineAI::bestPlacement(const Board& board, const std::vector<Pos>& excluded) {
    const Pos center{Board::kSize / 2, Board::kSize / 2};
    std::vector<Pos> best;
    double bestScore = 0.0;
    bool noStones = true;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            const Pos p{x, y};
            noStones = noStones && !isStone(board.at(p));
            if (!board.isEmpty(p) || contains(excluded, p)) {
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
    if (noStones && board.isEmpty(center) && !contains(excluded, center)) {
        return center;  // A12：中央已摧毀或在禁區內時改用評分
    }
    if (best.empty()) {
        return std::nullopt;
    }
    std::uniform_int_distribution<std::size_t> pick(0, best.size() - 1);  // A11
    return best[pick(rng)];
}

bool BaselineAI::canPlace(const PlayerView& view) const {
    return view.self.energy >= 1 && view.placeCooldownRemaining <= 0;
}

bool BaselineAI::skillReady(const PlayerView& view, SkillId skill) const {
    if (!useSkills) {
        return false;
    }
    if (skill == SkillId::Destroy && view.self.destroyUsed) {
        return false;  // SX3
    }
    return view.self.skill == skill && view.self.energy >= skills.costOf(skill);  // S3：沒有冷卻，只看能量
}

bool BaselineAI::skillReadyWithSpare(const PlayerView& view, SkillId skill) const {
    return skillReady(view, skill) && view.self.energy >= skills.costOf(skill) + kPlaceEnergy;
}

// A9a：下子後能量仍 ≥ 自己技能的消耗；摧毀已用過或不用技能時沒有技能要存能量
bool BaselineAI::keepsReserve(const PlayerView& view) const {
    if (!useSkills || (view.self.skill == SkillId::Destroy && view.self.destroyUsed)) {
        return true;
    }
    return view.self.energy - kPlaceEnergy >= skills.costOf(view.self.skill);
}

std::vector<Pos> BaselineAI::opponentZones(const PlayerView& view) const {
    std::vector<Pos> cells;
    for (const ZoneCell& zone : view.zones) {
        if (zone.owner != self && view.now < zone.expiresAt) {  // SZ3：自己的禁區不限制自己
            cells.push_back(zone.pos);
        }
    }
    return cells;
}

std::vector<Pos> BaselineAI::fivePoints(const Board& board, PlayerId player) const {
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

bool BaselineAI::Threat::operator<(const Threat& other) const {
    if (fivePoints != other.fivePoints) {
        return fivePoints < other.fivePoints;
    }
    return bestAttack < other.bestAttack - kScoreEpsilon;
}

BaselineAI::Threat BaselineAI::threatOf(const Board& board) const {
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

// A5：1 個成五點且能擋 → 擋（不用炸彈）。擋不完、不能下子或成五點都在禁區內時，依序考慮炸彈、摧毀；
// 都不能用時，能擋就擋威脅分降最多的點，否則本次不行動
std::optional<Action> BaselineAI::respondToFivePoints(const PlayerView& view, const std::vector<Pos>& points,
                                                    const std::vector<Pos>& zones) {
    const Board& board = view.board;
    const bool placeable = canPlace(view);
    const std::vector<Pos> blockable = without(points, zones);  // A5a
    const std::vector<Pos> stones = threatStones(board, points);
    if (points.size() == 1 && placeable && !blockable.empty()) {
        return PlaceAction{self, blockable.front()};
    }
    if (skillReady(view, SkillId::Bomb)) {
        if (const auto target = bestBombTarget(board, stones)) {
            return SkillAction{self, SkillId::Bomb, *target};
        }
    }
    if (skillReady(view, SkillId::Destroy)) {
        if (const auto area = bestDestroyArea(board, stones)) {
            return SkillAction{self, SkillId::Destroy, area->target};
        }
    }
    if (!placeable || blockable.empty()) {
        return std::nullopt;
    }
    std::optional<Pos> best;
    Threat bestThreat{};
    double bestOwnScore = 0.0;
    for (Pos p : blockable) {
        Board after = board;
        after.set(p, stoneOf(self));
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

// 與成五點在同一條線上、能和它共同構成連五的對手棋子
std::vector<Pos> BaselineAI::threatStones(const Board& board, const std::vector<Pos>& points) const {
    const PlayerId enemy = opponent(self);
    const Cell enemyStone = stoneOf(enemy);
    std::vector<Pos> stones;
    for (Pos point : points) {
        for (int d = 0; d < kDirectionCount; ++d) {
            if (patternAt(board, point, enemy, d) != Pattern::Five) {
                continue;
            }
            const Direction dir = kDirections[static_cast<std::size_t>(d)];
            for (int sign : {1, -1}) {
                Pos p{point.x + dir.dx * sign, point.y + dir.dy * sign};
                while (board.inBounds(p) && board.at(p) == enemyStone) {
                    if (!contains(stones, p)) {
                        stones.push_back(p);
                    }
                    p = Pos{p.x + dir.dx * sign, p.y + dir.dy * sign};
                }
            }
        }
    }
    return stones;
}

// A5c：以目標為左上角的 2×2（SB3）至少涵蓋 stones（威脅棋子）其中一顆；依序比較炸完後的威脅分、
// 清掉的己方棋子數，都同分時隨機挑（A11）
std::optional<Pos> BaselineAI::bestBombTarget(const Board& board, const std::vector<Pos>& stones) {
    const Cell ownStone = stoneOf(self);
    const int size = skills.bombSize;
    auto inArea = [&](Pos topLeft, Pos p) {
        return p.x >= topLeft.x && p.x < topLeft.x + size && p.y >= topLeft.y && p.y < topLeft.y + size;
    };

    std::vector<Pos> best;
    Threat bestThreat{};
    int bestOwnLost = 0;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            const Pos topLeft{x, y};
            if (std::none_of(stones.begin(), stones.end(), [&](Pos s) { return inArea(topLeft, s); })) {
                continue;
            }
            Board after = board;
            int ownLost = 0;
            for (int dy = 0; dy < size; ++dy) {
                for (int dx = 0; dx < size; ++dx) {
                    const Pos p{x + dx, y + dy};
                    if (!board.inBounds(p) || !isStone(board.at(p))) {
                        continue;
                    }
                    ownLost += board.at(p) == ownStone ? 1 : 0;
                    after.set(p, Cell::Empty);
                }
            }
            const Threat threat = threatOf(after);
            const bool better = best.empty() || threat < bestThreat ||
                                (!(bestThreat < threat) && ownLost < bestOwnLost);
            const bool tie = !best.empty() && !(threat < bestThreat) && !(bestThreat < threat) && ownLost == bestOwnLost;
            if (better) {
                best = {topLeft};
                bestThreat = threat;
                bestOwnLost = ownLost;
            } else if (tie) {
                best.push_back(topLeft);
            }
        }
    }
    if (best.empty()) {
        return std::nullopt;
    }
    std::uniform_int_distribution<std::size_t> pick(0, best.size() - 1);
    return best[pick(rng)];
}

// A5b、A8b：「範圍內對手棋子數 − 己方棋子數」最大的中心點；mustCover 有值時範圍至少要涵蓋其中一顆
// （A5b 的威脅棋子）；同分隨機挑（A11）
std::optional<BaselineAI::AreaChoice> BaselineAI::bestDestroyArea(const Board& board,
                                                              const std::optional<std::vector<Pos>>& mustCover) {
    const Cell ownStone = stoneOf(self);
    const Cell enemyStone = stoneOf(opponent(self));
    auto inArea = [&](Pos center, Pos p) {
        return std::abs(p.x - center.x) <= skills.destroyRadius && std::abs(p.y - center.y) <= skills.destroyRadius;
    };

    std::vector<Pos> best;
    int bestValue = 0;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            const Pos center{x, y};
            if (mustCover &&
                std::none_of(mustCover->begin(), mustCover->end(), [&](Pos s) { return inArea(center, s); })) {
                continue;
            }
            int value = 0;
            for (int dy = -skills.destroyRadius; dy <= skills.destroyRadius; ++dy) {
                for (int dx = -skills.destroyRadius; dx <= skills.destroyRadius; ++dx) {
                    const Pos p{x + dx, y + dy};
                    if (!board.inBounds(p)) {
                        continue;
                    }
                    value += board.at(p) == enemyStone ? 1 : board.at(p) == ownStone ? -1 : 0;
                }
            }
            if (best.empty() || value > bestValue) {
                best = {center};
                bestValue = value;
            } else if (value == bestValue) {
                best.push_back(center);
            }
        }
    }
    if (best.empty()) {
        return std::nullopt;
    }
    std::uniform_int_distribution<std::size_t> pick(0, best.size() - 1);
    return AreaChoice{best[pick(rng)], bestValue};
}

// A7：對手的「活四點」（下了會成活四的空格）代表一個活三。同一條線上的活四點視為同一個活三，
// 候選擋點是下了之後讓該線所有活四點都失效的空格（兩端端點、跳三中間的空格），
// 全部活三的候選一起比較，選對自己評分最高的一格；對手禁區內的格子不當候選（A5a）
std::optional<Pos> BaselineAI::openThreeBlock(const Board& board, const std::vector<Pos>& zones) const {
    const PlayerId enemy = opponent(self);
    const Cell ownStone = stoneOf(self);

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
                if (!board.inBounds(c) || !board.isEmpty(c) || contains(zones, c) || contains(candidates, c)) {
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

std::optional<Pos> BaselineAI::highestScore(const Board& board, const std::vector<Pos>& candidates) const {
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
double BaselineAI::sideScore(const Board& board, Pos pos, PlayerId player) const {
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

Pattern BaselineAI::patternAt(const Board& board, Pos pos, PlayerId player, int dirIndex) {
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

AIConfig frozenConfig(bool useSkills) {
    AIConfig config;
    config.reactionTime = 350;
    config.defenseWeight = 0.8;
    config.destroyMinGain = 3;
    config.useSkills = useSkills;
    config.scores.five = 100000;
    config.scores.openFour = 10000;
    config.scores.doubleThreeBonus = 8000;
    config.scores.four = 1000;
    config.scores.openThree = 1000;
    config.scores.three = 100;
    config.scores.openTwo = 100;
    config.scores.two = 10;
    return config;
}

}  // namespace baseline
