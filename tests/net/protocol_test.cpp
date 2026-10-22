#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <set>
#include <string>
#include <vector>

#include "app/game_controller.h"
#include "net/protocol.h"

namespace {

template <typename T>
T roundTrip(const T& message) {
    const QByteArray line = encodeMessage(Message{message});
    const std::optional<Message> decoded = decodeMessage(line);
    EXPECT_TRUE(decoded.has_value()) << line.toStdString();
    EXPECT_TRUE(decoded && std::holds_alternative<T>(*decoded)) << line.toStdString();
    if (!decoded || !std::holds_alternative<T>(*decoded)) return message;
    return std::get<T>(*decoded);
}

QJsonObject encodedObject(const Message& message) {
    return QJsonDocument::fromJson(encodeMessage(message)).object();
}

std::set<std::string> keysOf(const QJsonObject& object) {
    std::set<std::string> keys;
    for (const QString& key : object.keys()) keys.insert(key.toStdString());
    return keys;
}

void expectSameStones(const std::vector<Pos>& a, const std::vector<Pos>& b) {
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) EXPECT_TRUE(a[i] == b[i]) << "stone " << i;
}

void expectSameLines(const std::vector<ClearedLine>& a, const std::vector<ClearedLine>& b) {
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].owner, b[i].owner);
        expectSameStones(a[i].stones, b[i].stones);
    }
}

void expectSameBoard(const Board& a, const Board& b) {
    for (int y = 0; y < Board::kSize; ++y)
        for (int x = 0; x < Board::kSize; ++x)
            EXPECT_EQ(a.at({x, y}), b.at({x, y})) << "(" << x << ", " << y << ")";
}

void expectSameState(const PlayerState& a, const PlayerState& b) {
    EXPECT_EQ(a.energy, b.energy);
    EXPECT_EQ(a.regenProgress, b.regenProgress);
    EXPECT_EQ(a.lastPlaceTime, b.lastPlaceTime);
    EXPECT_EQ(a.skill, b.skill);
    EXPECT_EQ(a.dominateCharges, b.dominateCharges);
    EXPECT_EQ(a.destroyUsed, b.destroyUsed);
    EXPECT_EQ(a.score, b.score);
}

void expectSameView(const PlayerView& a, const PlayerView& b) {
    EXPECT_EQ(a.me, b.me);
    expectSameBoard(a.board, b.board);
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.now, b.now);
    expectSameState(a.self, b.self);
    EXPECT_EQ(a.placeCooldownRemaining, b.placeCooldownRemaining);
    EXPECT_DOUBLE_EQ(a.nextEnergyRatio, b.nextEnergyRatio);
    EXPECT_EQ(a.opponentSkillRevealed, b.opponentSkillRevealed);
    EXPECT_EQ(a.countdownRemaining, b.countdownRemaining);
    EXPECT_EQ(a.scores, b.scores);
    EXPECT_EQ(a.mode, b.mode);
    EXPECT_EQ(a.timeRemaining, b.timeRemaining);
    EXPECT_EQ(a.overtime, b.overtime);
    EXPECT_EQ(a.boardScores, b.boardScores);
    EXPECT_EQ(a.targetScore, b.targetScore);
    ASSERT_EQ(a.zones.size(), b.zones.size());
    for (std::size_t i = 0; i < a.zones.size(); ++i) {
        EXPECT_TRUE(a.zones[i].pos == b.zones[i].pos);
        EXPECT_EQ(a.zones[i].owner, b.zones[i].owner);
        EXPECT_EQ(a.zones[i].expiresAt, b.zones[i].expiresAt);
    }
    expectSameLines(a.lastClearedLines, b.lastClearedLines);
}

// 每個欄位都不是預設值，才測得出漏編的欄位
MatchConfig customConfig() {
    MatchConfig c;
    c.regenInterval = 3500;
    c.maxEnergy = 8;
    c.startEnergy = 2;
    c.placeCooldown = 700;
    c.countdown = 4000;
    c.minLineLength = 6;
    c.mode = MatchMode::ScoreTarget;
    c.timeLimit = 300000;
    c.targetScore = 35;
    c.overtime = 45000;
    c.boardScore.four = 7;
    c.boardScore.three = 3;
    c.skill.bombCost = 4;
    c.skill.dominateCost = 2;
    c.skill.destroyCost = 3;
    c.skill.bombSize = 3;
    c.skill.dominateStones = 5;
    c.skill.zoneDuration = 4500;
    c.skill.zoneRadius = 2;
    c.skill.destroyRadius = 1;
    return c;
}

