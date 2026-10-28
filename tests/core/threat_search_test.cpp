#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <set>
#include <utility>
#include <vector>

#include "board_test_util.h"
#include "core/threat_search.h"

// spec §8.7：連續衝四（黑方 X 進攻，O 是對手）

namespace {

using Cells = std::set<std::pair<int, int>>;

Cells asCells(const std::vector<Pos>& list) {
    Cells cells;
    for (Pos p : list) {
        cells.insert({p.x, p.y});
    }
    return cells;
}

void put(Board& board, Cell cell, std::initializer_list<std::pair<int, int>> cells) {
    for (auto [x, y] : cells) {
        board.set({x, y}, cell);
    }
}

// 衝四後四三：(4,7) 橫向衝四（成五點 (5,7)），同時直向 (4,7)(4,8)(4,9) 成活三；
// 對手擋 (5,7) 後，(4,6) 或 (4,10) 做活四。L = 3
Board fourThenFourThree() {
    Board board;
    put(board, Cell::White, {{0, 7}});
    put(board, Cell::Black, {{1, 7}, {2, 7}, {3, 7}, {4, 8}, {4, 9}});
    return board;
}

// 一步雙四：(4,7) 同時做出橫向與直向的衝四。L = 2
Board doubleFour() {
    Board board;
    put(board, Cell::White, {{0, 7}, {4, 11}});
    put(board, Cell::Black, {{1, 7}, {2, 7}, {3, 7}, {4, 8}, {4, 9}, {4, 10}});
    return board;
}

VcfLimits limits(int maxDepth = 8, int nodeBudget = 100000) {
    return VcfLimits{maxDepth, nodeBudget};
}

}  // namespace

// ---- 成五點 ----

TEST(ThreatSearchTest, A4_FivePointsAtBothEndsOfFour) {
    const Board board = boardFromRows({"", "", "", "", "", "", "", "..XXXX..."});
    EXPECT_EQ(asCells(fivePointsOf(board, PlayerId::Black)), (Cells{{1, 7}, {6, 7}}));
}

TEST(ThreatSearchTest, A4_FivePointInsideBrokenFour) {
    const Board board = boardFromRows({"", "", "", "", "", "", "", "..XX.XX.."});
    EXPECT_EQ(asCells(fivePointsOf(board, PlayerId::Black)), (Cells{{4, 7}}));
}

TEST(ThreatSearchTest, A13_BlockedEndsAreNotFivePoints) {
    for (std::string_view row : {"OXXXX....", "#XXXX....", "XXXX....."}) {
        const Board board = boardFromRows({"", "", "", "", "", "", "", row});
        const Pos expected = row[0] == 'X' ? Pos{4, 7} : Pos{5, 7};
        EXPECT_EQ(asCells(fivePointsOf(board, PlayerId::Black)), (Cells{{expected.x, expected.y}})) << row;
    }
}

TEST(ThreatSearchTest, W2_LongLineCountsAsFive) {
    const Board board = boardFromRows({"", "", "", "", "", "", "", ".XXX.XX.."});  // (4,7) 連成六
    EXPECT_EQ(asCells(fivePointsOf(board, PlayerId::Black)), (Cells{{4, 7}}));
}

TEST(ThreatSearchTest, A4_DiagonalFivePoints) {
    Board board;
    put(board, Cell::Black, {{3, 3}, {4, 4}, {5, 5}, {6, 6}});
    EXPECT_EQ(asCells(fivePointsOf(board, PlayerId::Black)), (Cells{{2, 2}, {7, 7}}));
    EXPECT_TRUE(fivePointsOf(board, PlayerId::White).empty());
}

// ---- 連續衝四 ----

TEST(ThreatSearchTest, Spec87_DoubleFourInOneMove) {
    VcfLimits l = limits();
    const auto lines = findVcf(doubleFour(), PlayerId::Black, {}, l);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0].length, 2);
    EXPECT_EQ(lines[0].moves.front(), (Pos{4, 7}));
}

TEST(ThreatSearchTest, Spec87_FourThenOpenFour) {
    VcfLimits l = limits();
    const auto lines = findVcf(fourThenFourThree(), PlayerId::Black, {}, l);
    ASSERT_EQ(lines.size(), 1u);
    const VcfLine& line = lines[0];
    EXPECT_EQ(line.length, 3);
    ASSERT_EQ(line.moves.size(), 2u);
    EXPECT_EQ(line.moves[0], (Pos{4, 7}));
    EXPECT_EQ(line.fivePoints[0], (Pos{5, 7}));  // 對手被迫擋的位置
    EXPECT_TRUE(line.moves[1] == (Pos{4, 6}) || line.moves[1] == (Pos{4, 10}));
}

