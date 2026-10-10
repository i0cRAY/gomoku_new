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
    if (status != GameStatus::Running && status != GameStatus::Countdown) {
        return;  // W7：這次 tick 跨過時限，finish 已經發過 stateChanged
    }
    runAIs(now);
    emit stateChanged();
}

void GameController::attachAI(PlayerId player, AIEngine ai) {
    ais[indexOf(player)] = std::move(ai);
    if (status == GameStatus::SkillSelect) {
        letAIChooseSkill(player);
    }
}

void GameController::letAIChooseSkill(PlayerId player) {
    auto& ai = ais[indexOf(player)];
    selectSkill(player, ai->chooseSkill());  // A2a
    confirmSkill(player);
}

void GameController::runAIs(TimeMs now) {
    for (PlayerId p : {PlayerId::Black, PlayerId::White}) {
        auto& ai = ais[indexOf(p)];
        if (!ai || status != GameStatus::Running) {
            continue;
        }
        if (const auto action = ai->decide(viewFor(p))) {
            submit(*action, now);  // A1：和玩家走同一套規則檢查
        }
    }
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
    if (view.self.lastPlaceTime) {
        view.placeCooldownRemaining = std::max<TimeMs>(0, config.placeCooldown - (lastTime - *view.self.lastPlaceTime));
    }
    view.nextEnergyRatio = energy.nextEnergyRatio(view.self);
    view.countdownRemaining = status == GameStatus::Countdown ? std::max<TimeMs>(0, -lastTime) : 0;
    view.scores = scores();
    view.mode = config.mode;
    view.targetScore = config.targetScore;
    view.timeRemaining = std::clamp<TimeMs>(config.timeLimit - std::max<TimeMs>(lastTime, 0), 0, config.timeLimit);
    view.zones = zones.active(lastTime);
    view.lastClearedLines = lastClearedLines;
    return view;
}

void GameController::restart() {
    // selectedSkill 保留，作為這一局的預設選擇（G4）
    status = GameStatus::SkillSelect;
    board.clear();
    zones.clear();
    players = {};
    confirmed = {};
    skillRevealed = {};
    lastClearedLines.clear();
    lastTime = 0;
    emit stateChanged();
    for (PlayerId p : {PlayerId::Black, PlayerId::White}) {
        if (ais[indexOf(p)]) {
            letAIChooseSkill(p);  // A2a：每一局重新隨機選技能
        }
    }
}

void GameController::abort() {
    if (status == GameStatus::BlackWon || status == GameStatus::WhiteWon || status == GameStatus::Draw ||
        status == GameStatus::Aborted) {
        return;
    }
    finish(GameStatus::Aborted);  // W5：不計勝負
}

void GameController::advanceTo(TimeMs now) {
    if (status == GameStatus::Countdown) {
        if (now < 0) {
            lastTime = std::max(lastTime, now);
            return;
        }
        startMatch();  // G2：倒數結束時對局時間 = 0
    }
    if (status != GameStatus::Running) {
        return;
    }
    const TimeMs target = isTimeLimited() ? std::min(now, config.timeLimit) : now;
    if (target > lastTime) {
        for (PlayerState& s : players) {
            energy.advance(s, lastTime, target);
        }
        lastTime = target;
        zones.removeExpired(lastTime);
    }
    if (isTimeLimited() && lastTime >= config.timeLimit) {
        finishByScore();  // W7：時限那一刻（含）之後結束
    }
}

void GameController::startMatch() {
    status = GameStatus::Running;
    lastTime = 0;
}

void GameController::initPlayers() {
    skillRevealed = {};
    for (auto& ai : ais) {
        if (ai) {
            ai->newGame();
        }
    }
    for (PlayerId p : {PlayerId::Black, PlayerId::White}) {
        PlayerState& s = state(p);
        s = PlayerState{};
        s.energy = config.startEnergy;
        s.skill = *selectedSkill[indexOf(p)];
        skills.initPlayer(s);
    }
}

// P1：GAME_NOT_RUNNING → OUT_OF_BOARD → DESTROYED_CELL → OCCUPIED → RESTRICTED_ZONE → PLACE_COOLDOWN → NO_ENERGY
ActionResult GameController::submitPlace(const PlaceAction& action, TimeMs now) {
    if (status != GameStatus::Running) {
        return reject(action.player, RejectReason::GameNotRunning, action.pos);
    }
    if (!board.inBounds(action.pos)) {
        return reject(action.player, RejectReason::OutOfBoard, action.pos);
    }
    if (board.at(action.pos) == Cell::Destroyed) {
        return reject(action.player, RejectReason::DestroyedCell, action.pos);  // B4
    }
    if (!board.isEmpty(action.pos)) {
        return reject(action.player, RejectReason::Occupied, action.pos);  // P5：先處理到的成功
    }
    if (zones.isRestricted(action.pos, action.player, now)) {
        return reject(action.player, RejectReason::RestrictedZone, action.pos);  // SZ3
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
    skills.onPlaced(action.player, s, action.pos, zones, now);  // SZ2：被消除的子也照樣產生禁區
    clearLines(action.player, action.pos);

    if (config.mode == MatchMode::ScoreTarget && s.score >= config.targetScore) {
        finish(action.player == PlayerId::Black ? GameStatus::BlackWon : GameStatus::WhiteWon);  // W8
    } else if (board.isFull()) {
        finishByScore();  // W4
    } else {
        emit stateChanged();
    }
    return ActionResult{true, std::nullopt};
}

void GameController::clearLines(PlayerId player, Pos last) {
    const auto lines = RuleChecker::findLines(board, last, config.minLineLength);
    if (lines.empty()) {
        return;
    }
    lastClearedLines.clear();
    for (const auto& line : lines) {
        state(player).score += static_cast<int>(line.size());  // W2、W6：每條線各自計分
        lastClearedLines.push_back(ClearedLine{player, line});
    }
    for (const auto& line : lines) {
        for (Pos p : line) {
            board.set(p, Cell::Empty);  // W1：任何一方都可以再下
        }
    }
    emit linesCleared(lastClearedLines);
}

// GAME_NOT_RUNNING 先檢查，其餘依 SB4 / SZ5 / SX4 交給 SkillSystem；通過後扣 3 格能量（S2）
ActionResult GameController::submitSkill(const SkillAction& action, TimeMs now) {
    if (status != GameStatus::Running) {
        return reject(action.player, RejectReason::GameNotRunning, action.target);  // G3
    }
    PlayerState& s = state(action.player);
    if (const auto reason = skills.check(action, s, board)) {
        return reject(action.player, *reason, action.target);
    }
    energy.consume(s, skills.energyCost());  // S2
    skills.apply(action, s, board, zones, now);  // W3：炸彈、摧毀不觸發計分
    skillRevealed[indexOf(action.player)] = true;  // S1：對手從此看得到這項技能
    emit stateChanged();
    return ActionResult{true, std::nullopt};
}

ActionResult GameController::reject(PlayerId player, RejectReason reason, std::optional<Pos> pos) {
    emit actionRejected(player, reason, pos);  // P3：不改變任何狀態
    return ActionResult{false, reason};
}

void GameController::finish(GameStatus result) {
    status = result;
    emit stateChanged();
    emit gameOver(status, scores());
}

void GameController::finishByScore() {
    const auto [black, white] = scores();
    finish(black > white ? GameStatus::BlackWon : white > black ? GameStatus::WhiteWon : GameStatus::Draw);  // W9
}
