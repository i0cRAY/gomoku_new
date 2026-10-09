#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QMainWindow window;
    window.setWindowTitle(QStringLiteral("即時五子棋"));
    window.resize(800, 600);
    window.show();
    return QApplication::exec();
}
