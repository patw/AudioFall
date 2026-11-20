#include "pipeline.h"
#include "wav.h"
#include <QDate>
#include <QElapsedTimer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>
#include <stdexcept>

ProcessingPipeline::ProcessingPipeline(AppConfig config, QObject *parent,
                                       TrimFn trim, TranscribeFn transcribeFn, SummarizeFn summarizeFn)
    : QObject(parent), config_(std::move(config)), trimOverride_(std::move(trim)),
      transcribeOverride_(std::move(transcribeFn)), summarizeOverride_(std::move(summarizeFn)) {}

QByteArray ProcessingPipeline::post(const QUrl &url, const QByteArray &body,
                                    const QList<QPair<QByteArray, QByteArray>> &headers,
                                    const QByteArray &contentType) {
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    request.setTransferTimeout(300000);
    for (const auto &header : headers) request.setRawHeader(header.first, header.second);
    QNetworkReply *reply = manager.post(request, body);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray result = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString failure = reply->errorString();
    reply->deleteLater();
    if (status < 200 || status >= 300)
        throw std::runtime_error(QString("HTTP %1: %2").arg(status).arg(QString::fromUtf8(result.isEmpty() ? failure.toUtf8() : result)).toStdString());
    return result;
}

QString ProcessingPipeline::transcribe(const QString &wavPath) {
    if (transcribeOverride_) return transcribeOverride_(wavPath);
    int lastProgress = -1;
    return localTranscriber_.transcribe(wavPath, config_.transcriptionLanguage,
        [this, &lastProgress, &wavPath](int progress) {
            const int rounded = progress / 5 * 5;
            if (rounded == lastProgress) return;
            lastProgress = rounded;
            emit activityUpdated(QString("Transcribing: %1 (%2%)")
                                 .arg(QFileInfo(wavPath).fileName()).arg(rounded));
        });
}

QString ProcessingPipeline::summarize(const QString &prompt) {
    if (summarizeOverride_) return summarizeOverride_(prompt);
    QString base = config_.llmUrl;
    while (base.endsWith('/')) base.chop(1);
    QJsonArray messages{{QJsonObject{{"role", "system"}, {"content", config_.systemMessage}}},
                        {QJsonObject{{"role", "user"}, {"content", prompt}}}};
    const QByteArray reply = post(QUrl(base + "/chat/completions"),
                                  QJsonDocument(QJsonObject{{"model", config_.llmModel}, {"messages", messages}}).toJson(QJsonDocument::Compact),
                                  {{"Authorization", ("Bearer " + config_.llmApiKey).toUtf8()}, {"api-key", config_.llmApiKey.toUtf8()}},
                                  "application/json");
    const QJsonObject response = QJsonDocument::fromJson(reply).object();
    const QJsonArray choices = response.value("choices").toArray();
    lastCompletionTokens_ = response.value("usage").toObject().value("completion_tokens").toInt();
    const QString content = choices.isEmpty() ? QString() : choices.at(0).toObject().value("message").toObject().value("content").toString();
    if (content.isNull()) throw std::runtime_error("LLM response did not include choices[0].message.content");
    return content;
}

QString ProcessingPipeline::renderPrompt(const QString &templ, const QString &transcript) {
    QString out = templ;
    out.replace("{chunk}", transcript);
    return out;
}

