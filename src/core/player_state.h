#pragma once

#include <optional>

#include "core/types.h"

// 一位玩家的完整狀態。只存在 GameController 內部，對外一律透過 PlayerView（spec E5）
struct PlayerState {
    int energy = 0;
    TimeMs regenProgress = 0;                 // E2–E4
    std::optional<TimeMs> lastPlaceTime;      // P1-4、P4
    TimeMs accelerateUntil = 0;               // SA1，0 表示沒有加速
    SkillId skill = SkillId::Accelerate;      // 開局前選的技能，S1
    TimeMs skillReadyAt = 0;                  // 技能冷卻結束的時間，S3、S4
};
