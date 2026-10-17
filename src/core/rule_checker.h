#pragma once

#include <vector>

#include "core/board.h"
#include "core/config.h"
#include "core/types.h"

// spec W1、W2、W6、B4
class RuleChecker {
public:
    // 回傳剛下在 last 的那顆子在每個方向形成的連線（≥ minLength 顆同色連續棋子），每個方向最多一條。
    // 長連回傳整條（W2）；交叉時 last 會同時出現在多條線裡（W6）。已摧毀的格子與對手棋子一樣會截斷連線（B4）。
    static std::vector<std::vector<Pos>> findLines(const Board&, Pos last, int minLength);

    // spec W11：player 在盤面上的棋型分數（活四、衝四、活三），掃描整個棋盤的四個方向。
    // 只算剛好連續 3、4 子的段，跳形不算；邊界、已摧毀的格子與對手棋子算擋住。
    // 禁區不在 Board 裡，所以自然算空格。
    static int boardScore(const Board&, PlayerId, const BoardScoreConfig&);
};
