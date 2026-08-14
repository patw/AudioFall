#include "mainwindow.h"
#include "config.h"
#include "pipeline.h"
#include "recorder.h"
#include <QAudioDevice>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFont>
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
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>

MainWindow::MainWindow() : config_(loadConfig()), recorder_(new Recorder(this)) {
    // Match AudioSumma's compact, native Qt layout rather than adding a custom header.
    setWindowTitle("Meeting Recorder");
    resize(500, 400);
    setMinimumWidth(500);

    auto *central = new QWidget(this);
    // PyQt5's macOS widget window uses a light system-gray content surface;
    // Qt6 otherwise renders this central widget as pure white.
    QPalette centralPalette = central->palette();
    centralPalette.setColor(QPalette::Window, QColor("#ECECEC"));
    central->setPalette(centralPalette);
    central->setAutoFillBackground(true);
    auto *layout = new QVBoxLayout(central);

    layout->addWidget(new QLabel("Activity"));
    activity_ = new QPlainTextEdit;
    activity_->setReadOnly(true);
    activity_->setMaximumBlockCount(1000);
    activity_->setMinimumHeight(180);
    layout->addWidget(activity_);

    auto *row = new QHBoxLayout;
    status_ = new QLabel("🟢");
    status_->setFont(QFont("Apple Color Emoji", 63));
    status_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    status_->setMinimumWidth(108);
    status_->setToolTip("Not recording");
    row->addWidget(status_);
    devices_ = new QComboBox;
    // Defer the first device scan until the event loop is running so that Qt
    // Multimedia's FFmpeg/PipeWire backend has time to initialise its async
    // connection, avoiding a segfault on the very first launch.
    QTimer::singleShot(0, this, &MainWindow::refreshDevices);
    row->addWidget(devices_);
    layout->addLayout(row);

    auto *buttons = new QHBoxLayout;
    auto *settings = new QPushButton("Settings");
    record_ = new QPushButton("Record");
    process_ = new QPushButton("Transcribe");
    clean_ = new QPushButton("Clean");
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
    status_->setText(busy ? "🟡" : "🟢");
    status_->setToolTip(busy ? "Processing" : "Not recording");
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
        status_->setText("🟢");
        status_->setToolTip("Not recording");
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
    status_->setText("🔴");
    status_->setToolTip("Recording");
    log("Recording to " + name + "…");
}

void MainWindow::process() {
    setBusy(true);

    processingThread_ = new QThread(this);
    auto *pipeline = new ProcessingPipeline(config_);
    pipeline->moveToThread(processingThread_);
    connect(processingThread_, &QThread::started, pipeline, &ProcessingPipeline::run);
    connect(pipeline, &ProcessingPipeline::activity, this, &MainWindow::log);
    connect(pipeline, &ProcessingPipeline::finished, this, [this, pipeline](bool success, const QString &message) {
        if (!success) log("Processing failed: " + message);
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
    dialog.setWindowTitle("Settings");
    dialog.resize(600, 400);
    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget;
    auto makeEditor = [](const QString &value, int height = 60) {
        auto *editor = new QTextEdit(value);
        editor->setMinimumSize(500, height);
        editor->setLineWrapMode(QTextEdit::WidgetWidth);
        return editor;
    };

    auto *llmTab = new QWidget;
    auto *llmForm = new QFormLayout(llmTab);
    llmForm->setSpacing(8);
    auto *llm = makeEditor(config_.llmUrl);
    auto *key = makeEditor(config_.llmApiKey);
    auto *model = makeEditor(config_.llmModel);
    llmForm->addRow("Base URL:", llm);
    llmForm->addRow("API Key:", key);
    llmForm->addRow("Model:", model);
    tabs->addTab(llmTab, "LLM");

    auto *whisperTab = new QWidget;
    auto *whisperForm = new QFormLayout(whisperTab);
    whisperForm->setSpacing(8);
    auto *whisper = makeEditor(config_.whisperUrl);
    whisperForm->addRow("URL:", whisper);
    tabs->addTab(whisperTab, "Whisper");

    auto *promptsTab = new QWidget;
    auto *promptsForm = new QFormLayout(promptsTab);
    promptsForm->setSpacing(8);
    auto *system = makeEditor(config_.systemMessage, 80);
    promptsForm->addRow("System Message:", system);
    QVector<QLineEdit *> stepNames;
    QVector<QTextEdit *> stepPrompts;
    for (const auto &step : config_.steps) {
        auto *name = new QLineEdit(step.name);
        auto *prompt = makeEditor(step.prompt, 80);
        stepNames.append(name);
        stepPrompts.append(prompt);
        promptsForm->addRow("Step Name:", name);
        promptsForm->addRow("Prompt:", prompt);
    }
    tabs->addTab(promptsTab, "Prompts");

    auto *generalTab = new QWidget;
    auto *generalForm = new QFormLayout(generalTab);
    generalForm->setSpacing(8);
    auto *out = makeEditor(config_.outputDir);
    auto *silenceThreshold = new QLineEdit(QString::number(config_.silenceThresholdDb));
    auto *silenceMin = new QLineEdit(QString::number(config_.silenceMinSeconds));
    generalForm->addRow("Output Directory:", out);
    generalForm->addRow("Silence threshold (dB):", silenceThreshold);
    generalForm->addRow("Minimum silence (seconds):", silenceMin);
    tabs->addTab(generalTab, "General Settings");
    layout->addWidget(tabs);

    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton("Save");
    auto *cancel = new QPushButton("Cancel");
    buttons->addWidget(save);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    connect(save, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    config_.outputDir = out->toPlainText();
    config_.whisperUrl = whisper->toPlainText();
    config_.llmUrl = llm->toPlainText();
    config_.llmApiKey = key->toPlainText();
    config_.llmModel = model->toPlainText();
    config_.systemMessage = system->toPlainText();

    bool thresholdOk = false, minSilenceOk = false;
    const double thresholdValue = silenceThreshold->text().trimmed().toDouble(&thresholdOk);
    const double minSilenceValue = silenceMin->text().trimmed().toDouble(&minSilenceOk);
    if (!thresholdOk || !minSilenceOk || !std::isfinite(thresholdValue) ||
        !std::isfinite(minSilenceValue) || minSilenceValue < 0) {
        QMessageBox::warning(this, "Settings",
                             "Silence threshold must be a number (dB) and minimum silence a non-negative number of seconds.");
        return;
    }
    config_.silenceThresholdDb = thresholdValue;
    config_.silenceMinSeconds = minSilenceValue;

    QVector<SummaryStep> parsedSteps;
    for (qsizetype i = 0; i < stepNames.size(); ++i) {
        const QString name = stepNames.at(i)->text();
        const QString prompt = stepPrompts.at(i)->toPlainText();
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
