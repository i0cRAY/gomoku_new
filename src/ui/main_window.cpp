#include "ui/main_window.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QStatusBar>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>

#include "app/host_session.h"
#include "app/local_session.h"
#include "app/remote_session.h"
#include "ui/board_overlay.h"
#include "ui/hud_model.h"
#include "ui/menu_model.h"

namespace {

constexpr int kTitleScale = 3;
constexpr int kBannerScale = 2;
constexpr int kMenuButtonWidth = 260;
constexpr int kStatusMessageMs = 3000;

QString toQString(const std::string& s) {
    return QString::fromStdString(s);
}

void scaleFont(QWidget* widget, int scale) {
    QFont font = widget->font();
    font.setPointSize(font.pointSize() * scale);
    font.setBold(true);
    widget->setFont(font);
}

// 開房時顯示給對手輸入的本機 IPv4 位址
QString localAddresses() {
    QStringList result;
    for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            result << address.toString();
        }
    }
    return result.isEmpty() ? QStringLiteral("127.0.0.1") : result.join(QStringLiteral("、"));
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), pages(new QStackedWidget), skillPage(new SkillSelectView(config.skill)) {
    menuPage = buildMenuPage();
    gamePage = buildGamePage();
    waitPage = buildWaitPage();
    pages->addWidget(menuPage);
    pages->addWidget(skillPage);
    pages->addWidget(gamePage);
    pages->addWidget(waitPage);
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
    connect(skillPage, &SkillSelectView::backToMenuRequested, this, &MainWindow::backToMenu);  // G4b

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

    // G1b：比賽模式（限時 5／3／1 分鐘，或達分自訂目標）
    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(QStringLiteral("比賽模式")));
    modeBox = new QComboBox;
    for (MatchMode mode : {MatchMode::TimeLimit, MatchMode::ScoreTarget}) {
        modeBox->addItem(toQString(matchModeLabel(mode)), static_cast<int>(mode));
    }
    modeBox->setCurrentIndex(modeBox->findData(static_cast<int>(MatchConfig{}.mode)));
    modeRow->addWidget(modeBox);
    timeLimitBox = new QComboBox;
    for (TimeMs limit : timeLimitOptions()) {
        timeLimitBox->addItem(toQString(timeLimitLabel(limit)), QVariant::fromValue<qlonglong>(limit));
    }
    timeLimitBox->setCurrentIndex(static_cast<int>(defaultTimeLimitIndex()));
    modeRow->addWidget(timeLimitBox);
    targetScoreBox = new QSpinBox;
    targetScoreBox->setRange(kMinTargetScore, kMaxTargetScore);
    targetScoreBox->setValue(MatchConfig{}.targetScore);
    targetScoreBox->setSuffix(QStringLiteral(" 分"));
    modeRow->addWidget(targetScoreBox);
    layout->addLayout(modeRow);
    connect(modeBox, &QComboBox::currentIndexChanged, this, &MainWindow::refreshModeWidgets);
    refreshModeWidgets();

    // M1a：只影響人機對戰的畫面
    showAiInfoBox = new QCheckBox(QStringLiteral("顯示 AI 資訊（僅人機對戰）"));
    showAiInfoBox->setChecked(MatchConfig{}.showAiInfo);
    layout->addWidget(showAiInfoBox, 0, Qt::AlignCenter);
    layout->addSpacing(12);

    auto addButton = [&](const QString& text) {
        auto* button = new QPushButton(text);
        button->setFixedWidth(kMenuButtonWidth);
        layout->addWidget(button, 0, Qt::AlignCenter);
        return button;
    };
    auto* vsAI = addButton(QStringLiteral("人機對戰"));
    connect(vsAI, &QPushButton::clicked, this, &MainWindow::startVsAI);
    auto* hostButton = addButton(QStringLiteral("開房（區網）"));
    connect(hostButton, &QPushButton::clicked, this, &MainWindow::startHost);
    auto* joinButton = addButton(QStringLiteral("加入（區網）"));
    connect(joinButton, &QPushButton::clicked, this, &MainWindow::startJoin);
    auto* localDev = addButton(QStringLiteral("本機雙人（開發用）"));
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
    scoreBoard = new ScoreBoard;
    blackHud = new HudView(config, QKeySequence(Qt::Key_Q));
    whiteHud = new HudView(config, QKeySequence(Qt::Key_P));
    rematchButton = new QPushButton(QStringLiteral("再來一局"));
    menuButton = new QPushButton(QStringLiteral("回主選單"));
    side->addWidget(banner);
    side->addWidget(scoreBoard);
    side->addWidget(blackHud);
    side->addWidget(whiteHud);
    side->addStretch();
    side->addWidget(rematchButton);
    side->addWidget(menuButton);
    layout->addLayout(side);

    connect(board, &BoardView::cellClicked, this, &MainWindow::onBoardClicked);
    connect(blackHud, &HudView::skillTriggered, this, [this] { onSkillKey(PlayerId::Black); });
    connect(whiteHud, &HudView::skillTriggered, this, [this] { onSkillKey(PlayerId::White); });
    rematchButton->setFocusPolicy(Qt::NoFocus);
    menuButton->setFocusPolicy(Qt::NoFocus);
    connect(rematchButton, &QPushButton::clicked, this, &MainWindow::onRematchClicked);
    connect(menuButton, &QPushButton::clicked, this, &MainWindow::onMenuClicked);
    return page;
}

