#pragma once

#include <optional>

#include "core/player_view.h"
#include "core/types.h"

// U3：需要選目標的技能（炸彈、摧毀）
bool needsTarget(SkillId);

// 把本機玩家的點擊與快捷鍵轉成 Action（spec U1、U3、U4）。
// 只保存介面的輸入模式（是否正在選目標），不保存遊戲狀態；能不能做由 GameController 判定。
class BoardInput {
public:
    explicit BoardInput(PlayerId me);

    std::optional<Action> onBoardClick(Pos, const PlayerView&);
    // U4：加速、霸道直接送出；炸彈、摧毀進入（或再按一次離開）選目標模式（U3）
    std::optional<Action> onSkillKey(const PlayerView&);
    void cancel();  // U3：右鍵或 Esc
    void onViewChanged(const PlayerView&);  // 對局不在進行中時離開選目標模式
    bool isTargeting() const { return targeting; }

private:
    PlayerId me;
    bool targeting = false;
};
