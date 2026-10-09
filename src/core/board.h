#pragma once

#include <array>
#include <cstddef>

#include "core/types.h"

// spec B1–B3：15×15 棋盤，(0, 0) 在左上角
class Board {
public:
    static constexpr int kSize = 15;

    Cell at(Pos p) const;
    bool inBounds(Pos p) const;
    bool isEmpty(Pos p) const;
    void set(Pos p, Cell c);  // 不做規則檢查，由呼叫端負責
    bool isFull() const;
    void clear();

private:
    static std::size_t indexOf(Pos p);

    std::array<Cell, kSize * kSize> cells{};
};
