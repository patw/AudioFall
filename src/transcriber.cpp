#include "transcriber.h"
#include "wav.h"
#include <whisper.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <stdexcept>

namespace {
constexpr auto modelName = "ggml-large-v3-turbo-q5_0.bin";
// QFile handles Unicode paths on Windows, unlike narrow std::ifstream paths.
whisper_context *loadModel(const QString &path, bool gpu) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(file.errorString().toStdString());
    whisper_model_loader loader{};
    loader.context = &file;
    loader.read = [](void *p, void *out, size_t size) -> size_t {
        const qint64 n = static_cast<QFile *>(p)->read(static_cast<char *>(out), qint64(size));
        return n > 0 ? size_t(n) : 0;
    };
    loader.eof = [](void *p) { return static_cast<QFile *>(p)->atEnd(); };
    loader.close = [](void *p) { static_cast<QFile *>(p)->close(); };
    auto params = whisper_context_default_params();
    params.use_gpu = gpu;
    return whisper_init_with_params(&loader, params);
}
}

LocalTranscriber::LocalTranscriber() : context_(nullptr, whisper_free) {}
LocalTranscriber::~LocalTranscriber() = default;

QString LocalTranscriber::bundledModelPath(const QString &executableDirectory) {
    const QDir dir(executableDirectory);
#ifdef Q_OS_MACOS
    return QDir::cleanPath(dir.filePath(QString("../Resources/models/") + modelName));
#else
    const QString adjacent = dir.filePath(QString("models/") + modelName);
    if (QFileInfo::exists(adjacent)) return adjacent;
    // Debian /usr/bin and AppImage usr/bin, relative to their own installation.
    return QDir::cleanPath(dir.filePath(QString("../share/audiofall/models/") + modelName));
#endif
}

QString LocalTranscriber::transcribe(const QString &wavPath, const QString &language, ProgressFn progress) {
    const QByteArray lang = language.toUtf8();
    if (language != "auto" && whisper_lang_id(lang.constData()) < 0)
        throw std::runtime_error("Unsupported transcription language");
    QString error;
    QVector<float> samples;
    if (!Wav::readFloat16kMono(wavPath, samples, &error))
        throw std::runtime_error(error.toStdString());
    if (samples.isEmpty() || std::all_of(samples.cbegin(), samples.cend(),
                                       [](float sample) { return sample == 0.0f; })) return QStringLiteral("");
    if (!context_) {
        const QString model = bundledModelPath(QCoreApplication::applicationDirPath());
        if (!QFileInfo::exists(model))
            throw std::runtime_error(QString("Bundled Whisper model is missing: %1. Reinstall AudioFall.").arg(model).toStdString());
#ifdef Q_OS_MACOS
        context_.reset(loadModel(model, true));
        if (!context_) context_.reset(loadModel(model, false));
#else
        context_.reset(loadModel(model, false));
#endif
        if (!context_) throw std::runtime_error("Could not load bundled Whisper model (check available memory or reinstall AudioFall)");
    }
    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = std::clamp(QThread::idealThreadCount(), 1, 8);
    params.language = lang.constData();
    params.translate = false;
    params.no_context = true; // No transcript context leaks between recordings.
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.temperature = 0.0f;
    params.progress_callback_user_data = &progress;
    params.progress_callback = [](whisper_context *, whisper_state *, int value, void *p) {
        auto &callback = *static_cast<ProgressFn *>(p);
        if (callback) callback(value);
    };
    if (whisper_full(context_.get(), params, samples.constData(), int(samples.size())) != 0)
        throw std::runtime_error("Local Whisper transcription failed");
    QString text;
    for (int i = 0; i < whisper_full_n_segments(context_.get()); ++i)
        text += QString::fromUtf8(whisper_full_get_segment_text(context_.get(), i));
    return text.trimmed();
}
