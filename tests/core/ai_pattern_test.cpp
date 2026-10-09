#include <gtest/gtest.h>

#include "board_test_util.h"
#include "core/ai_engine.h"

namespace {

constexpr int kHorizontal = 0;
constexpr int kVertical = 1;
constexpr int kDiagonalDown = 2;  // 右下
constexpr int kDiagonalUp = 3;    // 右上

// 第 0 列的橫向棋型：假設黑方下在 x
Pattern blackRow(std::string_view row, int x) {
    return AIEngine::patternAt(boardFromRows({row}), {x, 0}, PlayerId::Black, kHorizontal);
}

}  // namespace

// ---- 連五 ----

TEST(PatternTest, A10_FiveInARow) {
    EXPECT_EQ(blackRow("..XXXX...", 6), Pattern::Five);
    EXPECT_EQ(blackRow("..XX.XX..", 4), Pattern::Five);  // 補中間
}

TEST(PatternTest, W2_A10_LongLineCountsAsFive) {
    EXPECT_EQ(blackRow("..XXX.XX..", 5), Pattern::Five);
}

// ---- 活四 / 衝四 ----

TEST(PatternTest, A10_OpenFourHasTwoFivePoints) {
    EXPECT_EQ(blackRow("..XXX....", 5), Pattern::OpenFour);
    EXPECT_EQ(blackRow("...XXX...", 2), Pattern::OpenFour);
}

TEST(PatternTest, A10_FourBlockedByOpponent) {
    EXPECT_EQ(blackRow(".OXXX....", 5), Pattern::Four);
}

TEST(PatternTest, A10_FourBlockedByEdge) {
    EXPECT_EQ(blackRow("XXX......", 3), Pattern::Four);
}

TEST(PatternTest, A10_SplitFourWithGapInMiddle) {
    EXPECT_EQ(blackRow("..XX.X....", 6), Pattern::Four);  // ●●_●●
    EXPECT_EQ(blackRow("..X.XX....", 6), Pattern::Four);  // ●_●●●
}

TEST(PatternTest, A10_FourBlockedOnBothSidesIsDead) {
    EXPECT_EQ(blackRow("OXXX.O", 4), Pattern::None);  // OXXXXO：沒有成五點
}

// ---- 活三 / 眠三 ----

TEST(PatternTest, A10_OpenThree) {
    EXPECT_EQ(blackRow("...XX....", 5), Pattern::OpenThree);
}

TEST(PatternTest, A10_JumpThreeIsOpen) {
    EXPECT_EQ(blackRow("..XX.....", 5), Pattern::OpenThree);  // _●●_●_
    EXPECT_EQ(blackRow("..X.X....", 3), Pattern::OpenThree);  // 補成 _●●●_
}

TEST(PatternTest, A10_ThreeBlockedByOpponent) {
    EXPECT_EQ(blackRow(".OXX.....", 4), Pattern::Three);
}

TEST(PatternTest, A10_ThreeBlockedByEdge) {
    EXPECT_EQ(blackRow("XX.......", 2), Pattern::Three);
}

TEST(PatternTest, A10_ThreeWithNoRoomIsDead) {
    EXPECT_EQ(blackRow("OXX.O....", 3), Pattern::None);  // OXXXO
}

// ---- 活二 / 眠二 ----

TEST(PatternTest, A10_OpenTwo) {
    EXPECT_EQ(blackRow("....X....", 5), Pattern::OpenTwo);
    EXPECT_EQ(blackRow("...X.....", 5), Pattern::OpenTwo);  // _●_●_
}

TEST(PatternTest, A10_TwoBlockedByOpponent) {
    EXPECT_EQ(blackRow(".OX......", 3), Pattern::Two);
}

TEST(PatternTest, A10_SingleStoneIsNone) {
    EXPECT_EQ(blackRow(".........", 4), Pattern::None);
}

// ---- 其他方向、白方、前提 ----

TEST(PatternTest, A10_VerticalFive) {
    const Board board = boardFromRows({".X", ".X", ".X", ".X", ".."});
    EXPECT_EQ(AIEngine::patternAt(board, {1, 4}, PlayerId::Black, kVertical), Pattern::Five);
}

TEST(PatternTest, A10_DiagonalDownOpenFour) {
    const Board board = boardFromRows({".....", ".X...", "..X..", "...X.", "....."});
    EXPECT_EQ(AIEngine::patternAt(board, {4, 4}, PlayerId::Black, kDiagonalDown), Pattern::OpenFour);
}

TEST(PatternTest, A10_DiagonalUpFourBlockedByEdge) {
    // (0,4) 碰到左邊界，(4,0) 再過去 (5,-1) 也超出棋盤 → 只有一端能延伸
    const Board board = boardFromRows({"......", "...X..", "..X...", ".X....", "X....."});
    EXPECT_EQ(AIEngine::patternAt(board, {4, 0}, PlayerId::Black, kDiagonalUp), Pattern::Five);
    const Board three = boardFromRows({"......", "......", "..X...", ".X....", "X....."});
    EXPECT_EQ(AIEngine::patternAt(three, {3, 1}, PlayerId::Black, kDiagonalUp), Pattern::Four);
}

TEST(PatternTest, A10_DirectionsAreIndependent) {
    const Board board = boardFromRows({"", "..XXX...."});
    EXPECT_EQ(AIEngine::patternAt(board, {5, 1}, PlayerId::Black, kHorizontal), Pattern::OpenFour);
    EXPECT_EQ(AIEngine::patternAt(board, {5, 1}, PlayerId::Black, kVertical), Pattern::None);
}

TEST(PatternTest, A10_WhitePatternsUseWhiteStones) {
    const Board board = boardFromRows({".XOOO...."});
    EXPECT_EQ(AIEngine::patternAt(board, {5, 0}, PlayerId::White, kHorizontal), Pattern::Four);
    EXPECT_EQ(AIEngine::patternAt(board, {5, 0}, PlayerId::Black, kHorizontal), Pattern::None);
}

TEST(PatternTest, A10_OccupiedCellIsNone) {
    const Board board = boardFromRows({"..XXXX..."});
    EXPECT_EQ(AIEngine::patternAt(board, {3, 0}, PlayerId::Black, kHorizontal), Pattern::None);
}
