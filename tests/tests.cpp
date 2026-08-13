#include "config.h"
#include "mainwindow.h"
#include "pipeline.h"
#include "wav.h"
#include <QtTest>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtEndian>
#include <limits>

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

    void longSilenceIsRemoved() {
        QTemporaryDir dir;
        QString path = dir.filePath("meeting.wav"), error;
        QVERIFY(Wav::writePcm16Mono(path, pcm({{1, 4000}, {.25, 0}, {1, 4000}, {2, 0}, {1, 4000}}), 16000, &error));
        QVERIFY2(Wav::removeSilence(path, -40, 1, .1, &error), qPrintable(error));
        QVERIFY(qAbs(frames(path) / 16000.0 - 3.45) < .08);
    }

    void shortPauseIsPreserved() {
        QTemporaryDir dir;
        QString path = dir.filePath("meeting.wav"), error;
        const QByteArray source = pcm({{1, 4000}, {.2, 0}, {1, 4000}});
        QVERIFY(Wav::writePcm16Mono(path, source, 16000, &error));
        QVERIFY(Wav::removeSilence(path, -40, 1, .15, &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().endsWith(source));
    }

    void allSilenceIsUnchanged() {
        QTemporaryDir dir;
        QString path = dir.filePath("silent.wav"), error;
        QVERIFY(Wav::writePcm16Mono(path, pcm({{2, 0}}), 16000, &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray before = file.readAll();
        QVERIFY(Wav::removeSilence(path, -40, 1, .15, &error));
        file.close();
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), before);
    }

    void invalidParametersFail() {
        QTemporaryDir dir;
        QString path = dir.filePath("meeting.wav"), error;
        QVERIFY(Wav::writePcm16Mono(path, pcm({{1, 4000}}), 16000, &error));
        QVERIFY(!Wav::removeSilence(path, std::numeric_limits<double>::quiet_NaN(), 1, .15, &error));
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
            [&trimCount](const QString &, QString *) { ++trimCount; return true; },
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

    void pipelineCleanRetainsMarkdown() {
        QTemporaryDir dir;
        for (const QString &name : {"call.wav", "call.tns", "summary.md"}) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("test");
        }
        ProcessingPipeline pipeline(configFor(dir.path()));
        pipeline.clean();
        QVERIFY(!QFile::exists(dir.filePath("call.wav")));
        QVERIFY(!QFile::exists(dir.filePath("call.tns")));
        QVERIFY(QFile::exists(dir.filePath("summary.md")));
    }

    void configRoundTrip() {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.json");
        setConfigPathForTesting(path);
        AppConfig config = configFor(dir.filePath("output"));
        config.llmModel = "test-model";
        config.steps = {{"Only", "Do: {chunk}"}};
        QString error;
        QVERIFY2(saveConfig(config, &error), qPrintable(error));
        const AppConfig loaded = loadConfig();
        QCOMPARE(loaded.outputDir, config.outputDir);
        QCOMPARE(loaded.llmModel, "test-model");
        QCOMPARE(loaded.steps.size(), 1);
        QCOMPARE(loaded.steps.first().name, "Only");
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
