#pragma once

#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "core/board.h"

// 測試輔助：用字串建立棋盤。每個字串是一列（第 0 個是 y = 0），字元依序對應 x = 0, 1, 2…
// '.' 空、'X' 黑、'O' 白；沒寫到的格子都是空的。
template <typename Rows>
Board boardFromRowRange(const Rows& rows) {
    Board board;
    int y = 0;
    for (std::string_view row : rows) {
        for (int x = 0; x < static_cast<int>(row.size()); ++x) {
            const Pos p{x, y};
            if (!board.inBounds(p)) {
                throw std::invalid_argument("boardFromRows: 超出棋盤");
            }
            switch (row[static_cast<std::size_t>(x)]) {
                case '.': break;
                case 'X': board.set(p, Cell::Black); break;
                case 'O': board.set(p, Cell::White); break;
                default: throw std::invalid_argument("boardFromRows: 不認得的字元");
            }
        }
        ++y;
    }
    return board;
}

inline Board boardFromRows(std::initializer_list<std::string_view> rows) {
    return boardFromRowRange(rows);
}

inline Board boardFromRows(const std::vector<std::string_view>& rows) {
    return boardFromRowRange(rows);
}
