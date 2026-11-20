#include "mainwindow.h"
#include "appicon.h"
#include <QApplication>
#include <QIcon>
#include "transcriber.h"
#include "config.h"
#include <QCoreApplication>
#include <QTextStream>
#include <cstring>
#include <exception>

int main(int argc, char *argv[]) {
    if (argc == 3 && std::strcmp(argv[1], "--transcribe-file") == 0) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("AudioFall");
        app.setOrganizationName("AudioFall");
        try {
            LocalTranscriber transcriber;
            QTextStream(stdout) << transcriber.transcribe(QString::fromLocal8Bit(argv[2]), loadConfig().transcriptionLanguage) << Qt::endl;
            return 0;
        } catch (const std::exception &error) {
            QTextStream(stderr) << error.what() << Qt::endl;
            return 1;
        }
    }
    QApplication app(argc, argv);
    app.setApplicationName("AudioFall");
    app.setOrganizationName("AudioFall");
    app.setWindowIcon(audiofallApplicationIcon());
    MainWindow window;
    window.show();
    return app.exec();
}
