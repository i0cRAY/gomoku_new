#include "net/protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

namespace {

// ---- 列舉 ↔ 字串 ----

template <typename E>
using NameTable = std::initializer_list<std::pair<E, const char*>>;

const NameTable<PlayerId> kPlayerNames = {{PlayerId::Black, "black"}, {PlayerId::White, "white"}};

const NameTable<SkillId> kSkillNames = {
    {SkillId::Bomb, "bomb"}, {SkillId::Dominate, "dominate"}, {SkillId::Destroy, "destroy"}};

const NameTable<MatchMode> kModeNames = {{MatchMode::TimeLimit, "timeLimit"},
                                         {MatchMode::ScoreTarget, "scoreTarget"}};

const NameTable<GameStatus> kStatusNames = {
    {GameStatus::SkillSelect, "skillSelect"}, {GameStatus::Countdown, "countdown"},
    {GameStatus::Running, "running"},         {GameStatus::BlackWon, "blackWon"},
    {GameStatus::WhiteWon, "whiteWon"},       {GameStatus::Draw, "draw"},
    {GameStatus::Aborted, "aborted"}};

const NameTable<RejectReason> kReasonNames = {
    {RejectReason::GameNotRunning, "gameNotRunning"}, {RejectReason::OutOfBoard, "outOfBoard"},
    {RejectReason::DestroyedCell, "destroyedCell"},   {RejectReason::Occupied, "occupied"},
    {RejectReason::RestrictedZone, "restrictedZone"}, {RejectReason::PlaceCooldown, "placeCooldown"},
    {RejectReason::NoEnergy, "noEnergy"},             {RejectReason::SkillNotOwned, "skillNotOwned"},
    {RejectReason::SkillUsedUp, "skillUsedUp"},       {RejectReason::InvalidTarget, "invalidTarget"}};

// 棋盤每列編成一個字串，一格一個字元（B2、B4）
const NameTable<Cell> kCellChars = {
    {Cell::Empty, "."}, {Cell::Black, "B"}, {Cell::White, "W"}, {Cell::Destroyed, "X"}};

template <typename E>
QString nameOf(const NameTable<E>& table, E value) {
    for (const auto& [v, name] : table)
        if (v == value) return QString::fromLatin1(name);
    return {};
}

template <typename E>
std::optional<E> valueOf(const NameTable<E>& table, const QString& name) {
    for (const auto& [v, n] : table)
        if (name == QLatin1String(n)) return v;
    return std::nullopt;
}

// ---- 編碼 ----

QJsonArray toJson(Pos p) { return QJsonArray{p.x, p.y}; }

QJsonArray toJson(const std::vector<Pos>& stones) {
    QJsonArray array;
    for (Pos p : stones) array.append(toJson(p));
    return array;
}

QJsonArray toJson(const std::array<int, 2>& pair) { return QJsonArray{pair[0], pair[1]}; }

QJsonArray toJson(const std::vector<ClearedLine>& lines) {
    QJsonArray array;
    for (const ClearedLine& line : lines)
        array.append(QJsonObject{{"owner", nameOf(kPlayerNames, line.owner)}, {"stones", toJson(line.stones)}});
    return array;
}

QJsonArray toJson(const Board& board) {
    QJsonArray rows;
    for (int y = 0; y < Board::kSize; ++y) {
        QString row;
        for (int x = 0; x < Board::kSize; ++x) row += nameOf(kCellChars, board.at({x, y}));
        rows.append(row);
    }
    return rows;
}

QJsonObject toJson(const PlayerState& s) {
    QJsonObject o{
        {"energy", s.energy},
        {"regenProgress", qint64{s.regenProgress}},
        {"skill", nameOf(kSkillNames, s.skill)},
        {"dominateCharges", s.dominateCharges},
        {"destroyUsed", s.destroyUsed},
        {"score", s.score},
    };
    if (s.lastPlaceTime) o.insert("lastPlaceTime", qint64{*s.lastPlaceTime});
    return o;
}

// N7：只帶對局規則；showAiInfo 與 ai 只有人機模式用，不送
QJsonObject toJson(const MatchConfig& c) {
    const SkillConfig& s = c.skill;
    return QJsonObject{
        {"regenInterval", qint64{c.regenInterval}},
        {"maxEnergy", c.maxEnergy},
        {"startEnergy", c.startEnergy},
        {"placeCooldown", qint64{c.placeCooldown}},
        {"countdown", qint64{c.countdown}},
        {"minLineLength", c.minLineLength},
        {"mode", nameOf(kModeNames, c.mode)},
        {"timeLimit", qint64{c.timeLimit}},
        {"targetScore", c.targetScore},
        {"overtime", qint64{c.overtime}},
        {"boardScore", QJsonObject{{"four", c.boardScore.four}, {"three", c.boardScore.three}}},
        {"skill", QJsonObject{
                      {"bombCost", s.bombCost},
                      {"dominateCost", s.dominateCost},
                      {"destroyCost", s.destroyCost},
                      {"bombSize", s.bombSize},
                      {"dominateStones", s.dominateStones},
                      {"zoneDuration", qint64{s.zoneDuration}},
                      {"zoneRadius", s.zoneRadius},
                      {"destroyRadius", s.destroyRadius},
                  }},
    };
}

QJsonObject toJson(const Action& action) {
    if (const auto* place = std::get_if<PlaceAction>(&action))
        return QJsonObject{{"kind", "place"}, {"player", nameOf(kPlayerNames, place->player)}, {"pos", toJson(place->pos)}};
    const auto& skill = std::get<SkillAction>(action);
    QJsonObject o{{"kind", "skill"}, {"player", nameOf(kPlayerNames, skill.player)}, {"skill", nameOf(kSkillNames, skill.skill)}};
    if (skill.target) o.insert("target", toJson(*skill.target));
    return o;
}

// N4、E5：PlayerView 本身沒有對手的狀態欄位，這裡也只編 PlayerView 有的東西
QJsonObject toJson(const PlayerView& v) {
    QJsonArray zones;
    for (const ZoneCell& z : v.zones)
        zones.append(QJsonObject{{"pos", toJson(z.pos)}, {"owner", nameOf(kPlayerNames, z.owner)},
                                 {"expiresAt", qint64{z.expiresAt}}});
    QJsonObject o{
        {"me", nameOf(kPlayerNames, v.me)},
        {"board", toJson(v.board)},
        {"status", nameOf(kStatusNames, v.status)},
        {"now", qint64{v.now}},
        {"self", toJson(v.self)},
        {"placeCooldownRemaining", qint64{v.placeCooldownRemaining}},
        {"nextEnergyRatio", v.nextEnergyRatio},
        {"countdownRemaining", qint64{v.countdownRemaining}},
        {"scores", toJson(v.scores)},
        {"mode", nameOf(kModeNames, v.mode)},
        {"timeRemaining", qint64{v.timeRemaining}},
        {"overtime", v.overtime},
        {"targetScore", v.targetScore},
        {"zones", zones},
        {"lastClearedLines", toJson(v.lastClearedLines)},
    };
    if (v.opponentSkillRevealed) o.insert("opponentSkillRevealed", nameOf(kSkillNames, *v.opponentSkillRevealed));
    if (v.boardScores) o.insert("boardScores", toJson(*v.boardScores));
    return o;
}

QJsonObject typed(const char* type, QJsonObject o = {}) {
    o.insert("type", QString::fromLatin1(type));
    return o;
}

QJsonObject encode(const HelloMessage& m) { return typed("hello", {{"version", m.version}}); }
QJsonObject encode(const WelcomeMessage& m) {
    return typed("welcome", {{"version", m.version}, {"config", toJson(m.config)},
                             {"yourColor", nameOf(kPlayerNames, m.yourColor)}});
}
QJsonObject encode(const SelectSkillMessage& m) { return typed("select_skill", {{"skill", nameOf(kSkillNames, m.skill)}}); }
QJsonObject encode(const ConfirmSkillMessage&) { return typed("confirm_skill"); }
QJsonObject encode(const OpponentReadyMessage&) { return typed("opponent_ready"); }
QJsonObject encode(const RequestMessage& m) {
    return typed("request", {{"seq", qint64{m.seq}}, {"action", toJson(m.action)}});
}
QJsonObject encode(const RejectMessage& m) {
    return typed("reject", {{"seq", qint64{m.seq}}, {"reason", nameOf(kReasonNames, m.reason)}});
}
QJsonObject encode(const SnapshotMessage& m) { return typed("snapshot", toJson(m.view)); }
QJsonObject encode(const LinesClearedMessage& m) { return typed("linesCleared", {{"lines", toJson(m.lines)}}); }
QJsonObject encode(const PingMessage&) { return typed("ping"); }
QJsonObject encode(const RematchMessage&) { return typed("rematch"); }
QJsonObject encode(const RematchRequestMessage&) { return typed("rematch_request"); }
QJsonObject encode(const RematchReplyMessage& m) { return typed("rematch_reply", {{"accept", m.accept}}); }
QJsonObject encode(const LeaveMessage&) { return typed("leave"); }

// ---- 解碼 ----

// 讀取 JSON 物件的欄位；任何一個欄位缺少或型別不符就記為失敗，最後再看 ok()
class Reader {
public:
    explicit Reader(QJsonObject object) : object(std::move(object)) {}

