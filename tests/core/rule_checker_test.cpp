#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "board_test_util.h"
#include "core/rule_checker.h"

namespace {

// 從 start 起沿 (dx, dy) 方向放 n 顆 cell，回傳放下的座標
std::vector<Pos> placeLine(Board& board, Pos start, int dx, int dy, int n, Cell cell) {
    std::vector<Pos> placed;
    for (int i = 0; i < n; ++i) {
        const Pos p{start.x + dx * i, start.y + dy * i};
        board.set(p, cell);
        placed.push_back(p);
    }
    return placed;
}

std::vector<Pos> sorted(std::vector<Pos> v) {
    std::sort(v.begin(), v.end(), [](Pos a, Pos b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return v;
}

constexpr int kLen = 5;  // spec W1

// 只形成一條連線，且內容與 expected 相同
void expectWinLine(const Board& board, Pos last, const std::vector<Pos>& expected) {
    const auto lines = RuleChecker::findLines(board, last, kLen);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(sorted(lines[0]), sorted(expected));
}

}  // namespace

TEST(RuleCheckerTest, W1_HorizontalFive) {
    Board board;
    const auto stones = placeLine(board, {2, 3}, 1, 0, 5, Cell::Black);
    expectWinLine(board, {4, 3}, stones);
}

TEST(RuleCheckerTest, W1_VerticalFive) {
    Board board;
    const auto stones = placeLine(board, {6, 1}, 0, 1, 5, Cell::Black);
    expectWinLine(board, {6, 5}, stones);
}

TEST(RuleCheckerTest, W1_DiagonalDownRightFive) {
    Board board;
    const auto stones = placeLine(board, {3, 3}, 1, 1, 5, Cell::White);
    expectWinLine(board, {3, 3}, stones);
}

TEST(RuleCheckerTest, W1_DiagonalUpRightFive) {
    Board board;
    const auto stones = placeLine(board, {2, 12}, 1, -1, 5, Cell::White);
    expectWinLine(board, {4, 10}, stones);
}

TEST(RuleCheckerTest, W1_FiveTouchingBoardEdges) {
    Board corner;
    const auto bottomRow = placeLine(corner, {10, 14}, 1, 0, 5, Cell::Black);
    expectWinLine(corner, {14, 14}, bottomRow);

    Board leftColumn;
    const auto column = placeLine(leftColumn, {0, 0}, 0, 1, 5, Cell::Black);
    expectWinLine(leftColumn, {0, 2}, column);

    Board antiDiagonal;
    const auto diagonal = placeLine(antiDiagonal, {14, 0}, -1, 1, 5, Cell::White);
    expectWinLine(antiDiagonal, {10, 4}, diagonal);
}

TEST(RuleCheckerTest, W1_FourInARowIsNotAWin) {
    Board board;
    placeLine(board, {2, 3}, 1, 0, 4, Cell::Black);
    EXPECT_TRUE(RuleChecker::findLines(board, {5, 3}, kLen).empty());
}

TEST(RuleCheckerTest, W1_GapBreaksTheLine) {
    const Board gap = boardFromRows({"XX.XX"});
    EXPECT_TRUE(RuleChecker::findLines(gap, {1, 0}, kLen).empty());

    const Board blocked = boardFromRows({"XXOXXX"});
    EXPECT_TRUE(RuleChecker::findLines(blocked, {4, 0}, kLen).empty());
}

TEST(RuleCheckerTest, W1_OnlyLinesThroughLastStoneCount) {
    Board board;
    placeLine(board, {0, 0}, 1, 0, 5, Cell::Black);
    board.set({7, 7}, Cell::Black);
    EXPECT_TRUE(RuleChecker::findLines(board, {7, 7}, kLen).empty());
}

TEST(RuleCheckerTest, W1_OpponentStonesDoNotCount) {
    const Board board = boardFromRows({"XXOOOOO"});
    expectWinLine(board, {4, 0}, {{2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}});
    EXPECT_TRUE(RuleChecker::findLines(board, {1, 0}, kLen).empty());
}

TEST(RuleCheckerTest, W1_EmptyLastCellIsNotAWin) {
    const Board board;
    EXPECT_TRUE(RuleChecker::findLines(board, {7, 7}, kLen).empty());
}

TEST(RuleCheckerTest, W2_SixInARowWinsWithWholeLine) {
    Board board;
    const auto stones = placeLine(board, {1, 8}, 1, 0, 6, Cell::Black);
    expectWinLine(board, {1, 8}, stones);
}

TEST(RuleCheckerTest, W2_SixStonesFormOneLineOfLengthSix) {
    Board board;
    placeLine(board, {1, 8}, 1, 0, 6, Cell::Black);
    const auto lines = RuleChecker::findLines(board, {3, 8}, kLen);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].size(), 6u);
}

