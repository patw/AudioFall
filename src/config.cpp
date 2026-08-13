#include "config.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

AppConfig AppConfig::defaults() {
    AppConfig c;
    c.outputDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    c.steps = {
        {"Summary", "Call Transcript:\n{chunk}\n\nInstruction: Summarize this call. Do not mention the transcript."},
        {"Facts", "Call Transcript:\n{chunk}\n\nInstruction: List the facts as concise bullet points."},
        {"Sentiment", "Call Transcript:\n{chunk}\n\nInstruction: Summarize sentiment by topic. Do not mention the transcript."}
    };
    return c;
}
static QString configPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/settings.json";
}
AppConfig loadConfig() {
    AppConfig c = AppConfig::defaults(); QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly)) return c;
    const auto o = QJsonDocument::fromJson(f.readAll()).object();
    c.outputDir = o.value("output_dir").toString(c.outputDir);
    c.whisperUrl = o.value("whisper_url").toString(c.whisperUrl);
    c.llmUrl = o.value("llm_url").toString(c.llmUrl);
    c.llmApiKey = o.value("llm_api_key").toString(c.llmApiKey);
    c.llmModel = o.value("llm_model").toString(c.llmModel);
    c.systemMessage = o.value("system_message").toString(c.systemMessage);
    if (o.contains("steps")) { c.steps.clear(); for (const auto &v : o.value("steps").toArray()) { auto s=v.toObject(); c.steps.append({s.value("name").toString(),s.value("prompt").toString()}); } }
    return c;
}
bool saveConfig(const AppConfig &c, QString *error) {
    QJsonArray steps; for (const auto &s : c.steps) steps.append(QJsonObject{{"name",s.name},{"prompt",s.prompt}});
    QJsonObject o{{"output_dir",c.outputDir},{"whisper_url",c.whisperUrl},{"llm_url",c.llmUrl},{"llm_api_key",c.llmApiKey},{"llm_model",c.llmModel},{"system_message",c.systemMessage},{"steps",steps}};
    const QString path=configPath(); QDir().mkpath(QFileInfo(path).dir().path()); QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { if(error) *error=f.errorString(); return false; }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit()) { if(error) *error=f.errorString(); return false; } return true;
}
