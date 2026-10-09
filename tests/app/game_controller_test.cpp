#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "app/game_controller.h"

namespace {

struct Rejection {
    PlayerId player;
    RejectReason reason;
    std::optional<Pos> pos;
};

struct GameOverEvent {
    GameStatus status;
    std::vector<Pos> line;
};

PlaceAction black(int x, int y) {
    return PlaceAction{PlayerId::Black, Pos{x, y}};
}

PlaceAction white(int x, int y) {
    return PlaceAction{PlayerId::White, Pos{x, y}};
}

std::vector<Pos> sorted(std::vector<Pos> v) {
    std::sort(v.begin(), v.end(), [](Pos a, Pos b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return v;
}

}  // namespace

class GameControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        QObject::connect(&controller, &GameController::stateChanged, [this] { ++stateChangedCount; });
        QObject::connect(&controller, &GameController::actionRejected,
                         [this](PlayerId p, RejectReason r, std::optional<Pos> pos) {
                             rejections.push_back({p, r, pos});
                         });
        QObject::connect(&controller, &GameController::gameOver,
                         [this](GameStatus s, std::vector<Pos> line) { gameOvers.push_back({s, line}); });
    }

    // 雙方選技能並確定 → 倒數 → 對局時間 0 開始（G1a、G2）
    void startMatch() {
        controller.selectSkill(PlayerId::Black, SkillId::Accelerate);
        controller.selectSkill(PlayerId::White, SkillId::Bomb);
        controller.confirmSkill(PlayerId::Black);
        controller.confirmSkill(PlayerId::White);
        controller.tick(0);
        ASSERT_EQ(controller.viewFor(PlayerId::Black).status, GameStatus::Running);
    }

    std::optional<RejectReason> reasonOf(const Action& action, TimeMs now) {
        return controller.submit(action, now).reason;
    }

    MatchConfig config;
    GameController controller{config};
    int stateChangedCount = 0;
    std::vector<Rejection> rejections;
    std::vector<GameOverEvent> gameOvers;
};

// ---- G2、G3：對局進行中才接受請求 ----

TEST_F(GameControllerTest, G3_RejectedDuringSkillSelect) {
    EXPECT_EQ(reasonOf(black(7, 7), 0), RejectReason::GameNotRunning);
}

TEST_F(GameControllerTest, G2_G3_RejectedDuringCountdownAcceptedAtZero) {
    controller.selectSkill(PlayerId::Black, SkillId::Accelerate);
    controller.selectSkill(PlayerId::White, SkillId::Accelerate);
    controller.confirmSkill(PlayerId::Black);
    controller.confirmSkill(PlayerId::White);
    controller.tick(-3000);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).status, GameStatus::Countdown);
    EXPECT_EQ(reasonOf(black(7, 7), -1), RejectReason::GameNotRunning);
    EXPECT_TRUE(controller.submit(black(7, 7), 0).accepted);
}

// ---- P1：拒絕原因與檢查順序 ----

TEST_F(GameControllerTest, P1_GameNotRunningCheckedFirst) {
    EXPECT_EQ(reasonOf(black(15, 0), 0), RejectReason::GameNotRunning);
}

TEST_F(GameControllerTest, P1_B3_OutOfBoard) {
    startMatch();
    EXPECT_EQ(reasonOf(black(15, 0), 0), RejectReason::OutOfBoard);
    EXPECT_EQ(reasonOf(black(0, -1), 0), RejectReason::OutOfBoard);
}

TEST_F(GameControllerTest, P1_Occupied) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(7, 7), 0).accepted);
    EXPECT_EQ(reasonOf(white(7, 7), 0), RejectReason::Occupied);
    EXPECT_EQ(reasonOf(black(7, 7), 5000), RejectReason::Occupied);
}

TEST_F(GameControllerTest, P1_OccupiedCheckedBeforeCooldown) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(7, 7), 0).accepted);
    EXPECT_EQ(reasonOf(black(7, 7), 500), RejectReason::Occupied);
}

TEST_F(GameControllerTest, P1_PlaceCooldown) {
    config.regenInterval = 500;
    GameController fast{config};
    fast.selectSkill(PlayerId::Black, SkillId::Accelerate);
    fast.selectSkill(PlayerId::White, SkillId::Accelerate);
    fast.confirmSkill(PlayerId::Black);
    fast.confirmSkill(PlayerId::White);
    fast.tick(0);
    ASSERT_TRUE(fast.submit(black(0, 0), 0).accepted);
    EXPECT_EQ(fast.submit(black(1, 0), 999).reason, RejectReason::PlaceCooldown);  // 能量已回滿 1 格
    EXPECT_TRUE(fast.submit(black(1, 0), 1000).accepted);                          // 剛好 1000 ms 可以
}

