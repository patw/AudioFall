#pragma once
#include <QAudioDevice>
#include <QObject>

class QAudioSource;
class QFile;

class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool start(const QAudioDevice &device, const QString &path, QString *error = nullptr);
    // Stops capture, writes the WAV header, and returns false for an empty or
    // failed recording. A failed recording is removed rather than left behind.
    bool stop(QString *error = nullptr);
    bool isRecording() const { return source_ != nullptr; }

signals:
    void errorOccurred(const QString &message);

private:
    void discardCapture();
    QAudioSource *source_ = nullptr;
    QFile *file_ = nullptr;
    QString path_;
};