PlayerView fullView() {
    PlayerView v;
    v.me = PlayerId::White;
    v.board.set({0, 0}, Cell::Black);
    v.board.set({14, 14}, Cell::White);
    v.board.set({7, 3}, Cell::Destroyed);
    v.board.set({3, 7}, Cell::White);
    v.status = GameStatus::Running;
    v.now = 123456;
    v.self.energy = 7;
    v.self.regenProgress = 1500;
    v.self.lastPlaceTime = 120000;
    v.self.skill = SkillId::Destroy;
    v.self.dominateCharges = 4;
    v.self.destroyUsed = true;
    v.self.score = 11;
    v.placeCooldownRemaining = 250;
    v.nextEnergyRatio = 0.75;
    v.opponentSkillRevealed = SkillId::Dominate;
    v.countdownRemaining = 0;
    v.scores = {9, 11};
    v.mode = MatchMode::TimeLimit;
    v.timeRemaining = 42000;
    v.overtime = true;
    v.boardScores = std::array<int, 2>{5, 3};
    v.targetScore = 20;
    v.zones = {ZoneCell{{6, 6}, PlayerId::Black, 125000}, ZoneCell{{8, 6}, PlayerId::White, 126000}};
    v.lastClearedLines = {ClearedLine{PlayerId::White, {{2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 7}}}};
    return v;
}

}  // namespace

// ---- 格式 ----

TEST(ProtocolTest, T19_EncodesOneLineEndingWithNewline) {
    const QByteArray line = encodeMessage(SnapshotMessage{fullView()});
    ASSERT_FALSE(line.isEmpty());
    EXPECT_EQ(line.back(), '\n');
    EXPECT_EQ(line.count('\n'), 1);
}

TEST(ProtocolTest, T19_TypeNamesMatchDesign) {
    EXPECT_EQ(encodedObject(HelloMessage{}).value("type").toString(), "hello");
    EXPECT_EQ(encodedObject(WelcomeMessage{}).value("type").toString(), "welcome");
    EXPECT_EQ(encodedObject(SelectSkillMessage{SkillId::Dominate}).value("type").toString(), "select_skill");
    EXPECT_EQ(encodedObject(ConfirmSkillMessage{}).value("type").toString(), "confirm_skill");
    EXPECT_EQ(encodedObject(OpponentReadyMessage{}).value("type").toString(), "opponent_ready");
    EXPECT_EQ(encodedObject(RequestMessage{1, PlaceAction{PlayerId::White, {1, 1}}}).value("type").toString(),
              "request");
    EXPECT_EQ(encodedObject(RejectMessage{1, RejectReason::Occupied}).value("type").toString(), "reject");
    EXPECT_EQ(encodedObject(SnapshotMessage{}).value("type").toString(), "snapshot");
    EXPECT_EQ(encodedObject(LinesClearedMessage{}).value("type").toString(), "linesCleared");
    EXPECT_EQ(encodedObject(PingMessage{}).value("type").toString(), "ping");
    EXPECT_EQ(encodedObject(RematchMessage{}).value("type").toString(), "rematch");
    EXPECT_EQ(encodedObject(RematchRequestMessage{}).value("type").toString(), "rematch_request");
    EXPECT_EQ(encodedObject(RematchReplyMessage{true}).value("type").toString(), "rematch_reply");
    EXPECT_EQ(encodedObject(LeaveMessage{}).value("type").toString(), "leave");
}

TEST(ProtocolTest, T19_DecodeAcceptsLineWithoutNewline) {
    QByteArray line = encodeMessage(PingMessage{});
    line.chop(1);
    const auto decoded = decodeMessage(line);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(std::holds_alternative<PingMessage>(*decoded));
}

// ---- N5：版本 ----

TEST(ProtocolTest, N5_ProtocolVersionIsOne) {
    EXPECT_EQ(kProtocolVersion, 1);
}

