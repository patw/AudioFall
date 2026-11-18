#include "mainwindow.h"
#include "appicon.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("AudioFall");
    app.setOrganizationName("AudioFall");
    app.setWindowIcon(audiofallApplicationIcon());
    MainWindow window;
    window.show();
    return app.exec();
}
