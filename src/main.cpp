#include <QApplication>

#include "ui/main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.resize(1000, 720);
    window.show();
    return QApplication::exec();
}