TEST(ProtocolTest, N5_HelloRoundTrip) {
    EXPECT_EQ(roundTrip(HelloMessage{}).version, kProtocolVersion);
}

TEST(ProtocolTest, N5_HelloWithOtherVersionStillDecodes) {
    // 版本不同要由主機拒絕並顯示提示，所以解碼本身不能丟掉這則訊息
    EXPECT_EQ(roundTrip(HelloMessage{kProtocolVersion + 5}).version, kProtocolVersion + 5);
}

// ---- N7：welcome 帶主機的對局規則 ----

TEST(ProtocolTest, N7_WelcomeRoundTripCarriesMatchRules) {
    const MatchConfig sent = customConfig();
    const WelcomeMessage got = roundTrip(WelcomeMessage{kProtocolVersion, sent, PlayerId::White});
    EXPECT_EQ(got.version, kProtocolVersion);
    EXPECT_EQ(got.yourColor, PlayerId::White);
    const MatchConfig& c = got.config;
    EXPECT_EQ(c.regenInterval, sent.regenInterval);
    EXPECT_EQ(c.maxEnergy, sent.maxEnergy);
    EXPECT_EQ(c.startEnergy, sent.startEnergy);
    EXPECT_EQ(c.placeCooldown, sent.placeCooldown);
    EXPECT_EQ(c.countdown, sent.countdown);
    EXPECT_EQ(c.minLineLength, sent.minLineLength);
    EXPECT_EQ(c.mode, sent.mode);
    EXPECT_EQ(c.timeLimit, sent.timeLimit);
    EXPECT_EQ(c.targetScore, sent.targetScore);
    EXPECT_EQ(c.overtime, sent.overtime);
    EXPECT_EQ(c.boardScore.four, sent.boardScore.four);
    EXPECT_EQ(c.boardScore.three, sent.boardScore.three);
    EXPECT_EQ(c.skill.bombCost, sent.skill.bombCost);
    EXPECT_EQ(c.skill.dominateCost, sent.skill.dominateCost);
    EXPECT_EQ(c.skill.destroyCost, sent.skill.destroyCost);
    EXPECT_EQ(c.skill.bombSize, sent.skill.bombSize);
    EXPECT_EQ(c.skill.dominateStones, sent.skill.dominateStones);
    EXPECT_EQ(c.skill.zoneDuration, sent.skill.zoneDuration);
    EXPECT_EQ(c.skill.zoneRadius, sent.skill.zoneRadius);
    EXPECT_EQ(c.skill.destroyRadius, sent.skill.destroyRadius);
}

TEST(ProtocolTest, N7_WelcomeTimeLimitModeRoundTrip) {
    MatchConfig sent;
    sent.mode = MatchMode::TimeLimit;
    sent.timeLimit = 60000;
    const WelcomeMessage got = roundTrip(WelcomeMessage{kProtocolVersion, sent, PlayerId::Black});
    EXPECT_EQ(got.config.mode, MatchMode::TimeLimit);
    EXPECT_EQ(got.config.timeLimit, 60000);
    EXPECT_EQ(got.yourColor, PlayerId::Black);
}

TEST(ProtocolTest, N7_WelcomeConfigOmitsAiOnlySettings) {
    MatchConfig sent;
    sent.showAiInfo = true;
    const QJsonObject config = encodedObject(WelcomeMessage{kProtocolVersion, sent, PlayerId::White})
                                   .value("config")
                                   .toObject();
    EXPECT_FALSE(config.isEmpty());
    EXPECT_FALSE(config.contains("showAiInfo"));
    EXPECT_FALSE(config.contains("ai"));
}

// ---- S1：技能選擇 ----

TEST(ProtocolTest, S1_SelectSkillRoundTrip) {
    for (SkillId skill : {SkillId::Bomb, SkillId::Dominate, SkillId::Destroy})
        EXPECT_EQ(roundTrip(SelectSkillMessage{skill}).skill, skill);
}

TEST(ProtocolTest, S1_ConfirmSkillAndOpponentReadyRoundTrip) {
    roundTrip(ConfirmSkillMessage{});
    roundTrip(OpponentReadyMessage{});
}

