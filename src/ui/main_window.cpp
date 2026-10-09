#include "ui/main_window.h"

#include <QHBoxLayout>
#include <QShortcut>
#include <QVBoxLayout>

#include "app/local_session.h"
#include "ui/hud_model.h"
#include "ui/menu_model.h"

namespace {

constexpr int kTitleScale = 3;
constexpr int kBannerScale = 2;
constexpr int kMenuButtonWidth = 260;

QString toQString(const std::string& s) {
    return QString::fromStdString(s);
}

void scaleFont(QWidget* widget, int scale) {
    QFont font = widget->font();
    font.setPointSize(font.pointSize() * scale);
    font.setBold(true);
    widget->setFont(font);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), pages(new QStackedWidget), skillPage(new SkillSelectView(config.skill)) {
    menuPage = buildMenuPage();
    gamePage = buildGamePage();
    pages->addWidget(menuPage);
    pages->addWidget(skillPage);
    pages->addWidget(gamePage);
    setCentralWidget(pages);
    setWindowTitle(QStringLiteral("即時五子棋"));

    connect(skillPage, &SkillSelectView::skillSelected, this, [this](PlayerId p, SkillId s) {
        if (session) {
            session->selectSkill(p, s);
        }
    });
    connect(skillPage, &SkillSelectView::skillConfirmed, this, [this](PlayerId p, SkillId s) {
        lastConfirmed[p] = s;
        if (session) {
            session->confirmSkill(p);
        }
    });

    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, &MainWindow::cancelTargeting);  // U3
}

QWidget* MainWindow::buildMenuPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel(QStringLiteral("即時五子棋"));
    scaleFont(title, kTitleScale);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    layout->addSpacing(24);

    // G1：回能間隔
    auto* intervalRow = new QHBoxLayout;
    intervalRow->addWidget(new QLabel(QStringLiteral("回能間隔")));
    intervalBox = new QComboBox;
    for (TimeMs interval : regenIntervalOptions()) {
        intervalBox->addItem(toQString(regenIntervalLabel(interval)), QVariant::fromValue<qlonglong>(interval));
    }
    intervalBox->setCurrentIndex(static_cast<int>(defaultRegenIntervalIndex()));
    intervalRow->addWidget(intervalBox);
    layout->addLayout(intervalRow);
    layout->addSpacing(12);

    auto addButton = [&](const QString& text, bool enabled) {
        auto* button = new QPushButton(text);
        button->setFixedWidth(kMenuButtonWidth);
        button->setEnabled(enabled);
        if (!enabled) {
            button->setToolTip(QStringLiteral("尚未實作"));
        }
        layout->addWidget(button, 0, Qt::AlignCenter);
        return button;
    };
    addButton(QStringLiteral("人機對戰"), false);   // T18
    addButton(QStringLiteral("開房（區網）"), false);  // T20
    addButton(QStringLiteral("加入（區網）"), false);  // T21
    auto* localDev = addButton(QStringLiteral("本機雙人（開發用）"), true);
    connect(localDev, &QPushButton::clicked, this, &MainWindow::startLocalDev);
    return page;
}

QWidget* MainWindow::buildGamePage() {
    auto* page = new QWidget;
    auto* layout = new QHBoxLayout(page);
    board = new BoardView;
    layout->addWidget(board, 1);

    auto* side = new QVBoxLayout;
    banner = new QLabel;
    scaleFont(banner, kBannerScale);
    banner->setAlignment(Qt::AlignCenter);
    blackHud = new HudView(config, QKeySequence(Qt::Key_Q));
    whiteHud = new HudView(config, QKeySequence(Qt::Key_P));
    rematchButton = new QPushButton(QStringLiteral("再來一局"));
    menuButton = new QPushButton(QStringLiteral("回主選單"));
    side->addWidget(banner);
    side->addWidget(blackHud);
    side->addWidget(whiteHud);
    side->addStretch();
    side->addWidget(rematchButton);
    side->addWidget(menuButton);
    layout->addLayout(side);

    connect(board, &BoardView::cellClicked, this, &MainWindow::onBoardClicked);
    connect(blackHud, &HudView::skillTriggered, this, [this] { onSkillKey(PlayerId::Black); });
    connect(whiteHud, &HudView::skillTriggered, this, [this] { onSkillKey(PlayerId::White); });
    connect(rematchButton, &QPushButton::clicked, this, [this] {
        if (session) {
            session->requestRematch();  // G4
        }
    });
    connect(menuButton, &QPushButton::clicked, this, &MainWindow::backToMenu);
    return page;
}

