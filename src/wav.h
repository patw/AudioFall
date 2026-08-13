#pragma once
#include <QString>
namespace Wav {
bool writePcm16Mono(const QString &path, const QByteArray &pcm, int sampleRate, QString *error = nullptr);
bool removeSilence(const QString &path, double thresholdDb = -40.0, double minSilenceSeconds = 1.0, double paddingSeconds = .15, QString *error = nullptr);
}