TEST(ProtocolTest, S1_OpponentReadyCarriesNoSkill) {
    EXPECT_EQ(keysOf(encodedObject(OpponentReadyMessage{})), std::set<std::string>{"type"});
}

// ---- N2、N3：request 與 reject ----

TEST(ProtocolTest, N2_PlaceRequestRoundTrip) {
    const RequestMessage got = roundTrip(RequestMessage{4000000000u, PlaceAction{PlayerId::White, {13, 2}}});
    EXPECT_EQ(got.seq, 4000000000u);
    ASSERT_TRUE(std::holds_alternative<PlaceAction>(got.action));
    const auto& place = std::get<PlaceAction>(got.action);
    EXPECT_EQ(place.player, PlayerId::White);
    EXPECT_TRUE(place.pos == (Pos{13, 2}));
}

TEST(ProtocolTest, N2_SkillRequestWithTargetRoundTrip) {
    const RequestMessage got = roundTrip(RequestMessage{7, SkillAction{PlayerId::White, SkillId::Destroy, Pos{4, 9}}});
    EXPECT_EQ(got.seq, 7u);
    ASSERT_TRUE(std::holds_alternative<SkillAction>(got.action));
    const auto& skill = std::get<SkillAction>(got.action);
    EXPECT_EQ(skill.player, PlayerId::White);
    EXPECT_EQ(skill.skill, SkillId::Destroy);
    ASSERT_TRUE(skill.target.has_value());
    EXPECT_TRUE(*skill.target == (Pos{4, 9}));
}

TEST(ProtocolTest, N2_SkillRequestWithoutTargetRoundTrip) {
    const RequestMessage got = roundTrip(RequestMessage{8, SkillAction{PlayerId::Black, SkillId::Dominate, std::nullopt}});
    ASSERT_TRUE(std::holds_alternative<SkillAction>(got.action));
    const auto& skill = std::get<SkillAction>(got.action);
    EXPECT_EQ(skill.player, PlayerId::Black);
    EXPECT_EQ(skill.skill, SkillId::Dominate);
    EXPECT_FALSE(skill.target.has_value());
}

TEST(ProtocolTest, N3_RejectRoundTripForEveryReason) {
    const RejectReason reasons[] = {
        RejectReason::GameNotRunning, RejectReason::OutOfBoard,     RejectReason::DestroyedCell,
        RejectReason::Occupied,       RejectReason::RestrictedZone, RejectReason::PlaceCooldown,
        RejectReason::NoEnergy,       RejectReason::SkillNotOwned,  RejectReason::SkillUsedUp,
        RejectReason::InvalidTarget,
    };
    std::uint32_t seq = 0;
    for (RejectReason reason : reasons) {
        const RejectMessage got = roundTrip(RejectMessage{++seq, reason});
        EXPECT_EQ(got.seq, seq);
        EXPECT_EQ(got.reason, reason);
    }
}

// ---- N4：snapshot ----

TEST(ProtocolTest, N4_SnapshotRoundTripKeepsEveryField) {
    const PlayerView sent = fullView();
    expectSameView(roundTrip(SnapshotMessage{sent}).view, sent);
}

TEST(ProtocolTest, N4_SnapshotRoundTripWithoutOptionalFields) {
    PlayerView sent = fullView();
    sent.self.lastPlaceTime.reset();
    sent.opponentSkillRevealed.reset();
    sent.boardScores.reset();
    sent.zones.clear();
    sent.lastClearedLines.clear();
    sent.status = GameStatus::Countdown;
    sent.countdownRemaining = 1800;
    sent.now = -1800;  // 倒數中對局時間是負的（G2）
    sent.mode = MatchMode::ScoreTarget;
    sent.overtime = false;
    expectSameView(roundTrip(SnapshotMessage{sent}).view, sent);
}

TEST(ProtocolTest, N4_SnapshotRoundTripForEveryStatus) {
    for (GameStatus status : {GameStatus::SkillSelect, GameStatus::Countdown, GameStatus::Running,
                              GameStatus::BlackWon, GameStatus::WhiteWon, GameStatus::Draw,
                              GameStatus::Aborted}) {
        PlayerView sent = fullView();
        sent.status = status;
        EXPECT_EQ(roundTrip(SnapshotMessage{sent}).view.status, status);
    }
}

