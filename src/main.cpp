#include <QApplication>
#include <QHBoxLayout>
#include <QMainWindow>

#include "app/local_session.h"
#include "ui/board_input.h"
#include "ui/board_view.h"
#include "ui/hud_view.h"

// T12–T13 暫時的測試畫面：自動選好技能並開局。HUD 顯示黑方；左鍵下黑、右鍵下白，Q 使用黑方技能。
// T14 會換成正式的主視窗。
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    const MatchConfig config;
    LocalSession session(config, PlayerId::Black);
    BoardInput input(PlayerId::Black);

    QMainWindow window;
    auto* central = new QWidget;
    auto* layout = new QHBoxLayout(central);
    auto* board = new BoardView;
    auto* hud = new HudView(config);
    layout->addWidget(board, 1);
    layout->addWidget(hud);
    window.setCentralWidget(central);
    window.setWindowTitle(QStringLiteral("即時五子棋"));

    auto refresh = [&] {
        const PlayerView view = session.viewFor(PlayerId::Black);
        input.onViewChanged(view);
        board->setView(view);
        board->setTargeting(input.isTargeting());
        hud->setView(view);
        hud->setTargeting(input.isTargeting());
    };
    auto send = [&](std::optional<Action> action) {
        if (action) {
            session.request(*action);
        }
        refresh();
    };

    QObject::connect(&session, &GameSession::stateChanged, &window, refresh);
    QObject::connect(&session, &GameSession::actionRejected, &window,
                     [&](PlayerId player, RejectReason reason, std::optional<Pos> pos) {
                         if (pos && board->rect().isValid()) {
                             board->flashRejected(*pos);
                         }
                         if (player == PlayerId::Black) {
                             hud->showRejection(reason);
                         }
                     });
    QObject::connect(board, &BoardView::cellClicked, &window, [&](Pos pos, Qt::MouseButton button) {
        if (button == Qt::RightButton) {
            if (input.isTargeting()) {
                input.cancel();
                refresh();
            } else {
                session.request(PlaceAction{PlayerId::White, pos});
            }
            return;
        }
        send(input.onBoardClick(pos, session.viewFor(PlayerId::Black)));
    });
    QObject::connect(hud, &HudView::skillTriggered, &window,
                     [&] { send(input.onSkillKey(session.viewFor(PlayerId::Black))); });
    QObject::connect(hud, &HudView::cancelRequested, &window, [&] {
        input.cancel();
        refresh();
    });

    session.selectSkill(PlayerId::Black, SkillId::Bomb);
    session.selectSkill(PlayerId::White, SkillId::Accelerate);
    session.confirmSkill(PlayerId::Black);
    session.confirmSkill(PlayerId::White);

    window.show();
    return QApplication::exec();
}
