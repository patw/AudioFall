#pragma once
#include <QString>
#include <functional>
#include <memory>

struct whisper_context;

// One context per processing worker, reused for all recordings in that batch.
class LocalTranscriber {
public:
    using ProgressFn = std::function<void(int)>;
    LocalTranscriber();
    ~LocalTranscriber();
    QString transcribe(const QString &wavPath, const QString &language, ProgressFn progress = {});
    static QString bundledModelPath(const QString &executableDirectory);
private:
    std::unique_ptr<whisper_context, void (*)(whisper_context *)> context_;
};