TEST_F(GameControllerTest, P1_CooldownCheckedBeforeEnergy) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(0, 0), 0).accepted);  // 能量 1 → 0
    EXPECT_EQ(reasonOf(black(1, 0), 500), RejectReason::PlaceCooldown);
}

TEST_F(GameControllerTest, P1_NoEnergy) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(0, 0), 0).accepted);  // 能量 1 → 0，T = 2000
    EXPECT_EQ(reasonOf(black(1, 0), 1000), RejectReason::NoEnergy);
    EXPECT_TRUE(controller.submit(black(1, 0), 2000).accepted);  // 回滿 1 格
}

// ---- P2–P5 ----

TEST_F(GameControllerTest, P2_AcceptedPlacementUpdatesState) {
    startMatch();
    const ActionResult result = controller.submit(black(3, 4), 0);
    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(result.reason, std::nullopt);

    const PlayerView view = controller.viewFor(PlayerId::Black);
    EXPECT_EQ(view.board.at({3, 4}), Cell::Black);
    EXPECT_EQ(view.self.energy, 0);
    EXPECT_EQ(view.self.lastPlaceTime, std::optional<TimeMs>(0));
}

TEST_F(GameControllerTest, P3_RejectedRequestChangesNothing) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(0, 0), 0).accepted);
    const int changesBefore = stateChangedCount;
    controller.tick(1500);
    const PlayerState before = controller.viewFor(PlayerId::Black).self;

    EXPECT_EQ(reasonOf(black(1, 0), 1500), RejectReason::NoEnergy);
    const PlayerState after = controller.viewFor(PlayerId::Black).self;
    EXPECT_EQ(after.energy, before.energy);
    EXPECT_EQ(after.lastPlaceTime, before.lastPlaceTime);  // 沒有重設下子間隔
    EXPECT_TRUE(controller.viewFor(PlayerId::Black).board.isEmpty({1, 0}));
    EXPECT_EQ(stateChangedCount, changesBefore + 1);  // 只有 tick 那一次
}

TEST_F(GameControllerTest, P4_FirstPlacementIgnoresCooldown) {
    startMatch();
    EXPECT_TRUE(controller.submit(black(7, 7), 0).accepted);
    EXPECT_TRUE(controller.submit(white(8, 8), 0).accepted);
}

TEST_F(GameControllerTest, P5_SecondRequestForSameCellIsOccupied) {
    startMatch();
    EXPECT_TRUE(controller.submit(white(5, 5), 300).accepted);
    EXPECT_EQ(reasonOf(black(5, 5), 300), RejectReason::Occupied);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).board.at({5, 5}), Cell::White);
}

TEST_F(GameControllerTest, P6_RejectionEmitsSignalWithPosition) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(7, 7), 0).accepted);
    controller.submit(white(7, 7), 100);
    ASSERT_EQ(rejections.size(), 1u);
    EXPECT_EQ(rejections[0].player, PlayerId::White);
    EXPECT_EQ(rejections[0].reason, RejectReason::Occupied);
    EXPECT_EQ(rejections[0].pos, std::optional<Pos>(Pos{7, 7}));
}

// ---- W1–W4 ----

class GameControllerWinTest : public GameControllerTest {
protected:
    void SetUp() override {
        config.regenInterval = 500;  // 能量充足，只受下子間隔限制
        GameControllerTest::SetUp();
    }
};

TEST_F(GameControllerWinTest, W1_FiveInARowWinsWithLine) {
    GameController game{config};
    std::vector<GameOverEvent> events;
    QObject::connect(&game, &GameController::gameOver,
                     [&](GameStatus s, std::vector<Pos> line) { events.push_back({s, line}); });
    game.selectSkill(PlayerId::Black, SkillId::Accelerate);
    game.selectSkill(PlayerId::White, SkillId::Accelerate);
    game.confirmSkill(PlayerId::Black);
    game.confirmSkill(PlayerId::White);
    game.tick(0);

    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(game.submit(black(i, 0), i * 1000).accepted);
        ASSERT_TRUE(game.submit(white(i, 5), i * 1000).accepted);
    }
    EXPECT_TRUE(events.empty());
    ASSERT_TRUE(game.submit(black(4, 0), 4000).accepted);

    EXPECT_EQ(game.viewFor(PlayerId::White).status, GameStatus::BlackWon);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].status, GameStatus::BlackWon);
    EXPECT_EQ(sorted(events[0].line), sorted({{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}}));
    EXPECT_EQ(sorted(game.viewFor(PlayerId::Black).winningLine), sorted(events[0].line));
}

TEST_F(GameControllerWinTest, W1_WhiteCanWin) {
    GameController game{config};
    game.selectSkill(PlayerId::Black, SkillId::Bomb);
    game.selectSkill(PlayerId::White, SkillId::Bomb);
    game.confirmSkill(PlayerId::Black);
    game.confirmSkill(PlayerId::White);
    game.tick(0);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(game.submit(white(9, 2 + i), i * 1000).accepted);
    }
    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::WhiteWon);
}

