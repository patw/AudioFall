#include "pipeline.h"
#include "wav.h"
#include <QDate>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

ProcessingPipeline::ProcessingPipeline(AppConfig config,QObject *parent):QObject(parent),config_(std::move(config)) {}
QByteArray ProcessingPipeline::post(const QUrl &url,const QByteArray &body,const QList<QPair<QByteArray,QByteArray>> &headers,const QByteArray &contentType) {
 QNetworkAccessManager manager; QNetworkRequest request(url);request.setHeader(QNetworkRequest::ContentTypeHeader,contentType);request.setTransferTimeout(300000);for(auto h:headers)request.setRawHeader(h.first,h.second);QNetworkReply *reply=manager.post(request,body);QEventLoop loop;connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);loop.exec();QByteArray result=reply->readAll();int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();QString failure=reply->errorString();reply->deleteLater();if(status<200||status>=300)throw std::runtime_error(QString("HTTP %1: %2").arg(status).arg(QString::fromUtf8(result.isEmpty()?failure.toUtf8():result)).toStdString());return result;
}
QString ProcessingPipeline::transcribe(const QString &wavPath) {
 QFile *file=new QFile(wavPath);if(!file->open(QIODevice::ReadOnly))throw std::runtime_error(file->errorString().toStdString());QNetworkAccessManager manager;QNetworkRequest request(QUrl(config_.whisperUrl));request.setTransferTimeout(300000);auto *multi=new QHttpMultiPart(QHttpMultiPart::FormDataType);QHttpPart temperature;temperature.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"temperature\"");temperature.setBody("0.0");multi->append(temperature);QHttpPart format;format.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"response_format\"");format.setBody("json");multi->append(format);QHttpPart audio;audio.setHeader(QNetworkRequest::ContentTypeHeader,"audio/wav");audio.setHeader(QNetworkRequest::ContentDispositionHeader,QString("form-data; name=\"file\"; filename=\"%1\"").arg(QFileInfo(wavPath).fileName()));audio.setBodyDevice(file);file->setParent(multi);multi->append(audio);QNetworkReply *reply=manager.post(request,multi);multi->setParent(reply);QEventLoop loop;connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);loop.exec();QByteArray result=reply->readAll();int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();QString failure=reply->errorString();reply->deleteLater();if(status<200||status>=300)throw std::runtime_error(QString("Whisper HTTP %1: %2").arg(status).arg(QString::fromUtf8(result.isEmpty()?failure.toUtf8():result)).toStdString());QString text=QJsonDocument::fromJson(result).object().value("text").toString();if(text.isNull())throw std::runtime_error("Whisper response did not include text");return text;
}
QString ProcessingPipeline::summarize(const QString &prompt) {
 QString base=config_.llmUrl;while(base.endsWith('/'))base.chop(1);QJsonArray messages{{QJsonObject{{"role","system"},{"content",config_.systemMessage}}},{QJsonObject{{"role","user"},{"content",prompt}}}};QByteArray reply=post(QUrl(base+"/chat/completions"),QJsonDocument(QJsonObject{{"model",config_.llmModel},{"messages",messages}}).toJson(QJsonDocument::Compact),{{"Authorization",("Bearer "+config_.llmApiKey).toUtf8()},{"api-key",config_.llmApiKey.toUtf8()}},"application/json");QJsonArray choices=QJsonDocument::fromJson(reply).object().value("choices").toArray();QString content=choices.isEmpty()?QString():choices.at(0).toObject().value("message").toObject().value("content").toString();if(content.isNull())throw std::runtime_error("LLM response did not include choices[0].message.content");return content;
}
QString ProcessingPipeline::renderPrompt(const QString &templ,const QString &transcript) { QString out=templ;out.replace("{chunk}",transcript);return out; }
void ProcessingPipeline::run() {
 try {QDir dir(config_.outputDir);if(!dir.exists()&&!QDir().mkpath(config_.outputDir))throw std::runtime_error("Could not create output directory");for(const auto &name:dir.entryList({"*.wav"},QDir::Files,QDir::Name)){QString wav=dir.filePath(name),tns=wav.left(wav.size()-4)+".tns";if(QFile::exists(tns)){emit activity("Skipping "+name+", transcript already exists");continue;}emit activity("Trimming silence: "+name);QString error;if(!Wav::removeSilence(wav,-40,1,.15,&error))throw std::runtime_error(error.toStdString());emit activity("Transcribing: "+name);QString text=transcribe(wav);QSaveFile output(tns);if(!output.open(QIODevice::WriteOnly))throw std::runtime_error(output.errorString().toStdString());output.write(text.toUtf8());if(!output.commit())throw std::runtime_error(output.errorString().toStdString());}
 QString summary=dir.filePath("summary-"+QDate::currentDate().toString("yyyyMMdd")+".md");QString existing;{QFile f(summary);if(f.open(QIODevice::ReadOnly))existing=QString::fromUtf8(f.readAll());}for(const auto &name:dir.entryList({"*.tns"},QDir::Files,QDir::Name)){QString heading="# Call Transcript - "+name+"\n";if(existing.contains(heading)){emit activity("Skipping "+name+", already summarized today");continue;}QFile input(dir.filePath(name));if(!input.open(QIODevice::ReadOnly))throw std::runtime_error(input.errorString().toStdString());QString transcript=QString::fromUtf8(input.readAll()),section=heading+"\n";emit activity("Summarizing: "+name);for(const auto &step:config_.steps){emit activity("  Running step: "+step.name);section+="## "+step.name+"\n\n"+summarize(renderPrompt(step.prompt,transcript))+"\n\n";}section+="---\n";QFile f(summary);if(!f.open(QIODevice::Append))throw std::runtime_error(f.errorString().toStdString());f.write(section.toUtf8());existing+=section;}emit finished(true,"Processing complete");}catch(const std::exception &e){emit finished(false,QString::fromUtf8(e.what()));}
}
void ProcessingPipeline::clean() { QDir dir(config_.outputDir);for(const auto &name:dir.entryList({"*.wav","*.tns"},QDir::Files)){if(QFile::remove(dir.filePath(name)))emit activity("Deleted: "+name);else emit activity("Could not delete: "+name);}emit activity("Cleaning complete."); }
