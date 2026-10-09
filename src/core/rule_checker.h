#pragma once

#include <optional>
#include <vector>

#include "core/board.h"
#include "core/types.h"

// spec W1、W2
class RuleChecker {
public:
    static constexpr int kWinLength = 5;

    // 檢查剛下在 last 的那顆子是否造成連五；有的話回傳連線上所有棋子的座標（給 U6 用）。
    // 長連回傳整條連線；多個方向同時成五時，合併所有連線（交叉點只出現一次）。
    static std::optional<std::vector<Pos>> findFive(const Board&, Pos last);
};
