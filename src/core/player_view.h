#pragma once

#include <array>
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
    std::optional<SkillId> opponentSkillRevealed;   // 對手第一次用技能後才有值，只有名稱（spec S1）
    TimeMs countdownRemaining = 0;                  // Countdown 狀態時的剩餘毫秒（spec G2）
    // 以下為公開資訊（spec E5）
    std::array<int, 2> scores{};                    // 以 PlayerId 為索引（W1、U8）
    MatchMode mode = MatchMode::TimeLimit;          // G1b
    TimeMs timeRemaining = 0;                       // 限時模式的剩餘毫秒（W7、U8）
    int targetScore = 0;                            // 達分模式的目標分數（W8、U8）
    std::vector<ZoneCell> zones;                    // 未到期的禁區（SZ4、U9）
    std::vector<ClearedLine> lastClearedLines;      // 最近一次消除的連線（U11）
};
