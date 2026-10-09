#pragma once

#include <array>
#include <cstddef>

#include "core/types.h"

// spec B1–B4：15×15 棋盤，(0, 0) 在左上角
class Board {
public:
    static constexpr int kSize = 15;

    Cell at(Pos p) const;
    bool inBounds(Pos p) const;
    bool isEmpty(Pos p) const;  // Destroyed 不算空格（B4）
    void set(Pos p, Cell c);  // 不做規則檢查，由呼叫端負責
    bool isFull() const;  // 沒有任何 Empty 格（W4）
    void clear();         // 包括 Destroyed 也清掉（G4）

private:
    static std::size_t indexOf(Pos p);

    std::array<Cell, kSize * kSize> cells{};
};
