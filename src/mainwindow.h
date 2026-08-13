#pragma once
#include "config.h"
#include <QMainWindow>

class QCloseEvent;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QThread;
class Recorder;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    static bool isSafeRecordingFileName(const QString &name, QString *error = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void toggleRecording();
    void process();
    void clean();
    void showSettings();

private:
    void log(const QString &text);
    void setBusy(bool busy);
    void refreshDevices();

    AppConfig config_;
    Recorder *recorder_;
    QComboBox *devices_;
    QPushButton *record_;
    QPushButton *process_;
    QPushButton *clean_;
    QPlainTextEdit *activity_;
    QLabel *status_;
    QThread *processingThread_ = nullptr;
};
