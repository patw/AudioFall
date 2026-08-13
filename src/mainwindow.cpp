#include "mainwindow.h"
#include "config.h"
#include "pipeline.h"
#include "recorder.h"
#include <QAudioDevice>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
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
    buttons->addWidget(settings);
    buttons->addWidget(record_);
    buttons->addWidget(process_);
    buttons->addWidget(clean_);
    layout->addLayout(buttons);
    setCentralWidget(central);

    connect(settings, &QPushButton::clicked, this, &MainWindow::showSettings);
    connect(record_, &QPushButton::clicked, this, &MainWindow::toggleRecording);
    connect(process_, &QPushButton::clicked, this, &MainWindow::process);
    connect(clean_, &QPushButton::clicked, this, &MainWindow::clean);
    connect(recorder_, &Recorder::errorOccurred, this, [this](const QString &e) { log("Recording error: " + e); });
}

bool MainWindow::isSafeRecordingFileName(const QString &name, QString *error) {
    const QFileInfo info(name);
    if (name.trimmed().isEmpty()) {
        if (error) *error = "A recording name is required.";
        return false;
    }
    if (info.fileName() != name || name.contains('/') || name.contains('\\') || name == "." || name == "..") {
        if (error) *error = "Use a filename only, without folders or path separators.";
        return false;
    }
    if (name.contains(QChar::Null)) {
        if (error) *error = "The filename contains an invalid character.";
        return false;
    }
    return true;
}

void MainWindow::refreshDevices() {
    devices_->clear();
    for (const auto &d : QMediaDevices::audioInputs())
        devices_->addItem(d.description(), QVariant::fromValue(d));
    if (devices_->count() == 0)
        devices_->addItem("No microphone available");
}

void MainWindow::log(const QString &text) { activity_->appendPlainText(text); }

void MainWindow::setBusy(bool busy) {
    process_->setEnabled(!busy);
    record_->setEnabled(!busy && !recorder_->isRecording());
    clean_->setEnabled(!busy);
    status_->setText(busy ? "● Processing" : "● Idle");
}

void MainWindow::toggleRecording() {
    if (recorder_->isRecording()) {
        QString error;
        if (recorder_->stop(&error))
            log("Audio saved.");
        else
            log("Recording discarded: " + error);
        record_->setText("Record");
        devices_->setEnabled(true);
        status_->setText("● Idle");
        return;
    }

    if (devices_->count() == 0 || !devices_->currentData().isValid()) {
        QMessageBox::warning(this, "No microphone", "No usable microphone is available.");
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "New recording", "Filename:", QLineEdit::Normal, "meeting", &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (!name.endsWith(".wav", Qt::CaseInsensitive)) name += ".wav";

    QString validationError;
    if (!isSafeRecordingFileName(name, &validationError)) {
        QMessageBox::warning(this, "Invalid recording name", validationError);
        return;
    }

    QDir output(config_.outputDir);
    if (!output.exists() && !QDir().mkpath(config_.outputDir)) {
        QMessageBox::critical(this, "Could not record", "Could not create the output directory.");
        return;
    }
    const QString path = output.filePath(name);
    if (QFile::exists(path) && QMessageBox::question(
            this, "Replace recording?", name + " already exists. Replace it?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QString error;
    const auto device = devices_->currentData().value<QAudioDevice>();
    if (!recorder_->start(device, path, &error)) {
        QMessageBox::critical(this, "Could not record", error);
        return;
    }
    record_->setText("Stop Recording");
    devices_->setEnabled(false);
    status_->setText("● Recording");
    log("Recording to " + name + "…");
}

void MainWindow::process() {
    setBusy(true);
    log("Starting transcription and summarization…");

    processingThread_ = new QThread(this);
    auto *pipeline = new ProcessingPipeline(config_);
    pipeline->moveToThread(processingThread_);
    connect(processingThread_, &QThread::started, pipeline, &ProcessingPipeline::run);
    connect(pipeline, &ProcessingPipeline::activity, this, &MainWindow::log);
    connect(pipeline, &ProcessingPipeline::finished, this, [this, pipeline](bool success, const QString &message) {
        log((success ? "" : "Processing failed: ") + message);
        setBusy(false);
        processingThread_->quit();
        pipeline->deleteLater();
        processingThread_->deleteLater();
        processingThread_ = nullptr;
    });
    processingThread_->start();
}

void MainWindow::clean() {
    auto *pipeline = new ProcessingPipeline(config_, this);
    connect(pipeline, &ProcessingPipeline::activity, this, &MainWindow::log);
    pipeline->clean();
    pipeline->deleteLater();
}

void MainWindow::showSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("AudioFall Settings");
    dialog.resize(650, 500);
    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget;
    auto make = [&tabs](const QString &label, QString &value) {
        auto *w = new QWidget;
        auto *f = new QFormLayout(w);
        auto *e = new QTextEdit(value);
        e->setMinimumHeight(55);
        f->addRow(label, e);
        tabs->addTab(w, label);
        return e;
    };

    auto *out = make("Output directory", config_.outputDir);
    auto *whisper = make("Whisper URL", config_.whisperUrl);
    auto *llm = make("LLM base URL", config_.llmUrl);
    auto *key = make("LLM API key", config_.llmApiKey);
    auto *model = make("LLM model", config_.llmModel);
    auto *system = make("System message", config_.systemMessage);
    QJsonArray values;
    for (const auto &step : config_.steps)
        values.append(QJsonObject{{"name", step.name}, {"prompt", step.prompt}});
    QString stepsJson = QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Indented));
    auto *steps = make("Summary steps (JSON)", stepsJson);
    layout->addWidget(tabs);

    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton("Save");
    auto *cancel = new QPushButton("Cancel");
    buttons->addStretch();
    buttons->addWidget(save);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    connect(save, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    config_.outputDir = out->toPlainText().trimmed();
    config_.whisperUrl = whisper->toPlainText().trimmed();
    config_.llmUrl = llm->toPlainText().trimmed();
    config_.llmApiKey = key->toPlainText().trimmed();
    config_.llmModel = model->toPlainText().trimmed();
    config_.systemMessage = system->toPlainText().trimmed();

    QJsonParseError parseError;
    QJsonDocument stepsDocument = QJsonDocument::fromJson(steps->toPlainText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !stepsDocument.isArray()) {
        QMessageBox::warning(this, "Settings", "Summary steps must be a JSON array of {name, prompt} objects.");
        return;
    }
    QVector<SummaryStep> parsedSteps;
    for (const auto &value : stepsDocument.array()) {
        const auto object = value.toObject();
        const QString name = object.value("name").toString().trimmed();
        const QString prompt = object.value("prompt").toString();
        if (name.isEmpty() || prompt.isEmpty()) {
            QMessageBox::warning(this, "Settings", "Every summary step requires a name and prompt.");
            return;
        }
        parsedSteps.append({name, prompt});
    }
    if (parsedSteps.isEmpty()) {
        QMessageBox::warning(this, "Settings", "At least one summary step is required.");
        return;
    }
    config_.steps = parsedSteps;

    QString error;
    if (!saveConfig(config_, &error))
        QMessageBox::warning(this, "Settings", error);
    else
        log("Settings saved.");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (recorder_->isRecording()) {
        QString error;
        if (!recorder_->stop(&error))
            log("Recording discarded while closing: " + error);
    }
    if (processingThread_ && processingThread_->isRunning()) {
        QMessageBox::warning(this, "Processing in progress", "Wait for transcription and summarization to finish before closing AudioFall.");
        event->ignore();
        return;
    }
    event->accept();
}
