#pragma once

#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "app/game_session.h"
#include "core/config.h"
#include "ui/board_input.h"
#include "ui/board_view.h"
#include "ui/hud_view.h"
#include "ui/skill_select_view.h"

// 主視窗：主選單 → 技能選擇 → 對局 → 結果（spec G1、G1a、G2、G4、U6、U7、M3）。
// 不保存遊戲狀態：收到 stateChanged 後用 viewFor 取得畫面資料。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QWidget* buildMenuPage();
    QWidget* buildGamePage();

    void startLocalDev();  // M3
    void backToMenu();
    void refresh();
    void onRejected(PlayerId, RejectReason, std::optional<Pos>);
    void onBoardClicked(Pos, Qt::MouseButton);
    void onSkillKey(PlayerId);
    void cancelTargeting();
    void send(std::optional<Action>);

    BoardInput& input(PlayerId p) { return inputs[static_cast<std::size_t>(p)]; }
    HudView* hud(PlayerId p) const { return p == PlayerId::Black ? blackHud : whiteHud; }
    bool isLocalDev() const { return localPlayers.size() > 1; }

    MatchConfig config;  // 放在最前面：其他成員的初始化會用到它
    QStackedWidget* pages;
    QWidget* menuPage;
    QComboBox* intervalBox;
    SkillSelectView* skillPage;
    QWidget* gamePage;
    BoardView* board;
    HudView* blackHud;
    HudView* whiteHud;
    QLabel* banner;
    QPushButton* rematchButton;
    QPushButton* menuButton;

    std::unique_ptr<GameSession> session;
    std::vector<PlayerId> localPlayers;
    PlayerId primary = PlayerId::Black;  // 棋盤與結果畫面以這一方的 PlayerView 為準
    std::array<BoardInput, 2> inputs{BoardInput(PlayerId::Black), BoardInput(PlayerId::White)};
    std::map<PlayerId, SkillId> lastConfirmed;  // G4：再來一局時預設選中上一局的技能
    std::optional<GameStatus> lastStatus;
};