QWidget* MainWindow::buildWaitPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    waitLabel = new QLabel;
    waitLabel->setAlignment(Qt::AlignCenter);
    scaleFont(waitLabel, kBannerScale);
    layout->addWidget(waitLabel);
    auto* cancel = new QPushButton(QStringLiteral("取消"));
    cancel->setFixedWidth(kMenuButtonWidth);
    layout->addWidget(cancel, 0, Qt::AlignCenter);
    connect(cancel, &QPushButton::clicked, this, [this] {
        cancelWaiting();
        pages->setCurrentWidget(menuPage);
    });
    return page;
}

void MainWindow::refreshModeWidgets() {
    const bool timeLimited = static_cast<MatchMode>(modeBox->currentData().toInt()) == MatchMode::TimeLimit;
    timeLimitBox->setVisible(timeLimited);
    targetScoreBox->setVisible(!timeLimited);
}

MatchConfig MainWindow::configFromMenu() const {
    MatchConfig result;
    result.regenInterval = intervalBox->currentData().toLongLong();
    result.mode = static_cast<MatchMode>(modeBox->currentData().toInt());
    result.timeLimit = timeLimitBox->currentData().toLongLong();
    result.targetScore = targetScoreBox->value();
    result.showAiInfo = showAiInfoBox->isChecked();
    return result;
}

void MainWindow::startVsAI() {
    config = configFromMenu();
    // E5：只顯示自己的資訊；開啟 M1a 時另外顯示 AI 的唯讀 HUD（U10）
    blackHud->setTitle(config.showAiInfo ? QStringLiteral("你（黑方）") : QString());
    blackHud->show();
    whiteHud->setTitle(QStringLiteral("AI（白方）"));
    whiteHud->setReadOnly(true);
    whiteHud->setVisible(config.showAiInfo);
    startSession({PlayerId::Black}, std::make_unique<LocalSession>(config, PlayerId::Black));
    aiPlayer = PlayerId::White;
    // 先讓技能選擇畫面就緒，AI 確定時才看得到「對手已準備」
    auto* local = static_cast<LocalSession*>(session.get());
    local->attachAI(PlayerId::White, QRandomGenerator::global()->generate());
}

void MainWindow::startLocalDev() {
    config = configFromMenu();
    blackHud->setTitle(QStringLiteral("黑方（左鍵、Q）"));
    blackHud->show();
    whiteHud->setTitle(QStringLiteral("白方（右鍵、P）"));
    whiteHud->setReadOnly(false);
    whiteHud->setSkillKey(QKeySequence(Qt::Key_P));
    whiteHud->show();  // M3：HUD 同時顯示雙方（E5 的例外）
    startSession({PlayerId::Black, PlayerId::White}, std::make_unique<LocalSession>(config, PlayerId::Black));
}

void MainWindow::startHost() {
    config = configFromMenu();
    config.showAiInfo = false;  // M1a 只用在人機對戰
    auto network = std::make_unique<NetworkManager>();
    if (!network->host(config, kDefaultPort)) {
        QMessageBox::warning(this, QStringLiteral("開房"),
                             QStringLiteral("無法在埠 %1 開房，可能已有其他程式在使用。").arg(kDefaultPort));
        return;
    }
    connect(network.get(), &NetworkManager::connected, this, [this] {
        auto ready = std::move(pendingNetwork);
        ready->disconnect(this);
        blackHud->setTitle(QStringLiteral("你（黑方）"));
        blackHud->show();
        whiteHud->hide();  // E5：看不到對手的資訊
        startSession({HostSession::kHostColor}, std::make_unique<HostSession>(config, std::move(ready)), true);
    });
    pendingNetwork = std::move(network);
    waitLabel->setText(QStringLiteral("等待對手加入…\n\n本機 IP：%1\n埠：%2").arg(localAddresses()).arg(kDefaultPort));
    pages->setCurrentWidget(waitPage);
}

