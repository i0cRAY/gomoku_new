#pragma once

#include <cstdint>

#include "core/types.h"

// 所有數值集中在這裡（design §3.1）

enum class Difficulty : std::uint8_t { Easy, Normal, Hard };

struct SkillConfig {
    TimeMs accelerateCooldown = 25000;  // spec SA
    TimeMs accelerateDuration = 5000;
    TimeMs bombCooldown = 20000;  // spec SB
};

struct AIConfig {
    Difficulty difficulty = Difficulty::Normal;
    TimeMs reactionEasy = 1200;  // spec A2
    TimeMs reactionNormal = 700;
    TimeMs reactionHard = 350;
    double defenseWeight = 0.8;  // spec A10，各難度相同
};

struct MatchConfig {
    TimeMs regenInterval = 2000;  // spec §4：500–10000 且為 500 的倍數
    int maxEnergy = 10;
    int startEnergy = 1;
    TimeMs placeCooldown = 1000;  // spec §5
    TimeMs countdown = 3000;      // spec G2
    SkillConfig skill;
    AIConfig ai;  // 僅 M1 使用
};
