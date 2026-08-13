#include "recorder.h"
#include "wav.h"
#include <QAudioFormat>
#include <QAudioSource>
#include <QFile>

Recorder::Recorder(QObject *parent) : QObject(parent) {}

bool Recorder::start(const QAudioDevice &device, const QString &path, QString *error) {
    if (source_) {
        if (error) *error = "Recording is already active";
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
    if (!file_->open(QIODevice::WriteOnly)) {
        if (error) *error = file_->errorString();
        file_->deleteLater();
        file_ = nullptr;
        return false;
    }

    source_ = new QAudioSource(device, format, this);
    connect(source_, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::StoppedState && source_ && source_->error() != QAudio::NoError)
            emit errorOccurred("The microphone stream stopped unexpectedly.");
    });
    source_->start(file_);
    path_ = path;
    return true;
}

void Recorder::stop() {
    if (!source_) return;
    source_->stop();
    file_->close();

    QFile raw(path_);
    QByteArray pcm;
    if (raw.open(QIODevice::ReadOnly))
        pcm = raw.readAll();
    else
        emit errorOccurred("Could not read captured audio: " + raw.errorString());

    QString error;
    if (!Wav::writePcm16Mono(path_, pcm, 16000, &error))
        emit errorOccurred("Could not finalize WAV: " + error);

    source_->deleteLater();
    file_->deleteLater();
    source_ = nullptr;
    file_ = nullptr;
}