TEST_F(GameControllerWinTest, G3_RequestsRejectedAfterGameOver) {
    GameController game{config};
    game.selectSkill(PlayerId::Black, SkillId::Accelerate);
    game.selectSkill(PlayerId::White, SkillId::Accelerate);
    game.confirmSkill(PlayerId::Black);
    game.confirmSkill(PlayerId::White);
    game.tick(0);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(game.submit(black(i, 0), i * 1000).accepted);
    }
    EXPECT_EQ(game.submit(white(7, 7), 5000).reason, RejectReason::GameNotRunning);
    EXPECT_EQ(game.submit(black(7, 7), 9000).reason, RejectReason::GameNotRunning);
}

// 以 ((x / 2) + y) % 2 塗滿棋盤：任何方向最多連 2 子，下滿也不會有人連五
TEST_F(GameControllerWinTest, W4_FullBoardWithoutFiveIsDraw) {
    GameController game{config};
    std::vector<GameOverEvent> events;
    QObject::connect(&game, &GameController::gameOver,
                     [&](GameStatus s, std::vector<Pos> line) { events.push_back({s, line}); });
    game.selectSkill(PlayerId::Black, SkillId::Accelerate);
    game.selectSkill(PlayerId::White, SkillId::Accelerate);
    game.confirmSkill(PlayerId::Black);
    game.confirmSkill(PlayerId::White);
    game.tick(0);

    std::vector<Pos> blackCells;
    std::vector<Pos> whiteCells;
    for (int y = 0; y < Board::kSize; ++y) {
        for (int x = 0; x < Board::kSize; ++x) {
            (((x / 2) + y) % 2 == 0 ? blackCells : whiteCells).push_back({x, y});
        }
    }
    const std::size_t rounds = std::max(blackCells.size(), whiteCells.size());
    for (std::size_t i = 0; i < rounds; ++i) {
        const TimeMs now = static_cast<TimeMs>(i) * 1000;
        if (i < blackCells.size()) {
            ASSERT_TRUE(game.submit(PlaceAction{PlayerId::Black, blackCells[i]}, now).accepted) << i;
        }
        if (i < whiteCells.size()) {
            ASSERT_TRUE(game.submit(PlaceAction{PlayerId::White, whiteCells[i]}, now).accepted) << i;
        }
    }

    EXPECT_EQ(game.viewFor(PlayerId::Black).status, GameStatus::Draw);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].status, GameStatus::Draw);
    EXPECT_TRUE(events[0].line.empty());
}

// ---- 訊號 ----

TEST_F(GameControllerTest, StateChangedEmittedOnAcceptedPlacement) {
    startMatch();
    const int before = stateChangedCount;
    ASSERT_TRUE(controller.submit(black(7, 7), 0).accepted);
    EXPECT_EQ(stateChangedCount, before + 1);
}

TEST_F(GameControllerTest, P3_RejectionDoesNotEmitStateChanged) {
    startMatch();
    const int before = stateChangedCount;
    controller.submit(black(15, 15), 0);
    EXPECT_EQ(stateChangedCount, before);
}

// ---- E5、E6：viewFor ----

TEST_F(GameControllerTest, E5_ViewForReturnsOnlyOwnState) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(7, 7), 0).accepted);  // 黑方能量 1 → 0

    const PlayerView blackView = controller.viewFor(PlayerId::Black);
    const PlayerView whiteView = controller.viewFor(PlayerId::White);
    EXPECT_EQ(blackView.me, PlayerId::Black);
    EXPECT_EQ(blackView.self.energy, 0);
    EXPECT_EQ(blackView.self.skill, SkillId::Accelerate);
    EXPECT_EQ(whiteView.me, PlayerId::White);
    EXPECT_EQ(whiteView.self.energy, 1);
    EXPECT_EQ(whiteView.self.skill, SkillId::Bomb);
    EXPECT_EQ(whiteView.board.at({7, 7}), Cell::Black);  // 棋盤公開
}

TEST_F(GameControllerTest, E6_ViewHasNextEnergyRatio) {
    startMatch();
    controller.tick(1000);  // T = 2000，進度 1000
    const PlayerView view = controller.viewFor(PlayerId::Black);
    EXPECT_EQ(view.now, 1000);
    EXPECT_EQ(view.self.energy, 1);
    EXPECT_DOUBLE_EQ(view.nextEnergyRatio, 0.5);
    EXPECT_FALSE(view.accelerating);
}

TEST_F(GameControllerTest, E2_EnergyStartsAtOneAndRegensFromZero) {
    startMatch();
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, 1);
    controller.tick(4000);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, 3);
    EXPECT_EQ(controller.viewFor(PlayerId::White).self.energy, 3);
}
