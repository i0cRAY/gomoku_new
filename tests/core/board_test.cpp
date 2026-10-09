#include <gtest/gtest.h>

#include <stdexcept>

#include "board_test_util.h"
#include "core/board.h"

TEST(BoardTest, B1_SizeIs15) {
    EXPECT_EQ(Board::kSize, 15);
}

TEST(BoardTest, B1_FourCornersAreInBounds) {
    const Board board;
    EXPECT_TRUE(board.inBounds({0, 0}));
    EXPECT_TRUE(board.inBounds({14, 0}));
    EXPECT_TRUE(board.inBounds({0, 14}));
    EXPECT_TRUE(board.inBounds({14, 14}));
}

TEST(BoardTest, B1_B3_OutOfBoundsCoordinates) {
    const Board board;
    EXPECT_FALSE(board.inBounds({-1, 0}));
    EXPECT_FALSE(board.inBounds({0, -1}));
    EXPECT_FALSE(board.inBounds({15, 0}));
    EXPECT_FALSE(board.inBounds({0, 15}));
    EXPECT_FALSE(board.inBounds({-1, 15}));
}

TEST(BoardTest, B2_NewBoardIsAllEmpty) {
    const Board board;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            EXPECT_EQ(board.at({x, y}), Cell::Empty);
            EXPECT_TRUE(board.isEmpty({x, y}));
        }
    }
}

TEST(BoardTest, B2_SetAndReadCornersAndCells) {
    Board board;
    board.set({0, 0}, Cell::Black);
    board.set({14, 0}, Cell::White);
    board.set({0, 14}, Cell::White);
    board.set({14, 14}, Cell::Black);
    EXPECT_EQ(board.at({0, 0}), Cell::Black);
    EXPECT_EQ(board.at({14, 0}), Cell::White);
    EXPECT_EQ(board.at({0, 14}), Cell::White);
    EXPECT_EQ(board.at({14, 14}), Cell::Black);
    EXPECT_FALSE(board.isEmpty({0, 0}));
    EXPECT_EQ(board.at({1, 0}), Cell::Empty);  // 相鄰格不受影響
    EXPECT_EQ(board.at({0, 1}), Cell::Empty);  // x、y 沒有對調

    board.set({0, 0}, Cell::Empty);
    EXPECT_TRUE(board.isEmpty({0, 0}));
}

TEST(BoardTest, B2_IsFullOnlyWhenEveryCellOccupied) {
    Board board;
    EXPECT_FALSE(board.isFull());
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            board.set({x, y}, (x + y) % 2 == 0 ? Cell::Black : Cell::White);
        }
    }
    EXPECT_TRUE(board.isFull());

    board.set({7, 7}, Cell::Empty);
    EXPECT_FALSE(board.isFull());
}

TEST(BoardTest, B2_ClearEmptiesEveryCell) {
    Board board;
    board.set({0, 0}, Cell::Black);
    board.set({7, 7}, Cell::White);
    board.set({14, 14}, Cell::Black);
    board.clear();
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            EXPECT_TRUE(board.isEmpty({x, y}));
        }
    }
}

TEST(BoardTestUtil, T04_FromRowsPlacesStones) {
    const Board board = boardFromRows({
        "..XX.O",
        ".O",
    });
    EXPECT_EQ(board.at({2, 0}), Cell::Black);
    EXPECT_EQ(board.at({3, 0}), Cell::Black);
    EXPECT_EQ(board.at({5, 0}), Cell::White);
    EXPECT_EQ(board.at({1, 1}), Cell::White);
    EXPECT_EQ(board.at({0, 0}), Cell::Empty);
    EXPECT_EQ(board.at({4, 0}), Cell::Empty);
    EXPECT_EQ(board.at({14, 14}), Cell::Empty);  // 沒寫到的格子是空的
}

TEST(BoardTestUtil, T04_FromRowsRejectsBadInput) {
    EXPECT_THROW(boardFromRows({"..Z"}), std::invalid_argument);
    EXPECT_THROW(boardFromRows({"................"}), std::invalid_argument);  // 16 格
}

TEST(BoardTest, B4_DestroyedCellIsNotEmpty) {
    Board board;
    board.set({3, 4}, Cell::Destroyed);
    EXPECT_EQ(board.at({3, 4}), Cell::Destroyed);
    EXPECT_FALSE(board.isEmpty({3, 4}));
}

TEST(BoardTest, W4_IsFullTreatsDestroyedAsNotEmpty) {
    Board board;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            board.set({x, y}, x < 5 ? Cell::Destroyed : Cell::Black);
        }
    }
    EXPECT_TRUE(board.isFull());
    board.set({0, 0}, Cell::Empty);
    EXPECT_FALSE(board.isFull());
}

TEST(BoardTest, G4_ClearRemovesDestroyedCells) {
    Board board;
    board.set({0, 0}, Cell::Destroyed);
    board.set({14, 14}, Cell::Destroyed);
    board.clear();
    EXPECT_TRUE(board.isEmpty({0, 0}));
    EXPECT_TRUE(board.isEmpty({14, 14}));
}

TEST(BoardTest, B4_TestUtilParsesDestroyed) {
    const Board board = boardFromRows({"#X.O"});
    EXPECT_EQ(board.at({0, 0}), Cell::Destroyed);
    EXPECT_EQ(board.at({1, 0}), Cell::Black);
    EXPECT_EQ(board.at({2, 0}), Cell::Empty);
    EXPECT_EQ(board.at({3, 0}), Cell::White);
}
