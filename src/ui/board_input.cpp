#include "ui/board_input.h"

bool needsTarget(SkillId skill) {
    return skill == SkillId::Bomb || skill == SkillId::Destroy;
}

BoardInput::BoardInput(PlayerId me) : me(me) {}

std::optional<Action> BoardInput::onBoardClick(Pos pos, const PlayerView& view) {
    if (targeting) {
        targeting = false;
        return SkillAction{me, view.self.skill, pos};  // 目標不合法時由 GameController 拒絕並提示
    }
    return PlaceAction{me, pos};
}

std::optional<Action> BoardInput::onSkillKey(const PlayerView& view) {
    if (!needsTarget(view.self.skill)) {
        targeting = false;
        return SkillAction{me, view.self.skill, std::nullopt};
    }
    targeting = !targeting;
    return std::nullopt;
}

void BoardInput::cancel() {
    targeting = false;
}

void BoardInput::onViewChanged(const PlayerView& view) {
    if (view.status != GameStatus::Running) {
        targeting = false;
    }
}
