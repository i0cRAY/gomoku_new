#pragma once

#include <QByteArray>

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "core/config.h"
#include "core/player_view.h"
#include "core/types.h"

// 區網協定（design §4.9）：每則訊息是一行 JSON，以 '\n' 結尾。編解碼都是純函式。

// spec N5：與程式發行版本分開，之後每次規則變更就加 1
inline constexpr int kProtocolVersion = 1;

struct HelloMessage {          // 加入方 → 主機
    int version = kProtocolVersion;
};
struct WelcomeMessage {        // 主機 → 加入方（N7）：config 只帶對局規則，不含 showAiInfo、ai
    int version = kProtocolVersion;
    MatchConfig config;
    PlayerId yourColor = PlayerId::White;
};
struct SelectSkillMessage {    // 加入方 → 主機，以最後一次為準（S1）
    SkillId skill;
};
struct ConfirmSkillMessage {};  // 加入方 → 主機
struct OpponentReadyMessage {}; // 主機 → 加入方，不透露選了什麼（S1）
struct RequestMessage {        // 加入方 → 主機（N2、N3）
    std::uint32_t seq;
    Action action;
};
struct RejectMessage {         // 主機 → 加入方
    std::uint32_t seq;
    RejectReason reason;
};
struct SnapshotMessage {       // 主機 → 加入方：viewFor(加入方)（N4、E5）
    PlayerView view;
};
struct LinesClearedMessage {   // 主機 → 加入方（U11）
    std::vector<ClearedLine> lines;
};
struct PingMessage {};           // 雙向（N6）
struct RematchMessage {};        // 雙向：對局結束後的再來一局（G4）
struct RematchRequestMessage {}; // 雙向：倒數或進行中提出重開（N9）
struct RematchReplyMessage {     // 雙向（N9）
    bool accept;
};
struct LeaveMessage {};          // 雙向：回主選單，送出後斷線（N8）

using Message = std::variant<HelloMessage, WelcomeMessage, SelectSkillMessage, ConfirmSkillMessage,
                             OpponentReadyMessage, RequestMessage, RejectMessage, SnapshotMessage,
                             LinesClearedMessage, PingMessage, RematchMessage, RematchRequestMessage,
                             RematchReplyMessage, LeaveMessage>;

// 回傳一行 JSON（含結尾的 '\n'）
QByteArray encodeMessage(const Message&);
// 壞掉的 JSON、未知的 type、缺欄位或型別不符時回傳 nullopt，不會崩潰。結尾的 '\n' 可有可無
std::optional<Message> decodeMessage(const QByteArray& line);
