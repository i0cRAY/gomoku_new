#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
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
    std::array<int, 2> scores;
};

PlaceAction black(int x, int y) {
    return PlaceAction{PlayerId::Black, Pos{x, y}};
}

PlaceAction white(int x, int y) {
    return PlaceAction{PlayerId::White, Pos{x, y}};
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
                         [this](GameStatus s, std::array<int, 2> scores) { gameOvers.push_back({s, scores}); });
    }

    // 雙方選技能並確定 → 倒數 → 對局時間 0 開始（G1a、G2）
    void startMatch() {
        controller.selectSkill(PlayerId::Black, SkillId::Dominate);
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
    controller.selectSkill(PlayerId::Black, SkillId::Dominate);
    controller.selectSkill(PlayerId::White, SkillId::Dominate);
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

// 開局能量設成滿格，方便在時間 0 就使用霸道、摧毀（S2）
std::unique_ptr<GameController> startedWithSkills(MatchConfig config, SkillId blackSkill, SkillId whiteSkill) {
    config.startEnergy = config.maxEnergy;
    auto game = std::make_unique<GameController>(config);
    game->selectSkill(PlayerId::Black, blackSkill);
    game->selectSkill(PlayerId::White, whiteSkill);
    game->confirmSkill(PlayerId::Black);
    game->confirmSkill(PlayerId::White);
    game->tick(0);
    return game;
}

TEST_F(GameControllerTest, P1_B4_DestroyedCell) {
    auto game = startedWithSkills(config, SkillId::Destroy, SkillId::Bomb);
    ASSERT_TRUE(game->submit(SkillAction{PlayerId::Black, SkillId::Destroy, Pos{7, 7}}, 0).accepted);
    EXPECT_EQ(game->submit(white(7, 7), 0).reason, RejectReason::DestroyedCell);
    EXPECT_EQ(game->submit(black(5, 5), 0).reason, RejectReason::DestroyedCell);
    EXPECT_EQ(game->submit(black(15, 7), 0).reason, RejectReason::OutOfBoard);  // 棋盤外仍先報 OUT_OF_BOARD
}

TEST_F(GameControllerTest, P1_DestroyedCheckedBeforeOccupied) {
    auto game = startedWithSkills(config, SkillId::Destroy, SkillId::Bomb);
    ASSERT_TRUE(game->submit(SkillAction{PlayerId::Black, SkillId::Destroy, Pos{7, 7}}, 0).accepted);
    EXPECT_EQ(game->submit(white(7, 7), 0).reason, RejectReason::DestroyedCell);  // 已摧毀不是空格，但不報 OCCUPIED
}

TEST_F(GameControllerTest, P1_SZ3_RestrictedZone) {
    auto game = startedWithSkills(config, SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(game->submit(SkillAction{PlayerId::Black, SkillId::Dominate, std::nullopt}, 0).accepted);
    ASSERT_TRUE(game->submit(black(7, 7), 0).accepted);
    EXPECT_EQ(game->submit(white(7, 8), 100).reason, RejectReason::RestrictedZone);
    EXPECT_EQ(game->submit(white(7, 7), 100).reason, RejectReason::Occupied);  // 已有棋子先報 OCCUPIED
}

TEST_F(GameControllerTest, P1_RestrictedZoneCheckedBeforeCooldownAndEnergy) {
    auto game = startedWithSkills(config, SkillId::Dominate, SkillId::Bomb);
    ASSERT_TRUE(game->submit(SkillAction{PlayerId::Black, SkillId::Dominate, std::nullopt}, 0).accepted);
    ASSERT_TRUE(game->submit(black(7, 7), 0).accepted);
    ASSERT_TRUE(game->submit(white(0, 0), 0).accepted);  // 白方能量 1 → 0，下子間隔開始
    EXPECT_EQ(game->submit(white(8, 7), 100).reason, RejectReason::RestrictedZone);
    EXPECT_EQ(game->submit(white(9, 7), 100).reason, RejectReason::PlaceCooldown);
}

TEST_F(GameControllerTest, P1_OccupiedCheckedBeforeCooldown) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(7, 7), 0).accepted);
    EXPECT_EQ(reasonOf(black(7, 7), 500), RejectReason::Occupied);
}

TEST_F(GameControllerTest, P1_PlaceCooldown) {
    config.regenInterval = 500;
    GameController fast{config};
    fast.selectSkill(PlayerId::Black, SkillId::Dominate);
    fast.selectSkill(PlayerId::White, SkillId::Dominate);
    fast.confirmSkill(PlayerId::Black);
    fast.confirmSkill(PlayerId::White);
    fast.tick(0);
    ASSERT_TRUE(fast.submit(black(0, 0), 0).accepted);
    EXPECT_EQ(fast.submit(black(1, 0), 499).reason, RejectReason::PlaceCooldown);
    EXPECT_TRUE(fast.submit(black(1, 0), 500).accepted);  // 剛好 500 ms 可以，能量也回滿 1 格
}

