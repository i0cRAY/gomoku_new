#include "core/skill_system.h"


SkillSystem::SkillSystem(SkillConfig config) : config(config) {}

void SkillSystem::initPlayer(PlayerState& state) const {
    state.dominateCharges = 0;
    state.destroyUsed = false;
}

namespace {

Cell stoneOf(PlayerId player) {
    return player == PlayerId::Black ? Cell::Black : Cell::White;
}

}  // namespace

// 霸道：SKILL_NOT_OWNED → NO_ENERGY（SZ5，忽略目標座標）
// 炸彈：SKILL_NOT_OWNED → OUT_OF_BOARD（沒有目標時為 INVALID_TARGET）→ NO_ENERGY → INVALID_TARGET（SB4、SB5）
// 摧毀：SKILL_NOT_OWNED → OUT_OF_BOARD（沒有目標時為 INVALID_TARGET）→ SKILL_USED_UP → NO_ENERGY（SX4、SX5）
std::optional<RejectReason> SkillSystem::check(const SkillAction& action, const PlayerState& state,
                                               const Board& board) const {
    if (action.skill != state.skill) {
        return RejectReason::SkillNotOwned;  // S1a
    }
    if (action.skill == SkillId::Bomb || action.skill == SkillId::Destroy) {
        if (!action.target) {
            return RejectReason::InvalidTarget;  // SB5、SX5
        }
        if (!board.inBounds(*action.target)) {
            return RejectReason::OutOfBoard;  // SB2、SX1
        }
    }
    if (action.skill == SkillId::Destroy && state.destroyUsed) {
        return RejectReason::SkillUsedUp;  // SX3
    }
    if (state.energy < config.energyCost) {
        return RejectReason::NoEnergy;  // S3
    }
    if (action.skill == SkillId::Bomb && board.at(*action.target) != stoneOf(opponent(action.player))) {
        return RejectReason::InvalidTarget;  // SB2：空格或自己的棋子
    }
    return std::nullopt;
}

void SkillSystem::apply(const SkillAction& action, PlayerState& state, Board& board, ZoneMap& zones,
                        TimeMs /*now*/) const {
    switch (action.skill) {
        case SkillId::Bomb:
            board.set(*action.target, Cell::Empty);  // SB3
            break;
        case SkillId::Dominate:
            state.dominateCharges = config.dominateStones;  // SZ1：重設，不疊加
            break;
        case SkillId::Destroy:
            destroyArea(*action.target, board, zones);
            state.destroyUsed = true;  // SX3
            break;
    }
}

// SX2：中心 ±radius 範圍（截掉棋盤外）全部變成已摧毀，範圍內的禁區一併移除
void SkillSystem::destroyArea(Pos center, Board& board, ZoneMap& zones) const {
    const int r = config.destroyRadius;
    for (int y = center.y - r; y <= center.y + r; ++y) {
        for (int x = center.x - r; x <= center.x + r; ++x) {
            const Pos p{x, y};
            if (board.inBounds(p)) {
                board.set(p, Cell::Destroyed);
                zones.removeAt(p);
            }
        }
    }
}

void SkillSystem::onPlaced(PlayerId player, PlayerState& state, Pos pos, ZoneMap& zones, TimeMs now) const {
    if (state.dominateCharges <= 0) {
        return;
    }
    --state.dominateCharges;
    constexpr Pos kNeighbors[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};  // 上下左右
    for (Pos d : kNeighbors) {
        const Pos p{pos.x + d.x, pos.y + d.y};
        if (p.x >= 0 && p.x < Board::kSize && p.y >= 0 && p.y < Board::kSize) {
            zones.add(p, player, now + config.zoneDuration);
        }
    }
}
