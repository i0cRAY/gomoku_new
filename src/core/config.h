#pragma once

#include <cstdint>

#include "core/types.h"

// 所有數值集中在這裡（design §3.1）

struct SkillConfig {
    int bombCost = 2;             // spec S2：各技能的能量消耗，沒有冷卻
    int dominateCost = 3;
    int destroyCost = 3;
    int bombSize = 2;             // spec SB3：以目標為左上角的 2×2
    int dominateStones = 3;       // spec SZ1
    TimeMs zoneDuration = 3000;   // spec SZ2
    int destroyRadius = 2;        // spec SX2：5×5 = 中心 ±2

    // spec S2：規則檢查、HUD、AI 共用
    int costOf(SkillId skill) const {
        switch (skill) {
            case SkillId::Bomb: return bombCost;
            case SkillId::Dominate: return dominateCost;
            case SkillId::Destroy: return destroyCost;
        }
        return 0;
    }
};

// spec A10 的評分表
struct PatternScores {
    int five = 100000;
    int openFour = 10000;
    int doubleThreeBonus = 8000;  // 同一格兩個以上方向形成活三或更強
    int four = 1000;
    int openThree = 1000;
    int three = 100;
    int openTwo = 100;
    int two = 10;
};

struct AIConfig {
    TimeMs reactionTime = 350;   // spec A2，不分難度
    double defenseWeight = 0.8;  // spec A10
    PatternScores scores;
};

struct MatchConfig {
    TimeMs regenInterval = 2000;  // spec §4：500–10000 且為 500 的倍數
    int maxEnergy = 10;
    int startEnergy = 1;
    TimeMs placeCooldown = 500;   // spec §5
    TimeMs countdown = 3000;      // spec G2
    int minLineLength = 5;        // spec W1
    MatchMode mode = MatchMode::TimeLimit;  // spec G1b ⚠️待確認
    TimeMs timeLimit = 180000;    // 限時模式：60000／180000／300000
    int targetScore = 20;         // 達分模式：5–100 ⚠️待確認
    bool showAiInfo = false;      // spec M1a，僅 M1 使用，只影響 UI
    SkillConfig skill;
    AIConfig ai;  // 僅 M1 使用
};
