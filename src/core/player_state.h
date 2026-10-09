#pragma once

#include <optional>

#include "core/types.h"

// 一位玩家的完整狀態。只存在 GameController 內部，對外一律透過 PlayerView（spec E5）
struct PlayerState {
    int energy = 0;
    TimeMs regenProgress = 0;                 // E2–E4
    std::optional<TimeMs> lastPlaceTime;      // P1-4、P4
    SkillId skill = SkillId::Bomb;            // 開局前選的技能，S1
    int dominateCharges = 0;                  // 霸道剩餘次數，SZ1–SZ2（不公開）
    bool destroyUsed = false;                 // 摧毀已用過，SX3
    int score = 0;                            // W1、W2（公開，也會複製到 PlayerView::scores）
};
