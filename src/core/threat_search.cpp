#include "core/threat_search.h"

#include <algorithm>
#include <array>
#include <optional>

namespace {

constexpr int kWindow = 5;  // spec W1：連成五子

struct Direction {
    int dx;
    int dy;
};

constexpr std::array<Direction, 4> kDirections{{{1, 0}, {0, 1}, {1, 1}, {1, -1}}};

Cell stoneOf(PlayerId p) {
    return p == PlayerId::Black ? Cell::Black : Cell::White;
}

bool contains(const std::vector<Pos>& cells, Pos p) {
    return std::find(cells.begin(), cells.end(), p) != cells.end();
}

void addUnique(std::vector<Pos>& cells, Pos p) {
    if (!contains(cells, p)) {
        cells.push_back(p);
    }
}

using Window = std::array<Pos, kWindow>;

bool inBoard(Pos p) {
    return p.x >= 0 && p.x < Board::kSize && p.y >= 0 && p.y < Board::kSize;
}

// 棋盤內所有的五格窗口，只算一次（每個盤面都要看全部的窗口）
const std::vector<Window>& allWindows() {
    static const std::vector<Window> windows = [] {
        std::vector<Window> result;
        for (int y = 0; y < Board::kSize; ++y) {
            for (int x = 0; x < Board::kSize; ++x) {
                for (const Direction& d : kDirections) {
                    if (!inBoard({x + d.dx * (kWindow - 1), y + d.dy * (kWindow - 1)})) {
                        continue;
                    }
                    Window cells{};
                    for (int i = 0; i < kWindow; ++i) {
                        cells[static_cast<std::size_t>(i)] = Pos{x + d.dx * i, y + d.dy * i};
                    }
                    result.push_back(cells);
                }
            }
        }
        return result;
    }();
    return windows;
}

template <typename Visit>
void forEachWindow(Visit visit) {
    for (const Window& cells : allWindows()) {
        visit(cells);
    }
}

// 窗口內的空格（最多 5 個），放在固定大小的陣列裡，不配置記憶體（每個盤面要看幾百個窗口）
struct Empties {
    std::array<Pos, kWindow> cells{};
    int count = 0;
    const Pos* begin() const { return cells.data(); }
    const Pos* end() const { return cells.data() + count; }
};

// 窗口內 player 有 own 顆子、其餘都是空格時，回傳那些空格；有對手子或已摧毀格時回傳空
Empties emptiesIfOnly(const Board& board, const Window& cells, Cell stone, int own) {
    Empties empties;
    int count = 0;
    for (Pos p : cells) {
        const Cell c = board.at(p);
        if (c == stone) {
            ++count;
        } else if (c == Cell::Empty) {
            empties.cells[static_cast<std::size_t>(empties.count++)] = p;
        } else {
            return {};
        }
    }
    if (count != own) {
        return {};
    }
    return empties;
}

std::vector<Pos> usableFivePoints(const Board& board, PlayerId player, const std::vector<Pos>& excluded) {
    std::vector<Pos> points;
    for (Pos p : fivePointsOf(board, player)) {
        if (!contains(excluded, p)) {
            points.push_back(p);
        }
    }
    return points;
}

// 經過 p 的五格窗口（4 個方向 × 5 個位置，棋盤內最多 20 個）。下一子只會改變經過它的窗口
template <typename Visit>
void forEachWindowThrough(Pos p, Visit visit) {
    Window cells{};
    for (const Direction& d : kDirections) {
        for (int offset = 0; offset < kWindow; ++offset) {
            const Pos first{p.x - d.dx * offset, p.y - d.dy * offset};
            if (!inBoard(first) || !inBoard({first.x + d.dx * (kWindow - 1), first.y + d.dy * (kWindow - 1)})) {
                continue;
            }
            for (int i = 0; i < kWindow; ++i) {
                cells[static_cast<std::size_t>(i)] = Pos{first.x + d.dx * i, first.y + d.dy * i};
            }
            visit(cells);
        }
    }
}

// 下完 p 之後新出現的成五點：只要看經過 p 的窗口，不必掃描整個棋盤
std::vector<Pos> fivePointsThrough(const Board& board, PlayerId player, Pos p, const std::vector<Pos>& excluded) {
    const Cell stone = stoneOf(player);
    std::vector<Pos> points;
    forEachWindowThrough(p, [&](const Window& cells) {
        for (Pos empty : emptiesIfOnly(board, cells, stone, kWindow - 1)) {
            if (!contains(excluded, empty)) {
                addUnique(points, empty);
            }
        }
    });
    return points;
}

std::size_t indexOf(Pos p) {
    return static_cast<std::size_t>(p.y * Board::kSize + p.x);
}

// 搜尋中的盤面，同時記錄每個空格在幾個「player 3 子 + 2 空格」的窗口裡：
// 次數 > 0 的格子就是下一子後至少多一個成五點的候選（衝四的下法）。
// 下子、撤子時只更新經過該格的窗口，不必每個盤面都掃描整個棋盤
class SearchBoard {
public:
    SearchBoard(const Board& start, PlayerId player) : board(start), stone(stoneOf(player)) {
        forEachWindow([&](const Window& cells) { count(cells, 1); });
    }