void ProcessingPipeline::run() {
    emit activity("Starting transcription and summarization…");
    try {
        QDir dir(config_.outputDir);
        if (!dir.exists() && !QDir().mkpath(config_.outputDir))
            throw std::runtime_error("Could not create output directory");

        for (const auto &name : dir.entryList({"*.wav"}, QDir::Files, QDir::Name)) {
            if (name.endsWith(".trimmed.wav", Qt::CaseInsensitive)) continue;
            const QString wav = dir.filePath(name);
            const QString trimmedWav = wav.left(wav.size() - 4) + ".trimmed.wav";
            const QString transcriptPath = wav.left(wav.size() - 4) + ".tns";
            if (QFile::exists(transcriptPath)) {
                emit activity("Skipping " + name + ", transcript already exists");
                continue;
            }
            emit activity("Trimming silence: " + name);
            QString error;
            const bool trimmed = trimOverride_ ? trimOverride_(wav, trimmedWav, &error) : Wav::removeSilence(wav, trimmedWav, config_.silenceThresholdDb, config_.silenceMinSeconds, .5, &error);
            if (!trimmed) throw std::runtime_error(error.toStdString());
            emit activity("Transcribing: " + name);
            QElapsedTimer transcriptionTimer;
            transcriptionTimer.start();
            const QString text = transcribe(trimmedWav);
            const double audioSeconds = Wav::durationSeconds(trimmedWav);
            const double elapsedSeconds = transcriptionTimer.elapsed() / 1000.0;
            if (audioSeconds > 0.0 && elapsedSeconds > 0.0) {
                emit activityUpdated(QString("Transcribing: %1 (%2x real-time)")
                                     .arg(name).arg(audioSeconds / elapsedSeconds, 0, 'f', 1));
            }
            QSaveFile output(transcriptPath);
            if (!output.open(QIODevice::WriteOnly)) throw std::runtime_error(output.errorString().toStdString());
            output.write(text.toUtf8());
            if (!output.commit()) throw std::runtime_error(output.errorString().toStdString());
        }

        const QString summaryPath = dir.filePath("summary-" + QDate::currentDate().toString("yyyyMMdd") + ".md");
        QString existing;
        QFile existingFile(summaryPath);
        if (existingFile.open(QIODevice::ReadOnly)) existing = QString::fromUtf8(existingFile.readAll());

        for (const auto &name : dir.entryList({"*.tns"}, QDir::Files, QDir::Name)) {
            const QString heading = "# Call Transcript - " + name + "\n";
            if (existing.contains(heading)) {
                emit activity("Skipping " + name + ", already summarized today");
                continue;
            }
            QFile input(dir.filePath(name));
            if (!input.open(QIODevice::ReadOnly)) throw std::runtime_error(input.errorString().toStdString());
            const QString transcript = QString::fromUtf8(input.readAll());
            QString section = heading + "\n";
            emit activity("Summarizing: " + name);
            for (const auto &step : config_.steps) {
                emit activity("  Running step: " + step.name);
                QElapsedTimer completionTimer;
                completionTimer.start();
                lastCompletionTokens_ = 0;
                const QString result = summarize(renderPrompt(step.prompt, transcript));
                const double elapsedSeconds = completionTimer.elapsed() / 1000.0;
                if (lastCompletionTokens_ > 0 && elapsedSeconds > 0.0) {
                    emit activityUpdated(QString("  Running step: %1 (%2 tokens/s)")
                                         .arg(step.name)
                                         .arg(lastCompletionTokens_ / elapsedSeconds, 0, 'f', 1));
                }
                section += "## " + step.name + "\n\n" + result + "\n\n";
            }
            section += "---\n";
            QFile summary(summaryPath);
            if (!summary.open(QIODevice::Append)) throw std::runtime_error(summary.errorString().toStdString());
            if (summary.write(section.toUtf8()) < 0) throw std::runtime_error(summary.errorString().toStdString());
            existing += section;
        }
        emit activity("Processing complete");
        emit finished(true, {});
    } catch (const std::exception &e) {
        emit finished(false, QString::fromUtf8(e.what()));
    }
}

void ProcessingPipeline::clean() {
    QDir dir(config_.outputDir);
    for (const auto &name : dir.entryList({"*.wav", "*.trimmed.wav", "*.tns"}, QDir::Files)) {
        if (QFile::remove(dir.filePath(name))) emit activity("Deleted: " + name);
        else emit activity("Could not delete: " + name);
    }
    emit activity("Cleaning complete.");
}
