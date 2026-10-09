#include "app/game_controller.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "core/rule_checker.h"

namespace {

Cell stoneOf(PlayerId player) {
    return player == PlayerId::Black ? Cell::Black : Cell::White;
}

}  // namespace

GameController::GameController(MatchConfig config, QObject* parent)
    : QObject(parent), config(config), energy(config), skills(config.skill) {}

void GameController::selectSkill(PlayerId player, SkillId skill) {
    if (status != GameStatus::SkillSelect || confirmed[indexOf(player)]) {
        return;  // G1a：按下確定後再選一律忽略
    }
    selectedSkill[indexOf(player)] = skill;
    emit stateChanged();
}

void GameController::confirmSkill(PlayerId player) {
    if (status != GameStatus::SkillSelect || !selectedSkill[indexOf(player)]) {
        return;  // G1a：尚未選技能時不能確定
    }
    if (confirmed[indexOf(player)]) {
        return;
    }
    confirmed[indexOf(player)] = true;
    if (confirmed[0] && confirmed[1]) {
        status = GameStatus::Countdown;  // G2
        lastTime = -config.countdown;
        initPlayers();  // 倒數期間就顯示所選技能與開局能量
    }
    emit skillConfirmed(player);
    emit stateChanged();
}

ActionResult GameController::submit(const Action& action, TimeMs now) {
    advanceTo(now);

    if (const auto* place = std::get_if<PlaceAction>(&action)) {
        return submitPlace(*place, std::max(now, lastTime));
    }
    return submitSkill(std::get<SkillAction>(action), std::max(now, lastTime));
}

void GameController::tick(TimeMs now) {
    if (status != GameStatus::Countdown && status != GameStatus::Running) {
        return;
    }
    advanceTo(now);
    emit stateChanged();
}

PlayerView GameController::viewFor(PlayerId player) const {
    PlayerView view;
    view.me = player;
    view.board = board;
    view.status = status;
    view.now = lastTime;
    view.self = state(player);
    if (status == GameStatus::SkillSelect && selectedSkill[indexOf(player)]) {
        view.self.skill = *selectedSkill[indexOf(player)];  // 只有自己的選擇（S1）
    }
    if (skillRevealed[indexOf(opponent(player))]) {
        view.opponentSkillRevealed = state(opponent(player)).skill;
    }
    view.nextEnergyRatio = energy.nextEnergyRatio(view.self);
    view.accelerating = view.self.accelerateUntil != 0 && lastTime < view.self.accelerateUntil;
    view.countdownRemaining = status == GameStatus::Countdown ? std::max<TimeMs>(0, -lastTime) : 0;
    view.winningLine = winningLine;
    return view;
}

void GameController::restart() {
    if (status != GameStatus::BlackWon && status != GameStatus::WhiteWon && status != GameStatus::Draw &&
        status != GameStatus::Aborted) {
        return;
    }
    // selectedSkill 保留，作為這一局的預設選擇（G4）
    status = GameStatus::SkillSelect;
    board.clear();
    players = {};
    confirmed = {};
    skillRevealed = {};
    winningLine.clear();
    lastTime = 0;
    emit stateChanged();
}

void GameController::advanceTo(TimeMs now) {
    if (status == GameStatus::Countdown) {
        if (now < 0) {
            lastTime = std::max(lastTime, now);
            return;
        }
        startMatch();  // G2：倒數結束時對局時間 = 0
    }
    if (status == GameStatus::Running && now > lastTime) {
        for (PlayerState& s : players) {
            energy.advance(s, lastTime, now);
        }
        lastTime = now;
    }
}

void GameController::startMatch() {
    status = GameStatus::Running;
    lastTime = 0;
}

void GameController::initPlayers() {
    skillRevealed = {};
    for (PlayerId p : {PlayerId::Black, PlayerId::White}) {
        PlayerState& s = state(p);
        s = PlayerState{};
        s.energy = config.startEnergy;
        s.skill = *selectedSkill[indexOf(p)];
        skills.initPlayer(s, 0);  // S4
    }
}

// P1：GAME_NOT_RUNNING → OUT_OF_BOARD → OCCUPIED → PLACE_COOLDOWN → NO_ENERGY
ActionResult GameController::submitPlace(const PlaceAction& action, TimeMs now) {
    if (status != GameStatus::Running) {
        return reject(action.player, RejectReason::GameNotRunning, action.pos);
    }
    if (!board.inBounds(action.pos)) {
        return reject(action.player, RejectReason::OutOfBoard, action.pos);
    }
    if (!board.isEmpty(action.pos)) {
        return reject(action.player, RejectReason::Occupied, action.pos);  // P5：先處理到的成功
    }
    PlayerState& s = state(action.player);
    if (s.lastPlaceTime && now - *s.lastPlaceTime < config.placeCooldown) {
        return reject(action.player, RejectReason::PlaceCooldown, action.pos);  // P4：第一子沒有限制
    }
    if (!energy.canConsume(s)) {
        return reject(action.player, RejectReason::NoEnergy, action.pos);
    }

    // P2
    board.set(action.pos, stoneOf(action.player));
    energy.consume(s);
    s.lastPlaceTime = now;

    if (auto line = RuleChecker::findFive(board, action.pos)) {  // W1、W2
        finish(action.player == PlayerId::Black ? GameStatus::BlackWon : GameStatus::WhiteWon, std::move(*line));
    } else if (board.isFull()) {
        finish(GameStatus::Draw, {});  // W4
    } else {
        emit stateChanged();
    }
    return ActionResult{true, std::nullopt};
}

// SA5：GAME_NOT_RUNNING → SKILL_NOT_OWNED → SKILL_COOLDOWN
// SB4：GAME_NOT_RUNNING → SKILL_NOT_OWNED → OUT_OF_BOARD → SKILL_COOLDOWN → INVALID_TARGET
ActionResult GameController::submitSkill(const SkillAction& action, TimeMs now) {
    if (status != GameStatus::Running) {
        return reject(action.player, RejectReason::GameNotRunning, action.target);  // G3
    }
    PlayerState& s = state(action.player);
    if (const auto reason = skills.check(action, s, board, now)) {
        return reject(action.player, *reason, action.target);
    }
    skills.apply(action, s, board, now);  // W3：炸彈不觸發勝負判斷
    skillRevealed[indexOf(action.player)] = true;  // S1：對手從此看得到這項技能
    emit stateChanged();
    return ActionResult{true, std::nullopt};
}

ActionResult GameController::reject(PlayerId player, RejectReason reason, std::optional<Pos> pos) {
    emit actionRejected(player, reason, pos);  // P3：不改變任何狀態
    return ActionResult{false, reason};
}

void GameController::finish(GameStatus result, std::vector<Pos> line) {
    status = result;
    winningLine = std::move(line);
    emit stateChanged();
    emit gameOver(status, winningLine);
}