TEST_F(GameControllerTest, P1_CooldownCheckedBeforeEnergy) {
    startMatch();
    ASSERT_TRUE(controller.submit(black(0, 0), 0).accepted);  // 能量 1 → 0
    EXPECT_EQ(reasonOf(black(1, 0), 300), RejectReason::PlaceCooldown);
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

TEST_F(GameControllerTest, P1_P4_ViewShowsOwnPlaceCooldownRemaining) {
    startMatch();
    EXPECT_EQ(controller.viewFor(PlayerId::Black).placeCooldownRemaining, 0);  // P4：還沒下過子
    ASSERT_TRUE(controller.submit(black(7, 7), 0).accepted);
    controller.tick(200);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).placeCooldownRemaining, config.placeCooldown - 200);
    EXPECT_EQ(controller.viewFor(PlayerId::White).placeCooldownRemaining, 0);  // 只反映自己的下子間隔
    controller.tick(config.placeCooldown);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).placeCooldownRemaining, 0);
    controller.tick(config.placeCooldown + 300);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).placeCooldownRemaining, 0);  // 不會變成負數
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

// ---- W1–W4（計分制；詳細計分見 scoring_test.cpp）----

class GameControllerWinTest : public GameControllerTest {
protected:
    void SetUp() override {
        config.regenInterval = 500;  // 能量充足，只受下子間隔限制
        GameControllerTest::SetUp();
    }

    std::unique_ptr<GameController> started(const MatchConfig& match) {
        auto game = std::make_unique<GameController>(match);
        QObject::connect(game.get(), &GameController::gameOver,
                         [this](GameStatus s, std::array<int, 2> scores) { gameOvers.push_back({s, scores}); });
        game->selectSkill(PlayerId::Black, SkillId::Dominate);
        game->selectSkill(PlayerId::White, SkillId::Dominate);
        game->confirmSkill(PlayerId::Black);
        game->confirmSkill(PlayerId::White);
        game->tick(0);
        return game;
    }
};

TEST_F(GameControllerWinTest, W1_FiveIsClearedAndGameContinues) {
    auto game = started(config);
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(game->submit(black(i, 0), i * 1000).accepted);
        ASSERT_TRUE(game->submit(white(i, 5), i * 1000).accepted);
    }
    ASSERT_TRUE(game->submit(black(4, 0), 4000).accepted);

    const PlayerView view = game->viewFor(PlayerId::White);
    EXPECT_EQ(view.status, GameStatus::Running);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(view.board.isEmpty({i, 0}));
    }
    EXPECT_EQ(view.board.at({0, 5}), Cell::White);  // 白子不受影響
    EXPECT_EQ(view.scores, (std::array<int, 2>{5, 0}));
    EXPECT_TRUE(gameOvers.empty());
    EXPECT_TRUE(game->submit(white(0, 0), 5000).accepted);  // 被消除的格子可以再下
}

TEST_F(GameControllerWinTest, W1_WhiteCanScore) {
    auto game = started(config);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(game->submit(white(9, 2 + i), i * 1000).accepted);
    }
    EXPECT_EQ(game->viewFor(PlayerId::Black).scores, (std::array<int, 2>{0, 5}));
}

TEST_F(GameControllerWinTest, G3_RequestsRejectedAfterGameOver) {
    MatchConfig match = config;
    match.mode = MatchMode::ScoreTarget;
    match.targetScore = 5;
    auto game = started(match);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(game->submit(black(i, 0), i * 1000).accepted);
    }
    ASSERT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::BlackWon);
    EXPECT_EQ(game->submit(white(7, 7), 5000).reason, RejectReason::GameNotRunning);
    EXPECT_EQ(game->submit(black(7, 7), 9000).reason, RejectReason::GameNotRunning);
}

// 以 ((x / 2) + y) % 2 塗滿棋盤：任何方向最多連 2 子，下滿也不會有人連五
TEST_F(GameControllerWinTest, W4_W9_FullBoardEndsGameByScore) {
    MatchConfig match = config;
    match.timeLimit = 3600000;  // 不讓時限先到
    auto game = started(match);

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
            ASSERT_TRUE(game->submit(PlaceAction{PlayerId::Black, blackCells[i]}, now).accepted) << i;
        }
        if (i < whiteCells.size()) {
            ASSERT_TRUE(game->submit(PlaceAction{PlayerId::White, whiteCells[i]}, now).accepted) << i;
        }
    }

    EXPECT_EQ(game->viewFor(PlayerId::Black).status, GameStatus::Draw);  // 0 : 0 同分
    ASSERT_EQ(gameOvers.size(), 1u);
    EXPECT_EQ(gameOvers[0].status, GameStatus::Draw);
    EXPECT_EQ(gameOvers[0].scores, (std::array<int, 2>{0, 0}));
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
    EXPECT_EQ(blackView.self.skill, SkillId::Dominate);
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
}

TEST_F(GameControllerTest, E2_EnergyStartsAtOneAndRegensFromZero) {
    startMatch();
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, 1);
    controller.tick(4000);
    EXPECT_EQ(controller.viewFor(PlayerId::Black).self.energy, 3);
    EXPECT_EQ(controller.viewFor(PlayerId::White).self.energy, 3);
}
