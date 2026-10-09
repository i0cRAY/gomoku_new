#pragma once

#include <optional>

#include "core/types.h"

// 棋盤在元件中的位置計算（純計算，不依賴 Qt，方便測試）。
// 棋盤置中，四周各留一格的邊距，所以 15 條線佔 (kSize - 1 + 2) 格。
class BoardGeometry {
public:
    struct Point {
        double x;
        double y;
    };

    BoardGeometry(int width, int height);

    double cellSize() const { return cell; }
    Point cellCenter(Pos p) const;
    // 點擊位置換成最近的交叉點；離棋盤最外圈的線超過半格時回傳 nullopt（U1）
    std::optional<Pos> pixelToCell(double x, double y) const;

private:
    double cell;
    double originX;
    double originY;
};