TEST(RuleCheckerTest, W6_FivesInTwoDirectionsAreSeparateLines) {
    Board board;
    const auto horizontal = placeLine(board, {3, 7}, 1, 0, 5, Cell::Black);  // (3..7, 7)
    const auto vertical = placeLine(board, {7, 3}, 0, 1, 5, Cell::Black);    // (7, 3..7)
    auto lines = RuleChecker::findLines(board, {7, 7}, kLen);
    ASSERT_EQ(lines.size(), 2u);
    for (auto& line : lines) {
        line = sorted(line);
    }
    std::sort(lines.begin(), lines.end(), [](const auto& a, const auto& b) { return a[0].x > b[0].x; });
    EXPECT_EQ(lines[0], sorted(vertical));    // 交叉點 (7, 7) 在兩條線裡都出現
    EXPECT_EQ(lines[1], sorted(horizontal));
}

TEST(RuleCheckerTest, B4_DestroyedCellBreaksTheLine) {
    const Board board = boardFromRows({"XX#XXX"});
    EXPECT_TRUE(RuleChecker::findLines(board, {4, 0}, kLen).empty());
    const Board edge = boardFromRows({"#XXXXX#"});
    expectWinLine(edge, {3, 0}, {{1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}});
}

TEST(RuleCheckerTest, B4_DestroyedLastCellHasNoLine) {
    const Board board = boardFromRows({"#"});
    EXPECT_TRUE(RuleChecker::findLines(board, {0, 0}, kLen).empty());
}

// ---- 盤面分（spec W11）：只看剛好連續 3、4 子，不看兩端 ----

namespace {

int blackScore(const Board& board, const BoardScoreConfig& config = {}) {
    return RuleChecker::boardScore(board, PlayerId::Black, config);
}

}  // namespace

TEST(BoardScoreTest, W11_EmptyBoardIsZero) {
    EXPECT_EQ(blackScore(Board{}), 0);
    EXPECT_EQ(RuleChecker::boardScore(Board{}, PlayerId::White, {}), 0);
}

TEST(BoardScoreTest, W11_FourScoresTwo) {
    EXPECT_EQ(blackScore(boardFromRows({"...XXXX..."})), 2);
}

TEST(BoardScoreTest, W11_ThreeScoresOne) {
    EXPECT_EQ(blackScore(boardFromRows({"...XXX...."})), 1);
}

TEST(BoardScoreTest, W11_EndsDoNotMatter) {
    EXPECT_EQ(blackScore(boardFromRows({"XXXX..."})), 2);              // 邊界
    EXPECT_EQ(blackScore(boardFromRows({"...........XXXX"})), 2);
    EXPECT_EQ(blackScore(boardFromRows({"..OXXXX..."})), 2);           // 對手
    EXPECT_EQ(blackScore(boardFromRows({"..#XXXX..."})), 2);           // 已摧毀
    EXPECT_EQ(blackScore(boardFromRows({"..OXXXX#.."})), 2);           // 兩端都擋住也算
    EXPECT_EQ(blackScore(boardFromRows({"..OXXX#..."})), 1);
    EXPECT_EQ(blackScore(boardFromRows({"XXX......."})), 1);
}

