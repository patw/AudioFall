#pragma once
#include "config.h"
#include <QObject>

class ProcessingPipeline : public QObject {
    Q_OBJECT
public:
    explicit ProcessingPipeline(AppConfig config, QObject *parent=nullptr);
    void run();
    void clean();
signals:
    void activity(const QString &message);
    void finished(bool success, const QString &message);
private:
    AppConfig config_;
    QString transcribe(const QString &wavPath);
    QString summarize(const QString &prompt);
    QByteArray post(const QUrl &url, const QByteArray &body, const QList<QPair<QByteArray,QByteArray>> &headers, const QByteArray &contentType);
    static QString renderPrompt(const QString &templ, const QString &transcript);
};
