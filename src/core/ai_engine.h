#pragma once

#include <cstdint>
#include <optional>
#include <random>

#include "core/board.h"
#include "core/config.h"
#include "core/types.h"

// 棋型（spec §8.2），由強到弱
enum class Pattern { Five, OpenFour, Four, OpenThree, Three, OpenTwo, Two, None };

// AI 引擎（spec A1–A12）。亂數使用以 seed 初始化的 std::mt19937（A11）
class AIEngine {
public:
    static constexpr int kDirectionCount = 4;  // 橫、直、右下斜、右上斜

    AIEngine(PlayerId self, TimeMs reactionTime, double defenseWeight, std::uint32_t seed,
             PatternScores scores = {});

    // pos 必須是空格：回傳假設 player 下在 pos 後，dirIndex 方向最強的棋型；邊界與對手棋子視為擋住
    static Pattern patternAt(const Board&, Pos, PlayerId, int dirIndex);
    static int patternScore(Pattern, const PatternScores& = {});

    // A10：進攻分 + 防守分 × w（含雙活三加成）
    double score(const Board&, Pos) const;
    // A10–A12：全盤最高分的空格；同分隨機挑一格；空盤下中央；棋盤滿了回傳 nullopt
    std::optional<Pos> bestPlacement(const Board&);

private:
    double sideScore(const Board&, Pos, PlayerId) const;

    PlayerId self;
    TimeMs reactionTime;
    double defenseWeight;
    std::mt19937 rng;
    PatternScores scores;
};
