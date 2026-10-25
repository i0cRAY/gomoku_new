#pragma once

#include <optional>

#include "core/player_view.h"
#include "core/types.h"

// GameController 操作 AI 的介面（design §4.8）：遊戲本體用 AIEngine，ai_arena 另外可以換成基準 AI
class AIPlayer {
public:
    virtual ~AIPlayer() = default;
    virtual SkillId chooseSkill() = 0;                             // A2a
    virtual std::optional<Action> decide(const PlayerView&) = 0;  // A1–A9：只拿得到自己的 PlayerView（A3、E5）
    virtual void newGame() = 0;                                    // 新的一局重設反應時間的計時
};