void MainWindow::startLocalDev() {
    config = MatchConfig{};
    config.regenInterval = intervalBox->currentData().toLongLong();
    localPlayers = {PlayerId::Black, PlayerId::White};
    primary = PlayerId::Black;
    lastConfirmed.clear();
    lastStatus.reset();

    blackHud->setConfig(config);
    whiteHud->setConfig(config);
    blackHud->setTitle(QStringLiteral("黑方（左鍵、Q）"));
    whiteHud->setTitle(QStringLiteral("白方（右鍵、P）"));
    whiteHud->show();  // M3：HUD 同時顯示雙方（E5 的例外）

    session = std::make_unique<LocalSession>(config, PlayerId::Black);
    connect(session.get(), &GameSession::stateChanged, this, &MainWindow::refresh);
    connect(session.get(), &GameSession::actionRejected, this, &MainWindow::onRejected);
    connect(session.get(), &GameSession::opponentReady, this, [this] { skillPage->setOpponentReady(true); });
    refresh();
}

void MainWindow::backToMenu() {
    session.reset();
    for (BoardInput& i : inputs) {
        i.cancel();
    }
    pages->setCurrentWidget(menuPage);
}

void MainWindow::refresh() {
    if (!session) {
        return;
    }
    const PlayerView view = session->viewFor(primary);
    const bool enteredSkillSelect = view.status == GameStatus::SkillSelect && lastStatus != GameStatus::SkillSelect;
    lastStatus = view.status;

    if (view.status == GameStatus::SkillSelect) {
        if (enteredSkillSelect) {
            skillPage->reset(localPlayers, lastConfirmed);
        }
        pages->setCurrentWidget(skillPage);
        return;
    }

    pages->setCurrentWidget(gamePage);
    board->setView(view);
    for (PlayerId p : localPlayers) {
        const PlayerView own = session->viewFor(p);
        input(p).onViewChanged(own);
        hud(p)->setView(own);
        hud(p)->setTargeting(input(p).isTargeting());
    }
    board->setTargeting(input(PlayerId::Black).isTargeting() || input(PlayerId::White).isTargeting());

    if (view.status == GameStatus::Countdown) {
        banner->setText(toQString(countdownText(view.countdownRemaining)));
    } else {
        banner->setText(toQString(resultText(view.status)));  // U6；進行中為空字串
    }
    const bool finished = isFinished(view.status);
    rematchButton->setVisible(finished);
    menuButton->setVisible(finished);
}

void MainWindow::onRejected(PlayerId player, RejectReason reason, std::optional<Pos> pos) {
    if (pos && pages->currentWidget() == gamePage) {
        board->flashRejected(*pos);  // P6
    }
    hud(player)->showRejection(reason);  // U5
}

// 一般：左鍵依輸入模式下子或送出炸彈，右鍵取消選目標（U1、U3）
// M3：左鍵屬於黑方、右鍵屬於白方；黑方正在選目標時，右鍵先當取消
void MainWindow::onBoardClicked(Pos pos, Qt::MouseButton button) {
    if (!session) {
        return;
    }
    if (button == Qt::LeftButton) {
        send(input(primary).onBoardClick(pos, session->viewFor(primary)));
        return;
    }
    if (button != Qt::RightButton) {
        return;
    }
    if (isLocalDev() && !input(PlayerId::Black).isTargeting()) {
        send(input(PlayerId::White).onBoardClick(pos, session->viewFor(PlayerId::White)));
        return;
    }
    cancelTargeting();
}

void MainWindow::onSkillKey(PlayerId player) {
    if (!session || pages->currentWidget() != gamePage) {
        return;
    }
    send(input(player).onSkillKey(session->viewFor(player)));
}

void MainWindow::cancelTargeting() {
    for (BoardInput& i : inputs) {
        i.cancel();
    }
    refresh();
}

void MainWindow::send(std::optional<Action> action) {
    if (action && session) {
        session->request(*action);
    }
    refresh();
}
