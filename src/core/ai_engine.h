#pragma once

#include "core/board.h"
#include "core/types.h"

// 棋型（spec §8.2），由強到弱
enum class Pattern { Five, OpenFour, Four, OpenThree, Three, OpenTwo, Two, None };

// AI 引擎（spec A1–A12）
class AIEngine {
public:
    static constexpr int kDirectionCount = 4;  // 橫、直、右下斜、右上斜

    // pos 必須是空格：回傳假設 player 下在 pos 後，dirIndex 方向最強的棋型；邊界與對手棋子視為擋住
    static Pattern patternAt(const Board&, Pos, PlayerId, int dirIndex);
};
