#include "config.h"
#include "mainwindow.h"
#include "pipeline.h"
#include "wav.h"
#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QFile>
#include <QSignalSpy>
#include <QThread>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <limits>
#include <stdexcept>

class AudioFallTests : public QObject {
    Q_OBJECT

    QByteArray pcm(const QList<QPair<double, qint16>> &segments) {
        QByteArray out;
        for (const auto &segment : segments) {
            for (int i = 0; i < qRound(segment.first * 16000); ++i) {
                char bytes[2];
                qToLittleEndian(segment.second, reinterpret_cast<uchar *>(bytes));
                out.append(bytes, 2);
            }
        }
        return out;
    }

    int frames(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return -1;
        const QByteArray bytes = file.readAll();
        if (bytes.size() < 44) return -1;
        return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + 40)) / 2;
    }

    AppConfig configFor(const QString &directory) {
        AppConfig config = AppConfig::defaults();
        config.outputDir = directory;
        config.steps = {{"Summary", "Summary: {chunk}"}, {"Facts", "Facts: {chunk}"}};
        return config;
    }

private slots:
    void init() { setConfigPathForTesting({}); }
    void cleanup() { setConfigPathForTesting({}); }

    void windowFollowsSystemPalette_data() {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light-to-dark") << false;
        QTest::newRow("dark-to-light") << true;
    }

    void windowFollowsSystemPalette() {
        QFETCH(bool, dark);
        const QPalette original = QApplication::palette();
        const auto restorePalette = qScopeGuard([original] {
            QApplication::setPalette(original);
        });
        QTemporaryDir dir;
        setConfigPathForTesting(dir.filePath("config.json"));

        auto systemPalette = [](bool darkMode) {
            QPalette palette;
            for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
                palette.setColor(group, QPalette::Window, darkMode ? QColor("#292929") : QColor("#ececec"));
                palette.setColor(group, QPalette::WindowText, darkMode ? QColor("#eeeeee") : QColor("#202020"));
                palette.setColor(group, QPalette::Base, darkMode ? QColor("#181818") : QColor("#ffffff"));
                palette.setColor(group, QPalette::Text, darkMode ? QColor("#eeeeee") : QColor("#202020"));
                palette.setColor(group, QPalette::Button, darkMode ? QColor("#383838") : QColor("#e0e0e0"));
                palette.setColor(group, QPalette::ButtonText, darkMode ? QColor("#eeeeee") : QColor("#202020"));
            }
            return palette;
        };
        QApplication::setPalette(systemPalette(dark));
        MainWindow window;
        auto verifyPalette = [&window](const QPalette &expected) {
            QVERIFY(!window.centralWidget()->testAttribute(Qt::WA_SetPalette));
            for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
                QCOMPARE(window.centralWidget()->palette().color(group, QPalette::Window), expected.color(group, QPalette::Window));
                for (auto *label : window.findChildren<QLabel *>())
                    QCOMPARE(label->palette().color(group, QPalette::WindowText), expected.color(group, QPalette::WindowText));
                for (auto *button : window.findChildren<QPushButton *>())
                    QCOMPARE(button->palette().color(group, QPalette::ButtonText), expected.color(group, QPalette::ButtonText));
                for (auto *combo : window.findChildren<QComboBox *>())
                    QCOMPARE(combo->palette().color(group, QPalette::ButtonText), expected.color(group, QPalette::ButtonText));
                for (auto *editor : window.findChildren<QPlainTextEdit *>()) {
                    QCOMPARE(editor->palette().color(group, QPalette::Base), expected.color(group, QPalette::Base));
                    QCOMPARE(editor->palette().color(group, QPalette::Text), expected.color(group, QPalette::Text));
                }
            }
        };
        verifyPalette(systemPalette(dark));
        QApplication::setPalette(systemPalette(!dark));
        QApplication::processEvents(); // ApplicationPaletteChange is delivered via the event loop.
        verifyPalette(systemPalette(!dark));
    }

    void longSilenceIsRemoved() {
        QTemporaryDir dir;
        QString path = dir.filePath("meeting.wav");
        QString trimmedPath = dir.filePath("meeting.trimmed.wav");
        QString error;
        QVERIFY(Wav::writePcm16Mono(path, pcm({{1, 4000}, {.25, 0}, {1, 4000}, {6, 0}, {1, 4000}}), 16000, &error));
        QVERIFY2(Wav::removeSilence(path, trimmedPath, -40, 5, .1, &error), qPrintable(error));
        QVERIFY(qAbs(frames(trimmedPath) / 16000.0 - 3.45) < .08);
    }

    void pauseUnderFiveSecondsIsPreserved() {
        QTemporaryDir dir;
        QString path = dir.filePath("meeting.wav");
        QString trimmedPath = dir.filePath("meeting.trimmed.wav");
        QString error;
        const QByteArray source = pcm({{1, 4000}, {4, 0}, {1, 4000}});
        QVERIFY(Wav::writePcm16Mono(path, source, 16000, &error));
        QVERIFY(Wav::removeSilence(path, trimmedPath, -40, 5, .15, &error));
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const QByteArray expected = original.readAll();
        original.close();
        QFile trimmed(trimmedPath);
        QVERIFY(trimmed.open(QIODevice::ReadOnly));
        QCOMPARE(trimmed.readAll(), expected);
    }

    void allSilenceIsCopied() {
        QTemporaryDir dir;
        QString path = dir.filePath("silent.wav");
        QString trimmedPath = dir.filePath("silent.trimmed.wav");
        QString error;
        QVERIFY(Wav::writePcm16Mono(path, pcm({{2, 0}}), 16000, &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray before = file.readAll();
        file.close();
        QVERIFY(Wav::removeSilence(path, trimmedPath, -40, 5, .15, &error));
        QFile trimmed(trimmedPath);
        QVERIFY(trimmed.open(QIODevice::ReadOnly));
        QCOMPARE(trimmed.readAll(), before);
    }

    void trimmingPreservesOriginal() {
        QTemporaryDir dir;
        QString path = dir.filePath("meeting.wav");
        QString trimmedPath = dir.filePath("meeting.trimmed.wav");
        QString error;
        const QByteArray source = pcm({{1, 4000}, {6, 0}, {1, 4000}});
        QVERIFY(Wav::writePcm16Mono(path, source, 16000, &error));
        QVERIFY(Wav::removeSilence(path, trimmedPath, -40, 5, .15, &error));
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const QByteArray after = original.readAll();
        QVERIFY(after.endsWith(source));
        QCOMPARE(after.size(), 44 + source.size());
        QVERIFY(QFile::exists(trimmedPath));
        QVERIFY(frames(trimmedPath) < frames(path));
    }

    void invalidParametersFail() {
        QTemporaryDir dir;
        QString path = dir.filePath("meeting.wav");
        QString trimmedPath = dir.filePath("meeting.trimmed.wav");
        QString error;
        QVERIFY(Wav::writePcm16Mono(path, pcm({{1, 4000}}), 16000, &error));
        QVERIFY(!Wav::removeSilence(path, trimmedPath, std::numeric_limits<double>::quiet_NaN(), 5, .15, &error));
        QVERIFY(!error.isEmpty());
    }

    void pipelineSkipsTranscriptAndWritesNewOne() {
        QTemporaryDir dir;
        QFile pending(dir.filePath("pending.wav"));
        QVERIFY(pending.open(QIODevice::WriteOnly));
        pending.close();
        QFile done(dir.filePath("done.wav"));
        QVERIFY(done.open(QIODevice::WriteOnly));
        done.close();
        QFile transcript(dir.filePath("done.tns"));
        QVERIFY(transcript.open(QIODevice::WriteOnly));
        transcript.write("existing");
        transcript.close();
        int trimCount = 0, transcribeCount = 0;
        ProcessingPipeline pipeline(configFor(dir.path()), nullptr,
            [&trimCount](const QString &, const QString &, QString *) { ++trimCount; return true; },
            [&transcribeCount](const QString &) { ++transcribeCount; return QString("new transcript"); },
            [](const QString &) { return QString("summary"); });
        QSignalSpy finished(&pipeline, &ProcessingPipeline::finished);
        pipeline.run();
        QCOMPARE(trimCount, 1);
        QCOMPARE(transcribeCount, 1);
        QCOMPARE(QFile(dir.filePath("pending.tns")).exists(), true);
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.at(0).at(0).toBool());
    }

    void pipelineSummarizesOncePerDay() {
        QTemporaryDir dir;
        QFile transcript(dir.filePath("call.tns"));
        QVERIFY(transcript.open(QIODevice::WriteOnly));
        transcript.write("hello");
        transcript.close();
        int calls = 0;
        ProcessingPipeline pipeline(configFor(dir.path()), nullptr, {},
            [](const QString &) { return QString(); },
            [&calls](const QString &prompt) { ++calls; return "result:" + prompt; });
        pipeline.run();
        pipeline.run();
        QCOMPARE(calls, 2);
        const QString summaryPath = dir.filePath("summary-" + QDate::currentDate().toString("yyyyMMdd") + ".md");
        QFile summary(summaryPath);
        QVERIFY(summary.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(summary.readAll());
        QCOMPARE(content.count("# Call Transcript - call.tns"), 1);
        QVERIFY(content.contains("result:Summary: hello"));
    }

    void pipelineEmitsLifecycleActivityOnce() {
        QTemporaryDir dir;
        ProcessingPipeline pipeline(configFor(dir.path()), nullptr, {},
            [](const QString &) { return QString(); },
            [](const QString &) { return QString(); });
        QSignalSpy activity(&pipeline, &ProcessingPipeline::activity);
        QSignalSpy finished(&pipeline, &ProcessingPipeline::finished);

        pipeline.run();

        QStringList messages;
        for (const auto &entry : activity)
            messages.append(entry.at(0).toString());
        QCOMPARE(messages.count("Starting transcription and summarization…"), 1);
        QCOMPARE(messages.count("Processing complete"), 1);
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.at(0).at(0).toBool());
        QCOMPARE(finished.at(0).at(1).toString(), QString());
    }

    void processingThreadIsStoppedBeforeDeletion() {
        auto *thread = new QThread;
        QSignalSpy deleted(thread, &QObject::destroyed);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start();
        QVERIFY(thread->isRunning());
        thread->quit();
        QVERIFY(thread->wait(5000));
        QTRY_COMPARE(deleted.count(), 1);
    }

    void pipelineIgnoresTrimmedFiles() {
        QTemporaryDir dir;
        QFile original(dir.filePath("meeting.wav"));
        QVERIFY(original.open(QIODevice::WriteOnly));
        original.close();
        QFile trimmed(dir.filePath("meeting.trimmed.wav"));
        QVERIFY(trimmed.open(QIODevice::WriteOnly));
        trimmed.close();
        int trimCount = 0, transcribeCount = 0;
        ProcessingPipeline pipeline(configFor(dir.path()), nullptr,
            [&trimCount](const QString &, const QString &, QString *) { ++trimCount; return true; },
            [&transcribeCount](const QString &) { ++transcribeCount; return QString("t"); },
            [](const QString &) { return QString("s"); });
        QSignalSpy finished(&pipeline, &ProcessingPipeline::finished);
        pipeline.run();
        QCOMPARE(trimCount, 1);
        QCOMPARE(transcribeCount, 1);
        QVERIFY(!QFile::exists(dir.filePath("meeting.trimmed.trimmed.wav")));
        QCOMPARE(finished.count(), 1);
    }

    void pipelineUsesConfiguredMinSilence() {
        QTemporaryDir dir;
        QString error;
        const QString path = dir.filePath("meeting.wav");
        QVERIFY(Wav::writePcm16Mono(path, pcm({{1, 4000}, {6, 0}, {1, 4000}}), 16000, &error));
        AppConfig config = configFor(dir.path());
        config.silenceMinSeconds = 10.0; // longer than the 6s pause, so nothing is trimmed
        ProcessingPipeline pipeline(config, nullptr, {},
            [](const QString &) { return QString("transcript"); },
            [](const QString &) { return QString("summary"); });
        QSignalSpy finished(&pipeline, &ProcessingPipeline::finished);
        pipeline.run();
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.at(0).at(0).toBool());
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const QByteArray originalBytes = original.readAll();
        original.close();
        QFile trimmed(dir.filePath("meeting.trimmed.wav"));
        QVERIFY(trimmed.open(QIODevice::ReadOnly));
        QCOMPARE(trimmed.readAll(), originalBytes);
    }

    void pipelineUsesConfiguredThreshold() {
        QTemporaryDir dir;
        QString error;
        const QString path = dir.filePath("meeting.wav");
        QVERIFY(Wav::writePcm16Mono(path, pcm({{1, 4000}, {6, 0}, {1, 4000}}), 16000, &error));
        AppConfig config = configFor(dir.path());
        config.silenceThresholdDb = 0.0; // everything below full scale counts as silence
        ProcessingPipeline pipeline(config, nullptr, {},
            [](const QString &) { return QString("transcript"); },
            [](const QString &) { return QString("summary"); });
        QSignalSpy finished(&pipeline, &ProcessingPipeline::finished);
        pipeline.run();
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.at(0).at(0).toBool());
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const QByteArray originalBytes = original.readAll();
        original.close();
        QFile trimmed(dir.filePath("meeting.trimmed.wav"));
        QVERIFY(trimmed.open(QIODevice::ReadOnly));
        QCOMPARE(trimmed.readAll(), originalBytes);
    }

    void pipelineCleanRetainsMarkdown() {
        QTemporaryDir dir;
        for (const QString &name : {"call.wav", "call.trimmed.wav", "call.tns", "summary.md"}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("test");
        }
        ProcessingPipeline pipeline(configFor(dir.path()));
        pipeline.clean();
        QVERIFY(!QFile::exists(dir.filePath("call.wav")));
        QVERIFY(!QFile::exists(dir.filePath("call.trimmed.wav")));
        QVERIFY(!QFile::exists(dir.filePath("call.tns")));
        QVERIFY(QFile::exists(dir.filePath("summary.md")));
    }

    void configRoundTrip() {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.json");
        setConfigPathForTesting(path);
        AppConfig config = configFor(dir.filePath("output"));
        config.llmModel = "test-model";
        config.silenceThresholdDb = -35.5;
        config.silenceMinSeconds = 7.5;
        config.steps = {{"Only", "Do: {chunk}"}};
        QString error;
        QVERIFY2(saveConfig(config, &error), qPrintable(error));
        const AppConfig loaded = loadConfig();
        QCOMPARE(loaded.outputDir, config.outputDir);
        QCOMPARE(loaded.llmModel, "test-model");
        QCOMPARE(loaded.silenceThresholdDb, config.silenceThresholdDb);
        QCOMPARE(loaded.silenceMinSeconds, config.silenceMinSeconds);
        QCOMPARE(loaded.steps.size(), 1);
        QCOMPARE(loaded.steps.first().name, "Only");
    }

    void floatPcmConversionAndFormatValidation() {
        QTemporaryDir dir;
        QString error;
        const QString path = dir.filePath("samples.wav");
        QByteArray bytes;
        for (qint16 sample : {qint16(-32768), qint16(0), qint16(32767)}) {
            char b[2];
            qToLittleEndian(sample, reinterpret_cast<uchar *>(b));
            bytes.append(b, 2);
        }
        QVERIFY(Wav::writePcm16Mono(path, bytes, 16000, &error));
        QVector<float> floats;
        QVERIFY(Wav::readFloat16kMono(path, floats, &error));
        QCOMPARE(floats.size(), 3);
        QCOMPARE(floats[0], -1.0f);
        QCOMPARE(floats[1], 0.0f);
        QCOMPARE(floats[2], 32767.0f / 32768.0f);
        QVERIFY(Wav::writePcm16Mono(path, bytes, 48000, &error));
        QVERIFY(!Wav::readFloat16kMono(path, floats, &error));
        QVERIFY(!error.isEmpty());
    }

    void localSilenceDoesNotHallucinateOrLoadModel() {
        QTemporaryDir dir;
        const QString path = dir.filePath("silence.wav");
        QVERIFY(Wav::writePcm16Mono(path, pcm({{1, 0}}), 16000));
        LocalTranscriber transcriber;
        QCOMPARE(transcriber.transcribe(path, "auto"), QStringLiteral(""));
    }

    void legacyWhisperEndpointIsIgnored() {
        QTemporaryDir dir;
        setConfigPathForTesting(dir.filePath("settings.json"));
        QFile file(dir.filePath("settings.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{"whisper_url", "http://obsolete:8088/inference"},
                                            {"llm_model", "retained"}}).toJson());
        file.close();
        auto config = loadConfig();
        QCOMPARE(config.transcriptionLanguage, "auto");
        QCOMPARE(config.llmModel, "retained");
        config.transcriptionLanguage = "de";
        QVERIFY(saveConfig(config));
        QCOMPARE(loadConfig().transcriptionLanguage, "de");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        QVERIFY(!object.contains("whisper_url"));
    }

    void bundledModelResolutionDoesNotUseWorkingDirectory() {
        QTemporaryDir dir;
#ifdef Q_OS_MACOS
        const QString executableDir = dir.filePath("AudioFall.app/Contents/MacOS");
        QCOMPARE(LocalTranscriber::bundledModelPath(executableDir),
                 dir.filePath("AudioFall.app/Contents/Resources/models/ggml-large-v3-turbo-q5_0.bin"));
#else
        const QString executableDir = dir.filePath("usr/bin");
        QCOMPARE(LocalTranscriber::bundledModelPath(executableDir),
                 dir.filePath("usr/share/audiofall/models/ggml-large-v3-turbo-q5_0.bin"));
        QVERIFY(QDir().mkpath(executableDir + "/models"));
        QFile model(executableDir + "/models/ggml-large-v3-turbo-q5_0.bin");
        QVERIFY(model.open(QIODevice::WriteOnly));
        model.close();
        QCOMPARE(LocalTranscriber::bundledModelPath(executableDir), model.fileName());
#endif
    }

    void localTranscriberRejectsInvalidAudioWithoutNetwork() {
        QTemporaryDir dir;
        LocalTranscriber transcriber;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, transcriber.transcribe(dir.filePath("missing.wav"), "auto"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, transcriber.transcribe(dir.filePath("missing.wav"), "invalid-language"));
    }

    void safeRecordingFileNames() {
        QString error;
        QVERIFY(MainWindow::isSafeRecordingFileName("meeting.wav", &error));
        QVERIFY(!MainWindow::isSafeRecordingFileName("../meeting.wav", &error));
        QVERIFY(!MainWindow::isSafeRecordingFileName("folder/meeting.wav", &error));
        QVERIFY(!MainWindow::isSafeRecordingFileName("folder\\meeting.wav", &error));
        QVERIFY(!MainWindow::isSafeRecordingFileName("", &error));
    }
};

QTEST_MAIN(AudioFallTests)
#include "tests.moc"
