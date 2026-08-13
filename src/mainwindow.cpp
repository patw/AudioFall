#include "mainwindow.h"
#include "config.h"
#include "pipeline.h"
#include "recorder.h"
#include <QAudioDevice>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow() : config_(loadConfig()), recorder_(new Recorder(this)) {
    setWindowTitle("AudioFall");
    resize(700, 460);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel("<h2>AudioFall</h2><p>Local meeting recorder, transcription, and summaries.</p>");
    layout->addWidget(title);
    activity_ = new QPlainTextEdit;
    activity_->setReadOnly(true);
    activity_->setMaximumBlockCount(1000);
    activity_->setMinimumHeight(220);
    layout->addWidget(new QLabel("Activity"));
    layout->addWidget(activity_);
    auto *row = new QHBoxLayout;
    status_ = new QLabel("● Idle");
    devices_ = new QComboBox;
    refreshDevices();
    row->addWidget(status_);
    row->addWidget(new QLabel("Microphone:"));
    row->addWidget(devices_, 1);
    layout->addLayout(row);
    auto *buttons = new QHBoxLayout;
    auto *settings = new QPushButton("Settings");
    record_ = new QPushButton("Record");
    process_ = new QPushButton("Transcribe & Summarize");
    clean_ = new QPushButton("Clean Intermediates");
    buttons->addWidget(settings); buttons->addWidget(record_); buttons->addWidget(process_); buttons->addWidget(clean_);
    layout->addLayout(buttons);
    setCentralWidget(central);
    connect(settings, &QPushButton::clicked, this, &MainWindow::showSettings);
    connect(record_, &QPushButton::clicked, this, &MainWindow::toggleRecording);
    connect(process_, &QPushButton::clicked, this, &MainWindow::process);
    connect(clean_, &QPushButton::clicked, this, &MainWindow::clean);
    connect(recorder_, &Recorder::errorOccurred, this, [this](const QString &e) { log("Recording error: " + e); });
}
void MainWindow::refreshDevices() {
    devices_->clear();
    for (const auto &d : QMediaDevices::audioInputs()) devices_->addItem(d.description(), QVariant::fromValue(d));
    if (devices_->count() == 0) devices_->addItem("No microphone available");
}
void MainWindow::log(const QString &text) { activity_->appendPlainText(text); }
void MainWindow::setBusy(bool busy) { process_->setEnabled(!busy); record_->setEnabled(!busy && !recorder_->isRecording()); clean_->setEnabled(!busy); status_->setText(busy ? "● Processing" : "● Idle"); }
void MainWindow::toggleRecording() {
    if (recorder_->isRecording()) {
        recorder_->stop(); record_->setText("Record"); devices_->setEnabled(true); status_->setText("● Idle"); log("Audio saved."); return;
    }
    if (devices_->count() == 0 || !devices_->currentData().isValid()) { QMessageBox::warning(this, "No microphone", "No usable microphone is available."); return; }
    bool ok = false;
    QString name = QInputDialog::getText(this, "New recording", "Filename:", QLineEdit::Normal, "meeting", &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (!name.endsWith(".wav", Qt::CaseInsensitive)) name += ".wav";
    QDir().mkpath(config_.outputDir);
    QString error;
    auto d = devices_->currentData().value<QAudioDevice>();
    if (!recorder_->start(d, QDir(config_.outputDir).filePath(name), &error)) { QMessageBox::critical(this, "Could not record", error); return; }
    record_->setText("Stop Recording"); devices_->setEnabled(false); status_->setText("● Recording"); log("Recording to " + name + "…");
}
void MainWindow::process() {
    setBusy(true); log("Starting transcription and summarization…");
    auto *thread = new QThread(this);
    auto *pipeline = new ProcessingPipeline(config_);
    pipeline->moveToThread(thread);
    connect(thread, &QThread::started, pipeline, &ProcessingPipeline::run);
    connect(pipeline, &ProcessingPipeline::activity, this, &MainWindow::log);
    connect(pipeline, &ProcessingPipeline::finished, this, [this, thread, pipeline](bool success, const QString &message) {
        log((success ? "" : "Processing failed: ") + message); setBusy(false); thread->quit(); pipeline->deleteLater(); thread->deleteLater();
    });
    thread->start();
}
void MainWindow::clean() { auto *pipeline = new ProcessingPipeline(config_, this); connect(pipeline, &ProcessingPipeline::activity, this, &MainWindow::log); pipeline->clean(); pipeline->deleteLater(); }
void MainWindow::showSettings() {
    QDialog dialog(this); dialog.setWindowTitle("AudioFall Settings"); dialog.resize(650, 500);
    auto *layout = new QVBoxLayout(&dialog); auto *tabs = new QTabWidget;
    auto make = [&tabs](const QString &label, QString &value) {
        auto *w = new QWidget; auto *f = new QFormLayout(w); auto *e = new QTextEdit(value);
        e->setMinimumHeight(55); f->addRow(label, e); tabs->addTab(w, label); return e;
    };
    auto *out = make("Output directory", config_.outputDir); auto *whisper = make("Whisper URL", config_.whisperUrl);
    auto *llm = make("LLM base URL", config_.llmUrl); auto *key = make("LLM API key", config_.llmApiKey);
    auto *model = make("LLM model", config_.llmModel); auto *system = make("System message", config_.systemMessage);
    QString stepsJson; { QJsonArray values; for (const auto &step : config_.steps) values.append(QJsonObject{{"name", step.name}, {"prompt", step.prompt}}); stepsJson = QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Indented)); }
    auto *steps = make("Summary steps (JSON)", stepsJson);
    layout->addWidget(tabs);
    auto *buttons = new QHBoxLayout; auto *ok = new QPushButton("Save"); auto *cancel = new QPushButton("Cancel");
    buttons->addStretch(); buttons->addWidget(ok); buttons->addWidget(cancel); layout->addLayout(buttons);
    connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept); connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    config_.outputDir = out->toPlainText().trimmed(); config_.whisperUrl = whisper->toPlainText().trimmed(); config_.llmUrl = llm->toPlainText().trimmed();
    config_.llmApiKey = key->toPlainText().trimmed(); config_.llmModel = model->toPlainText().trimmed(); config_.systemMessage = system->toPlainText().trimmed();
    QJsonParseError parseError; QJsonDocument stepsDocument = QJsonDocument::fromJson(steps->toPlainText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !stepsDocument.isArray()) { QMessageBox::warning(this, "Settings", "Summary steps must be a JSON array of {name, prompt} objects."); return; }
    QVector<SummaryStep> parsedSteps; for (const auto &value : stepsDocument.array()) { const auto object = value.toObject(); const QString name = object.value("name").toString().trimmed(); const QString prompt = object.value("prompt").toString(); if (name.isEmpty() || prompt.isEmpty()) { QMessageBox::warning(this, "Settings", "Every summary step requires a name and prompt."); return; } parsedSteps.append({name, prompt}); }
    if (parsedSteps.isEmpty()) { QMessageBox::warning(this, "Settings", "At least one summary step is required."); return; }
    config_.steps = parsedSteps;
    QString error; if (!saveConfig(config_, &error)) QMessageBox::warning(this, "Settings", error); else log("Settings saved.");
}
