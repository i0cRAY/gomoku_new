#pragma once

#include <optional>
#include <vector>

#include "core/board.h"
#include "core/player_state.h"
#include "core/types.h"

// 某位玩家看得到的資訊（spec E5）。沒有任何對手 PlayerState 的欄位。
// 放在 core 而不是 app，因為 src/net 也要用它，而 net 不能依賴 app。
struct PlayerView {
    PlayerId me = PlayerId::Black;
    Board board;                                    // 公開
    GameStatus status = GameStatus::SkillSelect;
    TimeMs now = 0;
    PlayerState self;                               // 只有自己的狀態
    double nextEnergyRatio = 0.0;                   // spec E6，下一格的累積比例 0.0–1.0
    bool accelerating = false;                      // 加速中（能量條可換顏色）
    std::optional<SkillId> opponentSkillRevealed;   // 對手第一次用技能後才有值，只有名稱（spec S1）
    TimeMs countdownRemaining = 0;                  // Countdown 狀態時的剩餘毫秒（spec G2）
    std::vector<Pos> winningLine;                   // 結束時的連線，可能含多條線（spec U6）
};