TEST(BoardScoreTest, W11_OpponentAndDestroyedBreakTheRun) {
    EXPECT_EQ(blackScore(boardFromRows({"XXOXX....."})), 0);
    EXPECT_EQ(blackScore(boardFromRows({"XX#XX....."})), 0);
    EXPECT_EQ(blackScore(boardFromRows({"XXX#XXXX.."})), 1 + 2);
}

TEST(BoardScoreTest, W11_TwoStonesIsZero) {
    EXPECT_EQ(blackScore(boardFromRows({"...XX....."})), 0);
}

TEST(BoardScoreTest, W11_JumpShapesAreZero) {
    EXPECT_EQ(blackScore(boardFromRows({"..XX.XX..."})), 0);
    EXPECT_EQ(blackScore(boardFromRows({"..XX.X...."})), 0);
}

TEST(BoardScoreTest, W11_AllFourDirections) {
    const Board vertical = boardFromRows({".", ".", "X", "X", "X", "X", "."});
    EXPECT_EQ(blackScore(vertical), 2);
    const Board diagonalDown = boardFromRows({".", ".X", "..X", "...X", "....X", "....."});
    EXPECT_EQ(blackScore(diagonalDown), 2);
    const Board diagonalUp = boardFromRows({".....", "....X", "...X", "..X", ".X", "."});
    EXPECT_EQ(blackScore(diagonalUp), 2);
}

TEST(BoardScoreTest, W11_StoneCountsInEachDirection) {
    // (4, 3) 同時在橫向連三與直向連三裡
    const Board board = boardFromRows({
        "..........",
        "..........",
        "....X.....",
        "...XXX....",
        "....X.....",
        "..........",
    });
    EXPECT_EQ(blackScore(board), 2);
}

TEST(BoardScoreTest, W11_PatternsAddUp) {
    const Board board = boardFromRows({
        ".XXXX.....",
        "..........",
        "..OXXXX...",
        "..........",
        "...XXX....",
    });
    EXPECT_EQ(blackScore(board), 2 + 2 + 1);
}

TEST(BoardScoreTest, W11_OnlyCountsOwnStones) {
    const Board board = boardFromRows({"..OOOO....", "..........", "...XXX...."});
    EXPECT_EQ(blackScore(board), 1);
    EXPECT_EQ(RuleChecker::boardScore(board, PlayerId::White, {}), 2);
}

TEST(BoardScoreTest, W11_FullBoardStillScores) {
    // 每列依 XXXOOO… 重複、隔列錯開：橫向全是連三，直向與斜向最多連 2 或 1 子；沒有任何空格
    std::vector<std::string> rows;
    for (int y = 0; y < Board::kSize; ++y) {
        std::string row;
        for (int x = 0; x < Board::kSize; ++x) {
            row += ((x / 3) + y) % 2 == 0 ? 'X' : 'O';
        }
        rows.push_back(row);
    }
    const Board board = boardFromRowRange(rows);
    ASSERT_TRUE(board.isFull());
    const int black = blackScore(board);
    const int white = RuleChecker::boardScore(board, PlayerId::White, {});
    EXPECT_GT(black, 0);
    EXPECT_NE(black, white);  // 下滿時也能分出高下
}

TEST(BoardScoreTest, W11_UsesConfigValues) {
    const BoardScoreConfig config{10, 3};
    const Board board = boardFromRows({".XXXX.....", "..........", "OXXXX#....", "..........", ".XXX......"});
    EXPECT_EQ(blackScore(board, config), 10 + 10 + 3);
}

TEST(BoardScoreTest, W11_DefaultConfig) {
    const BoardScoreConfig config;
    EXPECT_EQ(config.four, 2);
    EXPECT_EQ(config.three, 1);
}
