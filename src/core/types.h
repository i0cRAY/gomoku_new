#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

using TimeMs = std::int64_t;  // 對局時間（毫秒），由呼叫端傳入

enum class PlayerId : std::uint8_t { Black = 0, White = 1 };

inline PlayerId opponent(PlayerId p) {
    return p == PlayerId::Black ? PlayerId::White : PlayerId::Black;
}

enum class Cell : std::uint8_t { Empty, Black, White, Destroyed };  // spec B2、B4

struct Pos {
    int x;
    int y;
};  // 0–14

inline bool operator==(Pos a, Pos b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Pos a, Pos b) { return !(a == b); }

enum class SkillId : std::uint8_t { Bomb, Dominate, Destroy };  // spec S1（加速已刪除）

enum class RejectReason : std::uint8_t {
    GameNotRunning,
    OutOfBoard,
    DestroyedCell,   // P1-3
    Occupied,
    RestrictedZone,  // P1-5、SZ3
    PlaceCooldown,
    NoEnergy,
    SkillNotOwned,
    SkillUsedUp,     // SX3；技能沒有冷卻，能量不足一律 NoEnergy（S3）
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

// Aborted：斷線（W5）或中途中止（G4a、G4b），不計勝負
enum class GameStatus : std::uint8_t { SkillSelect, Countdown, Running, BlackWon, WhiteWon, Draw, Aborted };

enum class MatchMode : std::uint8_t { TimeLimit, ScoreTarget };  // spec G1b

// 一條被消除的連線（spec W1、W6、U11）
struct ClearedLine {
    PlayerId owner;
    std::vector<Pos> stones;  // 長度即得分（W2）
};

// 霸道產生的禁區格（spec SZ2–SZ4），公開資訊
struct ZoneCell {
    Pos pos;
    PlayerId owner;     // 產生禁區的玩家；限制的是對手
    TimeMs expiresAt;   // 對局時間 ≥ expiresAt 時失效
};