TEST(ThreatSearchTest, Spec87_TwoFoursBeforeOpenFour) {
    // (4,2) 橫向衝四（擋 (5,2)）→ (4,5) 直向衝四（擋 (4,6)），同時第 5 列成跳三 → (3,5) 活四。L = 4
    Board board;
    put(board, Cell::White, {{0, 2}, {4, 1}, {3, 4}});  // (3,4)：擋住 (5,2) 開始的斜線
    put(board, Cell::Black, {{1, 2}, {2, 2}, {3, 2}, {4, 3}, {4, 4}, {1, 5}, {2, 5}});
    VcfLimits shallow = limits(3);
    EXPECT_TRUE(findVcf(board, PlayerId::Black, {}, shallow).empty());

    VcfLimits l = limits();
    const auto lines = findVcf(board, PlayerId::Black, {}, l);
    ASSERT_EQ(lines.size(), 1u);
    const VcfLine& line = lines[0];
    EXPECT_EQ(line.length, 4);
    EXPECT_EQ(line.moves, (std::vector<Pos>{{4, 2}, {4, 5}, {3, 5}}));
    ASSERT_EQ(line.fivePoints.size(), 3u);
    EXPECT_EQ(line.fivePoints[0], (Pos{5, 2}));
    EXPECT_EQ(line.fivePoints[1], (Pos{4, 6}));
}

TEST(ThreatSearchTest, Spec87_ImmediateFiveHasLengthOne) {
    const Board board = boardFromRows({"", "", "", "", "", "", "", "..XXXX..."});
    VcfLimits l = limits();
    const auto lines = findVcf(board, PlayerId::Black, {}, l);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0].length, 1);
}

TEST(ThreatSearchTest, Spec87_NothingWithoutFours) {
    const Board board = boardFromRows({"", "", "", "", "", "", "", "...XX...."});
    VcfLimits l = limits();
    EXPECT_TRUE(findVcf(board, PlayerId::Black, {}, l).empty());
}

TEST(ThreatSearchTest, Spec87_ForcedBlockThatMakesOpponentFivePointIsExcluded) {
    Board board = fourThenFourThree();
    put(board, Cell::White, {{5, 8}, {5, 9}, {5, 10}});  // 對手擋在 (5,7) 時直向成四，反過來逼黑方
    VcfLimits l = limits();
    EXPECT_TRUE(findVcf(board, PlayerId::Black, {}, l).empty());
}

TEST(ThreatSearchTest, Spec87_A5a_AvoidsExcludedCells) {
    VcfLimits l = limits();
    EXPECT_TRUE(findVcf(fourThenFourThree(), PlayerId::Black, {{4, 7}}, l).empty());
}

TEST(ThreatSearchTest, Spec87_A5a_ExcludedFivePointIsNotUsable) {
    // 雙四的其中一個成五點在禁區內：剩一個成五點，只是衝四，不算贏
    VcfLimits l = limits();
    const auto lines = findVcf(doubleFour(), PlayerId::Black, {{4, 6}}, l);
    for (const VcfLine& line : lines) {
        EXPECT_NE(line.length, 2);
    }
}

TEST(ThreatSearchTest, Spec87_A13_DestroyedCellBlocks) {
    Board board = fourThenFourThree();
    board.set({5, 7}, Cell::Destroyed);
    VcfLimits l = limits();
    EXPECT_TRUE(findVcf(board, PlayerId::Black, {}, l).empty());
}

TEST(ThreatSearchTest, Spec87_MaxDepthLimitsLength) {
    VcfLimits shallow = limits(2);
    EXPECT_TRUE(findVcf(fourThenFourThree(), PlayerId::Black, {}, shallow).empty());
    VcfLimits enough = limits(3);
    EXPECT_FALSE(findVcf(fourThenFourThree(), PlayerId::Black, {}, enough).empty());
}

TEST(ThreatSearchTest, Spec87_NodeBudgetStopsSearch) {
    VcfLimits tiny = limits(8, 1);
    EXPECT_TRUE(findVcf(fourThenFourThree(), PlayerId::Black, {}, tiny).empty());
    EXPECT_LE(tiny.nodeBudget, 0);

    VcfLimits plenty = limits();
    findVcf(fourThenFourThree(), PlayerId::Black, {}, plenty);
    EXPECT_GT(plenty.nodeBudget, 0);
    EXPECT_LT(plenty.nodeBudget, 100000);  // 每檢查一個盤面就遞減
}

TEST(ThreatSearchTest, Spec87_ReturnsShortestLines) {
    Board board = fourThenFourThree();  // 左下角：L = 3
    put(board, Cell::White, {{14, 0}});
    // 右上角：(10,0) 同時做出橫向衝四與直向衝四（上方是邊界），L = 2
    put(board, Cell::Black, {{13, 0}, {12, 0}, {11, 0}, {10, 1}, {10, 2}, {10, 3}});
    VcfLimits l = limits();
    const auto lines = findVcf(board, PlayerId::Black, {}, l);
    ASSERT_FALSE(lines.empty());
    for (const VcfLine& line : lines) {
        EXPECT_EQ(line.length, 2);
    }
    const bool hasCorner = std::any_of(lines.begin(), lines.end(),
                                       [](const VcfLine& line) { return line.moves.front() == Pos{10, 0}; });
    EXPECT_TRUE(hasCorner);
}

TEST(ThreatSearchTest, Spec87_SearchesForEitherPlayer) {
    Board board;
    put(board, Cell::Black, {{0, 7}});
    put(board, Cell::White, {{1, 7}, {2, 7}, {3, 7}, {4, 8}, {4, 9}});
    VcfLimits l = limits();
    EXPECT_FALSE(findVcf(board, PlayerId::White, {}, l).empty());
    EXPECT_TRUE(findVcf(board, PlayerId::Black, {}, l).empty());
}
