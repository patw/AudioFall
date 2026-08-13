#pragma once
#include "config.h"
#include <QObject>
#include <functional>

class ProcessingPipeline : public QObject {
    Q_OBJECT
public:
    using TrimFn = std::function<bool(const QString &, QString *)>;
    using TranscribeFn = std::function<QString(const QString &)>;
    using SummarizeFn = std::function<QString(const QString &)>;

    explicit ProcessingPipeline(AppConfig config, QObject *parent = nullptr,
                                TrimFn trim = {}, TranscribeFn transcribe = {}, SummarizeFn summarize = {});
    void run();
    void clean();

signals:
    void activity(const QString &message);
    void finished(bool success, const QString &message);

private:
    AppConfig config_;
    TrimFn trimOverride_;
    TranscribeFn transcribeOverride_;
    SummarizeFn summarizeOverride_;
    QString transcribe(const QString &wavPath);
    QString summarize(const QString &prompt);
    QByteArray post(const QUrl &url, const QByteArray &body,
                    const QList<QPair<QByteArray, QByteArray>> &headers,
                    const QByteArray &contentType);
    static QString renderPrompt(const QString &templ, const QString &transcript);
};