    const Board& get() const { return board; }

    void set(Pos p, Cell c) {
        forEachWindowThrough(p, [&](const Window& cells) { count(cells, -1); });
        board.set(p, c);
        forEachWindowThrough(p, [&](const Window& cells) { count(cells, 1); });
    }

    std::vector<Pos> fourMoves(const std::vector<Pos>& excluded) const {
        std::vector<Pos> moves;
        for (int y = 0; y < Board::kSize; ++y) {
            for (int x = 0; x < Board::kSize; ++x) {
                const Pos p{x, y};
                if (fourWindows[indexOf(p)] > 0 && !contains(excluded, p)) {
                    moves.push_back(p);
                }
            }
        }
        return moves;
    }

private:
    void count(const Window& cells, int delta) {
        for (Pos p : emptiesIfOnly(board, cells, stone, kWindow - 2)) {
            fourWindows[indexOf(p)] += delta;
        }
    }

    Board board;
    Cell stone;
    std::array<int, Board::kSize * Board::kSize> fourWindows{};
};

class Search {
public:
    Search(const Board& start, PlayerId player, const std::vector<Pos>& excluded, VcfLimits& limits)
        : board(start, player), player(player), other(opponent(player)), excluded(excluded), limits(limits) {}

    std::vector<Pos> fourMoves() const { return board.fourMoves(excluded); }

    // 從目前的盤面起，最多再用 budget 子（含成五那一子）能不能連續衝四成五；能的話把路線接在 line 後面。
    // 呼叫前自己沒有能用的成五點（根節點由 findVcf 先檢查；之後對手剛擋掉唯一的那一個）
    bool winWithin(int budget, VcfLine& line) {
        if (budget < 2) {
            return false;
        }
        for (Pos move : fourMoves()) {
            if (const auto result = tryMove(move, budget, line)) {
                return *result;
            }
        }
        return false;
    }

    // 試下 move：成功回傳 true；這一子不行回傳 nullopt；搜尋上限用完回傳 false（整個搜尋停止）
    std::optional<bool> tryMove(Pos move, int budget, VcfLine& line) {
        if (--limits.nodeBudget < 0) {
            return false;
        }
        board.set(move, stoneOf(player));
        const std::vector<Pos> points = fivePointsThrough(board.get(), player, move, excluded);
        std::optional<bool> result;
        if (points.size() >= 2) {
            line.moves.push_back(move);
            line.fivePoints.push_back(points.front());
            result = true;  // 活四或雙四：對手擋不完
        } else if (points.size() == 1 && budget >= 3) {
            const Pos forced = points.front();
            board.set(forced, stoneOf(other));  // 對手被迫擋
            if (fivePointsThrough(board.get(), other, forced, {}).empty()) {  // 擋完反過來有成五點的路線不算
                line.moves.push_back(move);
                line.fivePoints.push_back(forced);
                if (winWithin(budget - 1, line)) {
                    result = true;
                } else {
                    line.moves.pop_back();
                    line.fivePoints.pop_back();
                }
            }
            board.set(forced, Cell::Empty);
        }
        board.set(move, Cell::Empty);
        if (!result && limits.nodeBudget < 0) {
            return false;
        }
        return result;
    }

    bool exhausted() const { return limits.nodeBudget < 0; }

private:
    SearchBoard board;
    PlayerId player;
    PlayerId other;
    const std::vector<Pos>& excluded;
    VcfLimits& limits;
};

}  // namespace

std::vector<Pos> fivePointsOf(const Board& board, PlayerId player) {
    const Cell stone = stoneOf(player);
    std::vector<Pos> points;
    forEachWindow([&](const Window& cells) {
        for (Pos p : emptiesIfOnly(board, cells, stone, kWindow - 1)) {
            addUnique(points, p);
        }
    });
    return points;
}

std::vector<VcfLine> findVcf(const Board& board, PlayerId player, const std::vector<Pos>& excluded,
                             VcfLimits& limits) {
    if (const auto points = usableFivePoints(board, player, excluded); !points.empty()) {
        VcfLine line;
        line.fivePoints = {points.front()};
        line.length = 1;
        return {line};
    }
    Search search(board, player, excluded, limits);
    const std::vector<Pos> firstMoves = search.fourMoves();
    // 由短到長逐一加深，找到的第一個長度就是最短的；同一長度收集所有成功的第一子
    for (int length = 2; length <= limits.maxDepth; ++length) {
        std::vector<VcfLine> found;
        for (Pos move : firstMoves) {
            VcfLine line;
            const auto result = search.tryMove(move, length, line);
            if (result && *result) {
                line.length = length;
                found.push_back(line);
            }
            if (search.exhausted()) {
                return found;
            }
        }
        if (!found.empty()) {
            return found;
        }
    }
    return {};
}
