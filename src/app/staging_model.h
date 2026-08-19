#pragma once

#include <QObject>
#include <QString>

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "../core/texture_manifest_entry.h"

namespace bitdeck {

enum class AppState {
    None,
    ChangesStaged,
};

struct CustomStageEntry {
    std::vector<std::filesystem::path> files;
};

struct CustomSequencesEntry {
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> seqMetaPairs; // (seq, meta)
};

struct CustomTexturesEntry {
    std::vector<std::pair<std::filesystem::path, TextureManifestEntry>> files;
};

using StageEntry = std::variant<CustomStageEntry, CustomSequencesEntry, CustomTexturesEntry>;

size_t stageEntryFileCount(const StageEntry& entry);

// The shared staging "basket": every producer screen (Create Custom, Create
// Custom Sequences, Create Replace Textures, the font-generator debug tool)
// merges its output in here, keyed by archive-relative directory path. The
// bottom bar and the finish/generate panel both observe this model.
class StagingModel : public QObject {
    Q_OBJECT

public:
    explicit StagingModel(QObject* parent = nullptr);

    const std::map<std::string, StageEntry>& entries() const { return entries_; }
    AppState state() const { return state_; }

    void addCustomStageEntries(const std::vector<std::filesystem::path>& files, const std::string& path);
    void addCustomSequenceEntry(const std::vector<std::pair<std::filesystem::path, std::filesystem::path>>& pairs,
                                 const std::string& path);
    void addCustomTextureEntry(
        const std::map<std::string, std::vector<std::pair<std::filesystem::path, TextureManifestEntry>>>&
            processedFiles);
    void addFile(const std::filesystem::path& file, const std::string& path);
    void removeFile(const std::filesystem::path& file, const std::string& key);
    void clear();

    // Shared toggles, read/written by every staging screen.
    bool prependAlt = false;
    bool compressFiles = false;
    bool keepFolderOpenAfterStaging = false;

    // A setter (rather than a plain field) so every mode's status-bar
    // extension picker and the Finalize dialog's picker stay in sync via
    // changed() when any one of them is edited.
    QString outputExtension() const { return outputExtension_; }
    void setOutputExtension(const QString& extension);

    bool isGenerating() const { return isGenerating_; }
    int totalFiles() const { return totalFiles_; }
    int filesProcessed() const { return filesProcessed_; }

    void beginGeneration(int totalFiles);
    void reportGenerationProgress(int processed);
    void finishGeneration();
    void failGeneration(const QString& error);

signals:
    void changed();
    void generationProgress(int processed, int total);
    void generationFinished();
    void generationFailed(QString error);

private:
    void updateState();

    std::map<std::string, StageEntry> entries_;
    AppState state_ = AppState::None;
    bool isGenerating_ = false;
    int totalFiles_ = 0;
    int filesProcessed_ = 0;
    QString outputExtension_ = QStringLiteral("o2r");
};

} // namespace bitdeck