TEST(ProtocolTest, N4_SnapshotHasOnlyPublicFieldsAndOwnState) {
    const QJsonObject snapshot = encodedObject(SnapshotMessage{fullView()});
    // 頂層只有公開資訊與「自己」的欄位，沒有任何對手（主機方）的狀態（N4、E5）
    const std::set<std::string> allowed = {
        "type", "me", "board", "status", "now", "self", "placeCooldownRemaining", "nextEnergyRatio",
        "opponentSkillRevealed", "countdownRemaining", "scores", "mode", "timeRemaining", "overtime",
        "boardScores", "targetScore", "zones", "lastClearedLines",
    };
    for (const std::string& key : keysOf(snapshot)) EXPECT_TRUE(allowed.count(key)) << "unexpected key " << key;
    EXPECT_EQ(snapshot.value("me").toString(), "white");
    EXPECT_EQ(snapshot.value("self").toObject().value("energy").toInt(), 7);
}

TEST(ProtocolTest, N4_SnapshotOfJoinerHidesHostPrivateInfo) {
    // 主機方（黑）選霸道且還沒用過，加入方（白）選摧毀：快照裡不能出現主機方的技能、能量與霸道次數
    MatchConfig config;
    config.startEnergy = 2;
    config.maxEnergy = 9;
    GameController controller{config};
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);
    controller.selectSkill(PlayerId::White, SkillId::Destroy);
    controller.confirmSkill(PlayerId::Black);
    controller.confirmSkill(PlayerId::White);
    controller.tick(0);
    ASSERT_EQ(controller.viewFor(PlayerId::White).status, GameStatus::Running);
    // 主機方用掉 2 格能量（剩 0），加入方仍是 2 格
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {0, 0}}, 0).accepted);
    ASSERT_TRUE(controller.submit(PlaceAction{PlayerId::Black, {14, 14}}, 600).accepted);
    controller.tick(700);

    const PlayerView view = controller.viewFor(PlayerId::White);
    ASSERT_FALSE(view.opponentSkillRevealed.has_value());
    const QByteArray line = encodeMessage(SnapshotMessage{view});
    EXPECT_FALSE(line.contains("\"dominate\"")) << line.toStdString();

    // 唯一的能量欄位是加入方自己的 2 格，不是主機方的 0 格
    const QJsonObject self = QJsonDocument::fromJson(line).object().value("self").toObject();
    EXPECT_EQ(self.value("energy").toInt(), 2);
    EXPECT_EQ(self.value("skill").toString(), "destroy");
    EXPECT_EQ(line.count("\"energy\""), 1) << line.toStdString();
}

// ---- U11：linesCleared ----

TEST(ProtocolTest, U11_LinesClearedRoundTrip) {
    const std::vector<ClearedLine> sent = {
        ClearedLine{PlayerId::Black, {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}},
        ClearedLine{PlayerId::Black, {{4, 0}, {4, 1}, {4, 2}, {4, 3}, {4, 4}, {4, 5}}},
    };
    expectSameLines(roundTrip(LinesClearedMessage{sent}).lines, sent);
}

TEST(ProtocolTest, U11_EmptyLinesClearedRoundTrip) {
    EXPECT_TRUE(roundTrip(LinesClearedMessage{}).lines.empty());
}

// ---- N6、G4、N8、N9 ----

TEST(ProtocolTest, N6_PingRoundTrip) {
    roundTrip(PingMessage{});
}

TEST(ProtocolTest, G4_RematchRoundTrip) {
    roundTrip(RematchMessage{});
}

TEST(ProtocolTest, N9_RematchRequestRoundTrip) {
    roundTrip(RematchRequestMessage{});
}

TEST(ProtocolTest, N9_RematchReplyRoundTrip) {
    EXPECT_TRUE(roundTrip(RematchReplyMessage{true}).accept);
    EXPECT_FALSE(roundTrip(RematchReplyMessage{false}).accept);
}

TEST(ProtocolTest, N8_LeaveRoundTrip) {
    roundTrip(LeaveMessage{});
}

// ---- 壞掉的訊息不會讓程式崩潰 ----

