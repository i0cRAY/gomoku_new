#pragma once

#include <cstdint>
#include <optional>
#include <variant>

using TimeMs = std::int64_t;  // 對局時間（毫秒），由呼叫端傳入

enum class PlayerId : std::uint8_t { Black = 0, White = 1 };

inline PlayerId opponent(PlayerId p) {
    return p == PlayerId::Black ? PlayerId::White : PlayerId::Black;
}

enum class Cell : std::uint8_t { Empty, Black, White };

struct Pos {
    int x;
    int y;
};  // 0–14

enum class SkillId : std::uint8_t { Accelerate, Bomb };

enum class RejectReason : std::uint8_t {
    GameNotRunning,
    OutOfBoard,
    Occupied,
    PlaceCooldown,
    NoEnergy,
    SkillNotOwned,
    SkillCooldown,
    InvalidTarget
};

// 玩家送出的請求
struct PlaceAction {
    PlayerId player;
    Pos pos;
};

struct SkillAction {
    PlayerId player;
    SkillId skill;
    std::optional<Pos> target;
};

using Action = std::variant<PlaceAction, SkillAction>;

// 請求的結果
struct ActionResult {
    bool accepted;
    std::optional<RejectReason> reason;  // accepted == false 時才有值
};

enum class GameStatus : std::uint8_t { SkillSelect, Countdown, Running, BlackWon, WhiteWon, Draw, Aborted };
