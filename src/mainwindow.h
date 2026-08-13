#pragma once
#include "config.h"
#include <QMainWindow>
class QComboBox; class QPushButton; class QPlainTextEdit; class QLabel; class Recorder;
class MainWindow : public QMainWindow {
    Q_OBJECT
public: MainWindow();
private slots: void toggleRecording(); void process(); void clean(); void showSettings();
private:
 void log(const QString &text); void setBusy(bool busy); void refreshDevices();
 AppConfig config_; Recorder *recorder_; QComboBox *devices_; QPushButton *record_; QPushButton *process_; QPushButton *clean_; QPlainTextEdit *activity_; QLabel *status_;
};
