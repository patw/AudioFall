#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("AudioFall");
    app.setOrganizationName("AudioFall");
    app.setWindowIcon(QIcon(":/icons/audiofall-headphones.png"));
    MainWindow window;
    window.show();
    return app.exec();
}
