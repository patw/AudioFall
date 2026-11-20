#pragma once
#include <QString>
#include <QVector>
namespace Wav {
bool readFloat16kMono(const QString &path, QVector<float> &samples, QString *error = nullptr);
bool writePcm16Mono(const QString &path, const QByteArray &pcm, int sampleRate, QString *error = nullptr);
bool removeSilence(const QString &inputPath, const QString &outputPath, double thresholdDb = -40.0, double minSilenceSeconds = 5.0, double paddingSeconds = .15, QString *error = nullptr);
double durationSeconds(const QString &path, QString *error = nullptr);

}
