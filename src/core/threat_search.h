#pragma once

#include <vector>

#include "core/board.h"
#include "core/types.h"

// spec §8.7：連續衝四（design §4.8）

struct VcfLimits {
    int maxDepth;    // 自己最多幾子（spec 的 L 上限）
    int nodeBudget;  // 還能檢查幾個盤面；每檢查一個就遞減，同一次決策的多次搜尋共用
};

struct VcfLine {
    std::vector<Pos> moves;       // 自己的下子，不含最後成五的那一子
    std::vector<Pos> fivePoints;  // 每一子之後的成五點：前面是對手被迫擋的位置，最後一項是 ≥ 2 個成五點之一
    int length = 0;               // spec 的 L：自己需要的子數，含最後成五那一子
};

// player 下一子就能成五的空格：某個五格窗口內剛好 4 顆 player 的子、1 個空格（spec A4、A5 的成五點）
std::vector<Pos> fivePointsOf(const Board&, PlayerId);

// L 最短的連續衝四，每個不同的第一子一條；沒有或搜尋上限用完時回傳找到的部分（可能是空的）。
// excluded：player 不能下的格子（對手的禁區，A5a）；對手被迫擋的那一子假設一定擋得到
std::vector<VcfLine> findVcf(const Board&, PlayerId, const std::vector<Pos>& excluded, VcfLimits&);
