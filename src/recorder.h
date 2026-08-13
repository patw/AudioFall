#pragma once
#include <QObject>
#include <QAudioDevice>
class QAudioSource;
class QFile;
class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject *parent=nullptr);
    bool start(const QAudioDevice &device, const QString &path, QString *error=nullptr);
    void stop();
    bool isRecording() const { return source_ != nullptr; }
signals:
    void errorOccurred(const QString &message);
private:
    QAudioSource *source_ = nullptr;
    QFile *file_ = nullptr;
    QString path_;
};
