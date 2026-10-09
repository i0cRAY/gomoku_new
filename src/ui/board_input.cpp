#include "ui/board_input.h"

BoardInput::BoardInput(PlayerId me) : me(me) {}

std::optional<Action> BoardInput::onBoardClick(Pos pos, const PlayerView&) {
    if (targeting) {
        targeting = false;
        return SkillAction{me, SkillId::Bomb, pos};  // 目標不合法時由 GameController 拒絕並提示
    }
    return PlaceAction{me, pos};
}

std::optional<Action> BoardInput::onSkillKey(const PlayerView& view) {
    if (view.self.skill == SkillId::Accelerate) {
        targeting = false;
        return SkillAction{me, SkillId::Accelerate, std::nullopt};
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
