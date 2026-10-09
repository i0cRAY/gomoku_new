#include "core/board.h"

#include <algorithm>
#include <cassert>

Cell Board::at(Pos p) const {
    return cells[indexOf(p)];
}

bool Board::inBounds(Pos p) const {
    return p.x >= 0 && p.x < kSize && p.y >= 0 && p.y < kSize;
}

bool Board::isEmpty(Pos p) const {
    return at(p) == Cell::Empty;
}

void Board::set(Pos p, Cell c) {
    cells[indexOf(p)] = c;
}

bool Board::isFull() const {
    return std::none_of(cells.begin(), cells.end(), [](Cell c) { return c == Cell::Empty; });
}

void Board::clear() {
    cells.fill(Cell::Empty);
}

std::size_t Board::indexOf(Pos p) {
    assert(p.x >= 0 && p.x < kSize && p.y >= 0 && p.y < kSize);
    return static_cast<std::size_t>(p.y * kSize + p.x);
}
