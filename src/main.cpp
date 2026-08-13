#include "mainwindow.h"
#include <QApplication>
int main(int argc,char *argv[]){QApplication app(argc,argv);app.setApplicationName("AudioFall");app.setOrganizationName("AudioFall");MainWindow window;window.show();return app.exec();}