void MainWindow::startJoin() {
    bool ok = false;
    const QString text = QInputDialog::getText(this, QStringLiteral("加入（區網）"), QStringLiteral("主機 IP："),
                                               QLineEdit::Normal, lastJoinAddress, &ok)
                             .trimmed();
    if (!ok || text.isEmpty()) {
        return;
    }
    QHostAddress address;
    if (!address.setAddress(text)) {
        QMessageBox::warning(this, QStringLiteral("加入"), QStringLiteral("「%1」不是正確的 IP 位址。").arg(text));
        return;
    }
    lastJoinAddress = text;
    auto network = std::make_unique<NetworkManager>();
    connect(network.get(), &NetworkManager::connected, this, [this](MatchConfig hostConfig, PlayerId color) {
        auto ready = std::move(pendingNetwork);
        ready->disconnect(this);
        config = hostConfig;  // N7：設定由主機決定
        blackHud->hide();     // E5：看不到對手的資訊
        whiteHud->setTitle(QStringLiteral("你（白方）"));
        whiteHud->setReadOnly(false);
        whiteHud->setSkillKey(QKeySequence(Qt::Key_Q));  // U4
        whiteHud->show();
        startSession({color}, std::make_unique<RemoteSession>(config, color, std::move(ready)), true);
    });
    connect(network.get(), &NetworkManager::versionMismatch, this, [this](int hostVersion) {
        cancelWaiting();
        pages->setCurrentWidget(menuPage);
        QMessageBox::warning(this, QStringLiteral("加入"),
                             QStringLiteral("協定版本不同（主機 %1，本機 %2），無法連線。").arg(hostVersion).arg(kProtocolVersion));  // N5
    });
    connect(network.get(), &NetworkManager::disconnected, this, [this, text] {
        cancelWaiting();
        pages->setCurrentWidget(menuPage);
        QMessageBox::warning(this, QStringLiteral("加入"), QStringLiteral("無法連線到 %1。").arg(text));
    });
    pendingNetwork = std::move(network);
    pendingNetwork->join(address, kDefaultPort);
    waitLabel->setText(QStringLiteral("正在連線到 %1…").arg(text));
    pages->setCurrentWidget(waitPage);
}

// 由 NetworkManager 的 signal 呼叫時不能馬上刪除它，所以用 deleteLater
void MainWindow::cancelWaiting() {
    if (!pendingNetwork) {
        return;
    }
    pendingNetwork->disconnect(this);
    pendingNetwork->close();
    pendingNetwork.release()->deleteLater();
}

void MainWindow::startSession(std::vector<PlayerId> players, std::unique_ptr<GameSession> newSession, bool network) {
    localPlayers = std::move(players);
    primary = localPlayers.front();
    aiPlayer.reset();
    lastConfirmed.clear();
    lastStatus.reset();
    networked = network;
    notice.clear();
    blackHud->setConfig(config);
    whiteHud->setConfig(config);
    board->setZoneDuration(config.skill.zoneDuration);

    session = std::move(newSession);
    connect(session.get(), &GameSession::stateChanged, this, &MainWindow::refresh);
    connect(session.get(), &GameSession::linesCleared, board, &BoardView::flashCleared);  // U11
    connect(session.get(), &GameSession::actionRejected, this, &MainWindow::onRejected);
    connect(session.get(), &GameSession::opponentReady, this, [this] { skillPage->setOpponentReady(true); });
    connect(session.get(), &GameSession::rematchRequested, this, &MainWindow::onRematchRequested);  // N9
    connect(session.get(), &GameSession::rematchDeclined, this, [this] {
        statusBar()->showMessage(QStringLiteral("對手拒絕重新開局，本局繼續"), kStatusMessageMs);
    });
    connect(session.get(), &GameSession::opponentLeft, this, [this] { showNotice(QStringLiteral("對手已離開")); });  // N8
    connect(session.get(), &GameSession::connectionLost, this, [this] { showNotice(QStringLiteral("連線中斷")); });  // N6
    refresh();
}

void MainWindow::showNotice(const QString& text) {
    notice = text;
    if (rematchDialog) {
        rematchDialog->close();
    }
    statusBar()->clearMessage();
    refresh();
}

