#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

#include "core/ai_player.h"
#include "core/board.h"
#include "core/config.h"
#include "core/player_view.h"
#include "core/types.h"
#include "t25/opponent_model.h"

namespace t25 {

// 棋型（spec §8.2），由強到弱
enum class Pattern { Five, OpenFour, Four, OpenThree, Three, OpenTwo, Two, None };

// spec §11：T25 完成時（commit 0b18323）AIEngine 的凍結複本，只給 ai_arena 當對手，之後不修改（design §4.8）。
// AI 引擎（spec A1–A18）。亂數使用以 seed 初始化的 std::mt19937（A11）
class T25AI : public AIPlayer {
public:
    static constexpr int kDirectionCount = 4;  // 橫、直、右下斜、右上斜

    // 反應時間（A2）、w、評分表、保留能量（A15）取自 MatchConfig::ai；技能耗能與範圍取自 MatchConfig::skill；
    // 推算對手（A14）用回能間隔、開局能量、能量上限與下子間隔
    T25AI(PlayerId self, const MatchConfig&, std::uint32_t seed);

    SkillId chooseSkill() override;  // A2a：技能選擇階段隨機選一項
    // A1–A9：反應時間未到，或判斷本次不行動時回傳 nullopt。
    // 只拿得到自己的 PlayerView，讀不到對手的能量與冷卻（A3、E5）；下子間隔從 view.placeCooldownRemaining 判斷
    std::optional<Action> decide(const PlayerView&) override;
    void newGame() override;  // 新的一局對局時間從 0 重新開始，重設反應時間的計時

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
    bool skillReadyWithSpare(const PlayerView&, SkillId) const;  // 用完還留 1 格可以下子（A8b）
    bool keepsReserve(const PlayerView&) const;                   // A15
    bool canFollowOpenFour(const PlayerView&) const;              // A16
    bool opponentCannotRespond() const;                           // A17、A18 共用的 A14a 條件
    // 下在該格後，某個方向剛好形成 pattern 的空格（排除 excluded，A5a）
    static std::vector<Pos> cellsForming(const Board&, PlayerId, Pattern, const std::vector<Pos>& excluded);
    std::vector<Pos> opponentZones(const PlayerView&) const;  // A5a：當下對自己有效的禁區格
    std::vector<Pos> fivePoints(const Board&, PlayerId) const;
    Threat threatOf(const Board&) const;  // 對手的威脅分（spec A5）：成五點數量，再比全盤最高進攻分
    std::optional<Action> respondToFivePoints(const PlayerView&, const std::vector<Pos>& points,
                                              const std::vector<Pos>& zones);
    std::vector<Pos> threatStones(const Board&, const std::vector<Pos>& points) const;  // spec A5 名詞
    std::optional<Pos> bestBombTarget(const Board&, const std::vector<Pos>& stones);    // A5c
    struct AreaChoice {
        Pos target;
        int gain;  // 範圍內對手棋子數 − 己方棋子數
    };
    // A5b（mustCover 有值時，範圍至少涵蓋其中一顆）、A8b（不限範圍）
    std::optional<AreaChoice> bestDestroyArea(const Board&, const std::optional<std::vector<Pos>>& mustCover);
    std::optional<Pos> openThreeBlock(const Board&, const std::vector<Pos>& zones) const;  // A7
    std::optional<Pos> highestScore(const Board&, const std::vector<Pos>& candidates) const;
    std::optional<Pos> highestScoreRandomTie(const Board&, const std::vector<Pos>& candidates);  // 同分依 A11

    PlayerId self;
    TimeMs reactionTime;
    TimeMs regenInterval;  // spec §4：A16 計算自己下一格能量何時回來
    TimeMs placeCooldown;  // spec §5
    double defenseWeight;
    std::mt19937 rng;
    PatternScores scores;
    SkillConfig skills;  // spec S2 各技能的消耗、SB3 炸彈範圍、SX2 摧毀範圍
    int destroyMinGain;  // spec A8b
    bool useSkills;      // spec §11 無技能跑分
    int reserveEnergy;   // spec A15
    bool countOpponentDestroy;  // spec A14a
    OpponentModel opponentModel;  // spec A14
    std::optional<TimeMs> lastDecision;
};

// T25 當時 AIConfig 的預設值（spec §11 的凍結設定）
AIConfig frozenConfig(bool useSkills);

}  // namespace t25
