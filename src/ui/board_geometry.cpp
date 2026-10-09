#include "ui/board_geometry.h"

#include <algorithm>
#include <cmath>

#include "core/board.h"

namespace {

constexpr int kMarginCells = 1;  // 棋盤四周的邊距（格）

}  // namespace

BoardGeometry::BoardGeometry(int width, int height) {
    const int span = Board::kSize - 1 + 2 * kMarginCells;
    cell = static_cast<double>(std::max(0, std::min(width, height))) / span;
    const double boardSpan = cell * (Board::kSize - 1);
    originX = (width - boardSpan) / 2.0;
    originY = (height - boardSpan) / 2.0;
}

BoardGeometry::Point BoardGeometry::cellCenter(Pos p) const {
    return Point{originX + p.x * cell, originY + p.y * cell};
}

std::optional<Pos> BoardGeometry::pixelToCell(double x, double y) const {
    if (cell <= 0.0) {
        return std::nullopt;
    }
    const Pos p{static_cast<int>(std::lround((x - originX) / cell)),
                static_cast<int>(std::lround((y - originY) / cell))};
    if (p.x < 0 || p.x >= Board::kSize || p.y < 0 || p.y >= Board::kSize) {
        return std::nullopt;
    }
    return p;
}