    bool ok() const { return valid; }
    void fail() { valid = false; }
    bool has(const char* key) const { return object.contains(QLatin1String(key)); }
    QJsonValue value(const char* key) const { return object.value(QLatin1String(key)); }

    qint64 integer(const char* key, qint64 min, qint64 max) { return integer(value(key), min, max); }
    int integer(const char* key) {
        return static_cast<int>(integer(key, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
    }
    TimeMs time(const char* key) { return integer(key, -kMaxExactInteger, kMaxExactInteger); }

    qint64 integer(const QJsonValue& v, qint64 min, qint64 max) {
        if (!v.isDouble()) return failed<qint64>();
        const double d = v.toDouble();
        if (!std::isfinite(d) || std::floor(d) != d || d < static_cast<double>(min) || d > static_cast<double>(max))
            return failed<qint64>();
        return static_cast<qint64>(d);
    }

    bool boolean(const char* key) {
        const QJsonValue v = value(key);
        return v.isBool() ? v.toBool() : failed<bool>();
    }

    double ratio(const char* key) {
        const QJsonValue v = value(key);
        if (!v.isDouble()) return failed<double>();
        const double d = v.toDouble();
        return d >= 0.0 && d <= 1.0 ? d : failed<double>();
    }

    template <typename E>
    E enumeration(const char* key, const NameTable<E>& table) { return enumeration(value(key), table); }

    template <typename E>
    E enumeration(const QJsonValue& v, const NameTable<E>& table) {
        const std::optional<E> e = v.isString() ? valueOf(table, v.toString()) : std::nullopt;
        return e ? *e : failed<E>(table.begin()->first);
    }

    QJsonObject child(const char* key) {
        const QJsonValue v = value(key);
        return v.isObject() ? v.toObject() : failed<QJsonObject>();
    }

    QJsonArray array(const char* key) { return array(value(key)); }
    QJsonArray array(const QJsonValue& v) { return v.isArray() ? v.toArray() : failed<QJsonArray>(); }

    // onBoard：位置要在棋盤內（給收到後要畫出來的位置用；下子請求的越界交給 GameController 判定 OutOfBoard）
    Pos pos(const QJsonValue& v, bool onBoard) {
        const QJsonArray a = array(v);
        if (a.size() != 2) return failed<Pos>();
        const qint64 min = onBoard ? 0 : std::numeric_limits<int>::min();
        const qint64 max = onBoard ? Board::kSize - 1 : std::numeric_limits<int>::max();
        return Pos{static_cast<int>(integer(a[0], min, max)), static_cast<int>(integer(a[1], min, max))};
    }
    Pos pos(const char* key, bool onBoard) { return pos(value(key), onBoard); }

    std::array<int, 2> pair(const char* key) {
        const QJsonArray a = array(key);
        if (a.size() != 2) return failed<std::array<int, 2>>();
        const auto read = [&](const QJsonValue& v) {
            return static_cast<int>(integer(v, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
        };
        return {read(a[0]), read(a[1])};
    }

private:
    static constexpr qint64 kMaxExactInteger = qint64{1} << 53;  // double 能精確表示的整數範圍

    template <typename T>
    T failed(T fallback = T{}) {
        valid = false;
        return fallback;
    }

    QJsonObject object;
    bool valid = true;
};

std::vector<ClearedLine> readLines(Reader& r, const QJsonArray& array) {
    std::vector<ClearedLine> lines;
    for (const QJsonValue& item : array) {
        if (!item.isObject()) {
            r.fail();
            break;
        }
        Reader line(item.toObject());
        ClearedLine cleared{line.enumeration("owner", kPlayerNames), {}};
        for (const QJsonValue& stone : line.array("stones")) cleared.stones.push_back(line.pos(stone, true));
        if (!line.ok()) r.fail();
        lines.push_back(std::move(cleared));
    }
    return lines;
}

Board readBoard(Reader& r, const QJsonArray& rows) {
    Board board;
    if (rows.size() != Board::kSize) {
        r.fail();
        return board;
    }
    for (int y = 0; y < Board::kSize; ++y) {
        const QString row = rows[y].toString();
        if (!rows[y].isString() || row.size() != Board::kSize) {
            r.fail();
            return board;
        }
        for (int x = 0; x < Board::kSize; ++x) {
            const std::optional<Cell> cell = valueOf(kCellChars, QString(row[x]));
            if (!cell) {
                r.fail();
                return board;
            }
            board.set({x, y}, *cell);
        }
    }
    return board;
}

PlayerState readState(Reader& r, const QJsonObject& object) {
    Reader s(object);
    PlayerState state;
    state.energy = s.integer("energy");
    state.regenProgress = s.time("regenProgress");
    if (s.has("lastPlaceTime")) state.lastPlaceTime = s.time("lastPlaceTime");
    state.skill = s.enumeration("skill", kSkillNames);
    state.dominateCharges = s.integer("dominateCharges");
    state.destroyUsed = s.boolean("destroyUsed");
    state.score = s.integer("score");
    if (!s.ok()) r.fail();
    return state;
}

MatchConfig readConfig(Reader& r, const QJsonObject& object) {
    Reader c(object);
    MatchConfig config;  // showAiInfo、ai 不在協定裡，維持預設
    config.regenInterval = c.time("regenInterval");
    config.maxEnergy = c.integer("maxEnergy");
    config.startEnergy = c.integer("startEnergy");
    config.placeCooldown = c.time("placeCooldown");
    config.countdown = c.time("countdown");
    config.minLineLength = c.integer("minLineLength");
    config.mode = c.enumeration("mode", kModeNames);
    config.timeLimit = c.time("timeLimit");
    config.targetScore = c.integer("targetScore");
    config.overtime = c.time("overtime");

    Reader b(c.child("boardScore"));
    config.boardScore.four = b.integer("four");
    config.boardScore.three = b.integer("three");

    Reader s(c.child("skill"));
    config.skill.bombCost = s.integer("bombCost");
    config.skill.dominateCost = s.integer("dominateCost");
    config.skill.destroyCost = s.integer("destroyCost");
    config.skill.bombSize = s.integer("bombSize");
    config.skill.dominateStones = s.integer("dominateStones");
    config.skill.zoneDuration = s.time("zoneDuration");
    config.skill.zoneRadius = s.integer("zoneRadius");
    config.skill.destroyRadius = s.integer("destroyRadius");

    if (!c.ok() || !b.ok() || !s.ok()) r.fail();
    return config;
}

Action readAction(Reader& r, const QJsonObject& object) {
    Reader a(object);
    const PlayerId player = a.enumeration("player", kPlayerNames);
    const QJsonValue kind = a.value("kind");
    Action action = PlaceAction{player, {0, 0}};
    if (kind == QJsonValue("place")) {
        action = PlaceAction{player, a.pos("pos", false)};
    } else if (kind == QJsonValue("skill")) {
        SkillAction skill{player, a.enumeration("skill", kSkillNames), std::nullopt};
        if (a.has("target")) skill.target = a.pos("target", false);
        action = skill;
    } else {
        a.fail();
    }
    if (!a.ok()) r.fail();
    return action;
}

PlayerView readView(Reader& r) {
    PlayerView v;
    v.me = r.enumeration("me", kPlayerNames);
    v.board = readBoard(r, r.array("board"));
    v.status = r.enumeration("status", kStatusNames);
    v.now = r.time("now");
    v.self = readState(r, r.child("self"));
    v.placeCooldownRemaining = r.time("placeCooldownRemaining");
    v.nextEnergyRatio = r.ratio("nextEnergyRatio");
    if (r.has("opponentSkillRevealed")) v.opponentSkillRevealed = r.enumeration("opponentSkillRevealed", kSkillNames);
    v.countdownRemaining = r.time("countdownRemaining");
    v.scores = r.pair("scores");
    v.mode = r.enumeration("mode", kModeNames);
    v.timeRemaining = r.time("timeRemaining");
    v.overtime = r.boolean("overtime");
    if (r.has("boardScores")) v.boardScores = r.pair("boardScores");
    v.targetScore = r.integer("targetScore");
    for (const QJsonValue& item : r.array("zones")) {
        if (!item.isObject()) {
            r.fail();
            break;
        }
        Reader z(item.toObject());
        v.zones.push_back(ZoneCell{z.pos("pos", true), z.enumeration("owner", kPlayerNames), z.time("expiresAt")});
        if (!z.ok()) r.fail();
    }
    v.lastClearedLines = readLines(r, r.array("lastClearedLines"));
    return v;
}

std::optional<Message> decodeObject(Reader& r, const QString& type) {
    if (type == "hello") return HelloMessage{r.integer("version")};
    if (type == "welcome") {
        WelcomeMessage m;
        m.version = r.integer("version");
        m.config = readConfig(r, r.child("config"));
        m.yourColor = r.enumeration("yourColor", kPlayerNames);
        return m;
    }
    if (type == "select_skill") return SelectSkillMessage{r.enumeration("skill", kSkillNames)};
    if (type == "confirm_skill") return ConfirmSkillMessage{};
    if (type == "opponent_ready") return OpponentReadyMessage{};
    if (type == "request") {
        const auto seq = static_cast<std::uint32_t>(r.integer("seq", 0, std::numeric_limits<std::uint32_t>::max()));
        return RequestMessage{seq, readAction(r, r.child("action"))};
    }
    if (type == "reject") {
        const auto seq = static_cast<std::uint32_t>(r.integer("seq", 0, std::numeric_limits<std::uint32_t>::max()));
        return RejectMessage{seq, r.enumeration("reason", kReasonNames)};
    }
    if (type == "snapshot") return SnapshotMessage{readView(r)};
    if (type == "linesCleared") return LinesClearedMessage{readLines(r, r.array("lines"))};
    if (type == "ping") return PingMessage{};
    if (type == "rematch") return RematchMessage{};
    if (type == "rematch_request") return RematchRequestMessage{};
    if (type == "rematch_reply") return RematchReplyMessage{r.boolean("accept")};
    if (type == "leave") return LeaveMessage{};
    return std::nullopt;
}

}  // namespace

QByteArray encodeMessage(const Message& message) {
    const QJsonObject object = std::visit([](const auto& m) { return encode(m); }, message);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

std::optional<Message> decodeMessage(const QByteArray& line) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return std::nullopt;

    Reader reader(document.object());
    const QJsonValue type = reader.value("type");
    if (!type.isString()) return std::nullopt;
    std::optional<Message> message = decodeObject(reader, type.toString());
    if (!reader.ok()) return std::nullopt;
    return message;
}