// N9：不用會卡住事件迴圈的對話框，等待回覆期間對局照常進行
void MainWindow::onRematchRequested() {
    if (rematchDialog) {
        rematchDialog->close();
    }
    auto* box = new QMessageBox(QMessageBox::Question, QStringLiteral("重新開局"),
                                QStringLiteral("對手想重新開局，同意嗎？\n同意後本局中止、不計勝負。"),
                                QMessageBox::Yes | QMessageBox::No, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    connect(box, &QMessageBox::finished, this, [this, box] {
        if (session) {
            session->answerRematch(box->clickedButton() == box->button(QMessageBox::Yes));
        }
    });
    rematchDialog = box;
    box->open();
}

bool MainWindow::isLocal(PlayerId player) const {
    return std::find(localPlayers.begin(), localPlayers.end(), player) != localPlayers.end();
}

// G4a、G4b：中止本局（區網會通知對手，N8）後回主選單
void MainWindow::backToMenu() {
    if (session) {
        session->leave();
    }
    if (rematchDialog) {
        rematchDialog->close();
    }
    session.reset();
    cancelWaiting();
    networked = false;
    notice.clear();
    statusBar()->clearMessage();
    board->setView(PlayerView{});  // 停止禁區笑聲等仍在播放的音效（U14）
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

    if (rematchDialog && (view.status == GameStatus::SkillSelect || isFinished(view.status))) {
        rematchDialog->close();  // N9：邀請已經沒有意義
    }
    rematchButton->setEnabled(notice.isEmpty());  // 對手已離開或斷線後不能再來一局

    if (view.status == GameStatus::SkillSelect) {
        if (enteredSkillSelect) {
            skillPage->reset(localPlayers, lastConfirmed);
            statusBar()->clearMessage();
        }
        board->setView(view);  // 停止禁區笑聲等仍在播放的音效（U14）
        pages->setCurrentWidget(skillPage);
        return;
    }

    pages->setCurrentWidget(gamePage);
    board->setView(view);
    scoreBoard->setView(view);
    for (PlayerId p : localPlayers) {
        const PlayerView own = session->viewFor(p);
        input(p).onViewChanged(own);
        hud(p)->setView(own);
        hud(p)->setTargeting(input(p).isTargeting());
    }
    if (aiPlayer && config.showAiInfo) {
        hud(*aiPlayer)->setView(session->viewFor(*aiPlayer));  // M1a：只有 UI 讀 AI 的資訊，AIEngine 不受影響
    }
    std::optional<SkillArea> previewArea;
    bool targeting = false;
    for (PlayerId p : localPlayers) {
        if (input(p).isTargeting()) {
            targeting = true;
            previewArea = skillArea(session->viewFor(p).self.skill, config.skill);  // U3：預覽範圍
        }
    }
    board->setTargeting(targeting, previewArea);

    if (view.status == GameStatus::Countdown) {
        banner->setText(toQString(countdownText(view.countdownRemaining)));
    } else if (!notice.isEmpty()) {
        const QString result = view.status == GameStatus::Aborted
                                   ? QString()
                                   : toQString(finalResultText(view.status, view.scores, view.boardScores));
        banner->setText(result.isEmpty() ? notice : result + QStringLiteral("\n") + notice);  // N6、N8
    } else {
        banner->setText(toQString(finalResultText(view.status, view.scores, view.boardScores)));  // U6；進行中為空字串
    }
    // G4a：倒數、進行中與結束後都一直顯示（U12）
}

void MainWindow::onRematchClicked() {
    if (!session) {
        return;
    }
    if (networked) {
        const GameStatus status = session->viewFor(primary).status;
        if (isFinished(status)) {
            statusBar()->showMessage(QStringLiteral("等待對手也按「再來一局」…"));  // G4
        } else {
            const auto answer = QMessageBox::question(this, QStringLiteral("重新開局"),
                                                      QStringLiteral("要邀請對手重新開局嗎？"));
            if (answer != QMessageBox::Yes) {
                return;
            }
            statusBar()->showMessage(QStringLiteral("已送出重開邀請，等待對手回覆…"), kStatusMessageMs);  // N9
        }
        session->requestRematch();
        return;
    }
    if (!isFinished(session->viewFor(primary).status) && !confirmLeavingMatch(QStringLiteral("重新開局"))) {
        return;
    }
    for (BoardInput& i : inputs) {
        i.cancel();
    }
    session->requestRematch();  // G4、G4a
}

void MainWindow::onMenuClicked() {
    if (session && !isFinished(session->viewFor(primary).status) &&
        !confirmLeavingMatch(QStringLiteral("回到主選單"))) {
        return;
    }
    backToMenu();
}

bool MainWindow::confirmLeavingMatch(const QString& action) {
    const auto answer = QMessageBox::question(
        this, action, QStringLiteral("對局還在進行中，確定要%1嗎？本局不計勝負。").arg(action));
    return answer == QMessageBox::Yes;
}

void MainWindow::onRejected(PlayerId player, RejectReason reason, std::optional<Pos> pos) {
    if (!isLocal(player)) {
        return;  // 只提示本機玩家自己的請求（AI 被拒不顯示）
    }
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
    if (!session || pages->currentWidget() != gamePage || !isLocal(player)) {
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
