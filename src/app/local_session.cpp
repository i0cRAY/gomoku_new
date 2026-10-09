#include "app/local_session.h"

LocalSession::LocalSession(MatchConfig config, PlayerId localPlayer, QObject* parent)
    : GameSession(parent), config(config), localPlayer(localPlayer), controller(config) {
    connect(&controller, &GameController::stateChanged, this, &GameSession::stateChanged);
    connect(&controller, &GameController::actionRejected, this, &GameSession::actionRejected);
    connect(&controller, &GameController::gameOver, this, [this](GameStatus status, std::vector<Pos> line) {
        gameClock.stop();
        emit gameOver(status, line);
    });
    connect(&controller, &GameController::skillConfirmed, this, [this](PlayerId player) {
        if (player != this->localPlayer) {
            emit opponentReady();
        }
    });
    connect(&gameClock, &GameClock::tick, &controller, &GameController::tick);
}

void LocalSession::selectSkill(PlayerId player, SkillId skill) {
    controller.selectSkill(player, skill);
}

void LocalSession::confirmSkill(PlayerId player) {
    controller.confirmSkill(player);
    if (controller.viewFor(player).status == GameStatus::Countdown && !gameClock.isRunning()) {
        gameClock.start(-config.countdown);  // G2：倒數開始時對局時間為 −3000
    }
}

void LocalSession::request(const Action& action) {
    controller.submit(action, gameClock.now());
}

PlayerView LocalSession::viewFor(PlayerId player) const {
    return controller.viewFor(player);
}

void LocalSession::requestRematch() {
    controller.restart();
    if (controller.viewFor(localPlayer).status == GameStatus::SkillSelect) {
        gameClock.stop();
    }
}
