#pragma once
#include <QString>
#include <QVector>

struct SummaryStep { QString name; QString prompt; };
struct AppConfig {
    QString outputDir;
    QString whisperUrl = "http://localhost:8081/inference";
    QString llmUrl = "http://localhost:8080/v1";
    QString llmApiKey = "doesntmatter";
    QString llmModel = "whatever";
    QString systemMessage = "You are a friendly assistant that summarizes call transcripts.";
    double silenceThresholdDb = -40.0;
    double silenceMinSeconds = 5.0;
    QVector<SummaryStep> steps;
    static AppConfig defaults();
};

AppConfig loadConfig();
bool saveConfig(const AppConfig &config, QString *error = nullptr);
// Test-only override; an empty path restores the platform-standard location.
void setConfigPathForTesting(const QString &path);
