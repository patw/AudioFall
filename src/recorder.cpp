#include "recorder.h"
#include "wav.h"
#include <QAudioFormat>
#include <QAudioSource>
#include <QFile>

Recorder::Recorder(QObject *parent) : QObject(parent) {}

Recorder::~Recorder() {
    QString ignored;
    stop(&ignored);
}

bool Recorder::start(const QAudioDevice &device, const QString &path, QString *error) {
    if (source_) {
        if (error) *error = "Recording is already active.";
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!device.isFormatSupported(format)) {
        if (error) *error = "The selected microphone does not support 16 kHz mono signed-16-bit PCM.";
        return false;
    }

    file_ = new QFile(path, this);
    if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = file_->errorString();
        file_->deleteLater();
        file_ = nullptr;
        return false;
    }

    path_ = path;
    source_ = new QAudioSource(device, format, this);
    connect(source_, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (state != QAudio::StoppedState || !source_ || source_->error() == QAudio::NoError)
            return;
        const QString message = "The microphone stream stopped unexpectedly; the recording was discarded.";
        discardCapture();
        emit errorOccurred(message);
    });
    source_->start(file_);

    if (source_->state() == QAudio::StoppedState && source_->error() != QAudio::NoError) {
        const QString message = "Could not start the microphone stream.";
        discardCapture();
        if (error) *error = message;
        return false;
    }
    return true;
}

bool Recorder::stop(QString *error) {
    if (!source_) return true;

    source_->stop();
    file_->close();

    QFile raw(path_);
    if (!raw.open(QIODevice::ReadOnly)) {
        const QString message = "Could not read captured audio: " + raw.errorString();
        discardCapture();
        if (error) *error = message;
        emit errorOccurred(message);
        return false;
    }
    const QByteArray pcm = raw.readAll();
    raw.close();

    if (pcm.isEmpty()) {
        const QString message = "No microphone audio was captured; the empty recording was discarded.";
        discardCapture();
        if (error) *error = message;
        emit errorOccurred(message);
        return false;
    }

    QString wavError;
    if (!Wav::writePcm16Mono(path_, pcm, 16000, &wavError)) {
        const QString message = "Could not finalize WAV: " + wavError;
        discardCapture();
        if (error) *error = message;
        emit errorOccurred(message);
        return false;
    }

    source_->deleteLater();
    file_->deleteLater();
    source_ = nullptr;
    file_ = nullptr;
    path_.clear();
    return true;
}

void Recorder::discardCapture() {
    const QString path = path_;
    if (source_) {
        source_->stop();
        source_->deleteLater();
    }
    if (file_) {
        file_->close();
        file_->deleteLater();
    }
    source_ = nullptr;
    file_ = nullptr;
    path_.clear();
    if (!path.isEmpty()) QFile::remove(path);
}