TEST(ProtocolTest, T19_MalformedLinesReturnNullopt) {
    const std::vector<QByteArray> bad = {
        "",
        "\n",
        "not json",
        "{",
        "{\"type\":\"ping\"",
        "[]",
        "[{\"type\":\"ping\"}]",
        "42",
        "\"ping\"",
        "null",
        "{}",
        "{\"type\":42}",
        "{\"type\":\"unknown\"}",
        QByteArray("\x00\xff\xfe{\"type\":\"ping\"}", 18),
        // 缺欄位或型別不符
        "{\"type\":\"hello\"}",
        "{\"type\":\"hello\",\"version\":\"1\"}",
        "{\"type\":\"hello\",\"version\":1.5}",
        "{\"type\":\"welcome\",\"version\":1,\"yourColor\":\"white\"}",
        "{\"type\":\"welcome\",\"version\":1,\"yourColor\":\"red\",\"config\":{}}",
        "{\"type\":\"select_skill\"}",
        "{\"type\":\"select_skill\",\"skill\":\"speed\"}",
        "{\"type\":\"select_skill\",\"skill\":1}",
        "{\"type\":\"request\",\"seq\":1}",
        "{\"type\":\"request\",\"seq\":-1,\"action\":{\"kind\":\"place\",\"player\":\"white\",\"pos\":[1,1]}}",
        "{\"type\":\"request\",\"seq\":1,\"action\":{\"kind\":\"jump\",\"player\":\"white\",\"pos\":[1,1]}}",
        "{\"type\":\"request\",\"seq\":1,\"action\":{\"kind\":\"place\",\"player\":\"white\",\"pos\":[1]}}",
        "{\"type\":\"request\",\"seq\":1,\"action\":{\"kind\":\"place\",\"player\":\"white\",\"pos\":\"a1\"}}",
        "{\"type\":\"request\",\"seq\":1,\"action\":{\"kind\":\"skill\",\"player\":\"white\"}}",
        "{\"type\":\"reject\",\"seq\":1,\"reason\":\"tired\"}",
        "{\"type\":\"rematch_reply\"}",
        "{\"type\":\"rematch_reply\",\"accept\":\"yes\"}",
        "{\"type\":\"linesCleared\",\"lines\":{}}",
        "{\"type\":\"linesCleared\",\"lines\":[{\"owner\":\"black\",\"stones\":[[1,2],[99,2]]}]}",
        "{\"type\":\"snapshot\"}",
    };
    for (const QByteArray& line : bad)
        EXPECT_FALSE(decodeMessage(line).has_value()) << line.toStdString();
}

TEST(ProtocolTest, T19_SnapshotWithBrokenFieldReturnsNullopt) {
    const QJsonObject good = encodedObject(SnapshotMessage{fullView()});
    ASSERT_TRUE(decodeMessage(QJsonDocument(good).toJson(QJsonDocument::Compact)).has_value());

    auto expectBroken = [&](const QString& key, const QJsonValue& value) {
        QJsonObject broken = good;
        if (value.isUndefined())
            broken.remove(key);
        else
            broken.insert(key, value);
        EXPECT_FALSE(decodeMessage(QJsonDocument(broken).toJson(QJsonDocument::Compact)).has_value())
            << key.toStdString();
    };
    for (const QString& key : good.keys()) {
        if (key == "type" || key == "opponentSkillRevealed" || key == "boardScores") continue;  // 選填欄位
        expectBroken(key, QJsonValue::Undefined);
    }
    expectBroken("board", QJsonArray{"..."});                    // 棋盤大小不對
    expectBroken("scores", QJsonArray{1});                       // 不是兩個分數
    expectBroken("boardScores", QJsonArray{1, 2, 3});
    expectBroken("status", "paused");
    expectBroken("nextEnergyRatio", "full");
    expectBroken("zones", QJsonArray{QJsonObject{{"pos", QJsonArray{15, 0}}, {"owner", "black"}, {"expiresAt", 1}}});
    expectBroken("self", QJsonObject{{"energy", 1}});

    QJsonArray rows = good.value("board").toArray();
    rows[0] = QString(Board::kSize, QChar('?'));                 // 不認得的格子
    expectBroken("board", rows);
}
