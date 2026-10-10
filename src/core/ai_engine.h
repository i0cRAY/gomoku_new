#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

#include "core/board.h"
#include "core/config.h"
#include "core/player_view.h"
#include "core/types.h"

// 棋型（spec §8.2），由強到弱
enum class Pattern { Five, OpenFour, Four, OpenThree, Three, OpenTwo, Two, None };

// AI 引擎（spec A1–A12）。亂數使用以 seed 初始化的 std::mt19937（A11）
class AIEngine {
public:
    static constexpr int kDirectionCount = 4;  // 橫、直、右下斜、右上斜

    // 反應時間（A2）、w 與評分表取自 AIConfig；技能耗能與範圍取自 SkillConfig
    AIEngine(PlayerId self, const AIConfig&, const SkillConfig&, std::uint32_t seed);

    SkillId chooseSkill();  // A2a：技能選擇階段隨機選一項
    // A1–A9：反應時間未到，或判斷本次不行動時回傳 nullopt。
    // 只拿得到自己的 PlayerView，讀不到對手的能量與冷卻（A3、E5）；下子間隔從 view.placeCooldownRemaining 判斷
    std::optional<Action> decide(const PlayerView&);
    void newGame();  // 新的一局對局時間從 0 重新開始，重設反應時間的計時

    // pos 必須是空格：回傳假設 player 下在 pos 後，dirIndex 方向最強的棋型。
    // 邊界、對手棋子與已摧毀的格子都視為擋住（A13）；禁區不在 Board 上，所以照空格判斷（spec §8.2）
    static Pattern patternAt(const Board&, Pos, PlayerId, int dirIndex);
    static int patternScore(Pattern, const PatternScores& = {});

    // A10：進攻分 + 防守分 × w（含雙活三加成）
    double score(const Board&, Pos) const;
    // A10–A12：全盤最高分的空格（排除 excluded，A5a）；同分隨機挑一格；
    // 沒有任何棋子且中央能下時下中央；沒有可下的格子回傳 nullopt
    std::optional<Pos> bestPlacement(const Board&, const std::vector<Pos>& excluded = {});

private:
    struct Threat {
        int fivePoints;
        double bestAttack;
        bool operator<(const Threat& other) const;
    };

    double sideScore(const Board&, Pos, PlayerId) const;
    bool canPlace(const PlayerView&) const;
    bool skillReady(const PlayerView&, SkillId) const;
    std::vector<Pos> opponentZones(const PlayerView&) const;  // A5a：當下對自己有效的禁區格
    std::vector<Pos> fivePoints(const Board&, PlayerId) const;
    Threat threatOf(const Board&) const;  // 對手的威脅分（spec A5）：成五點數量，再比全盤最高進攻分
    std::optional<Action> respondToFivePoints(const PlayerView&, const std::vector<Pos>& points,
                                              const std::vector<Pos>& zones);
    std::vector<Pos> threatStones(const Board&, const std::vector<Pos>& points) const;  // spec A5 名詞
    std::optional<Pos> bestBombTarget(const Board&, const std::vector<Pos>& points);  // A5c
    std::optional<Pos> bestDestroyTarget(const Board&, const std::vector<Pos>& points);  // A5b
    std::optional<Pos> openThreeBlock(const Board&, const std::vector<Pos>& zones) const;  // A7
    std::optional<Pos> highestScore(const Board&, const std::vector<Pos>& candidates) const;

    PlayerId self;
    TimeMs reactionTime;
    double defenseWeight;
    std::mt19937 rng;
    PatternScores scores;
    SkillConfig skills;  // spec S2 各技能的消耗、SB3 炸彈範圍、SX2 摧毀範圍
    std::optional<TimeMs> lastDecision;
};
