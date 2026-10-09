#include <QApplication>
#include <QMainWindow>

#include "app/local_session.h"
#include "ui/board_view.h"

// T12 暫時的測試畫面：自動選好技能並開局，左鍵下黑、右鍵下白。T14 會換成正式的主視窗。
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    LocalSession session(MatchConfig{}, PlayerId::Black);
    QMainWindow window;
    auto* board = new BoardView;
    window.setCentralWidget(board);
    window.setWindowTitle(QStringLiteral("即時五子棋"));

    QObject::connect(&session, &GameSession::stateChanged, board,
                     [&] { board->setView(session.viewFor(PlayerId::Black)); });
    QObject::connect(&session, &GameSession::actionRejected, board,
                     [board](PlayerId, RejectReason, std::optional<Pos> pos) {
                         if (pos) {
                             board->flashRejected(*pos);
                         }
                     });
    QObject::connect(board, &BoardView::cellClicked, &session, [&](Pos pos, Qt::MouseButton button) {
        const PlayerId player = button == Qt::RightButton ? PlayerId::White : PlayerId::Black;
        session.request(PlaceAction{player, pos});
    });

    session.selectSkill(PlayerId::Black, SkillId::Accelerate);
    session.selectSkill(PlayerId::White, SkillId::Bomb);
    session.confirmSkill(PlayerId::Black);
    session.confirmSkill(PlayerId::White);

    window.show();
    return QApplication::exec();
}
