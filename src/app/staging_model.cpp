#include "staging_model.h"

#include <algorithm>

namespace bitdeck {

size_t stageEntryFileCount(const StageEntry& entry) {
    if (const auto* e = std::get_if<CustomStageEntry>(&entry)) {
        return e->files.size();
    }
    if (const auto* e = std::get_if<CustomSequencesEntry>(&entry)) {
        return e->seqMetaPairs.size();
    }
    return std::get<CustomTexturesEntry>(entry).files.size();
}

StagingModel::StagingModel(QObject* parent) : QObject(parent) {}

void StagingModel::addCustomStageEntries(const std::vector<std::filesystem::path>& files, const std::string& path) {
    auto& slot = entries_[path];
    if (!std::holds_alternative<CustomStageEntry>(slot)) {
        slot = CustomStageEntry{};
    }
    auto& entry = std::get<CustomStageEntry>(slot);
    entry.files.insert(entry.files.end(), files.begin(), files.end());
    updateState();
}

void StagingModel::addCustomSequenceEntry(
    const std::vector<std::pair<std::filesystem::path, std::filesystem::path>>& pairs, const std::string& path) {
    auto& slot = entries_[path];
    if (!std::holds_alternative<CustomSequencesEntry>(slot)) {
        slot = CustomSequencesEntry{};
    }
    auto& entry = std::get<CustomSequencesEntry>(slot);
    entry.seqMetaPairs.insert(entry.seqMetaPairs.end(), pairs.begin(), pairs.end());
    updateState();
}

void StagingModel::addCustomTextureEntry(
    const std::map<std::string, std::vector<std::pair<std::filesystem::path, TextureManifestEntry>>>&
        processedFiles) {
    for (const auto& [key, files] : processedFiles) {
        auto& slot = entries_[key];
        if (!std::holds_alternative<CustomTexturesEntry>(slot)) {
            slot = CustomTexturesEntry{};
        }
        auto& entry = std::get<CustomTexturesEntry>(slot);
        entry.files.insert(entry.files.end(), files.begin(), files.end());
    }
    updateState();
}

void StagingModel::addFile(const std::filesystem::path& file, const std::string& path) {
    addCustomStageEntries({file}, path);
}

void StagingModel::removeFile(const std::filesystem::path& file, const std::string& key) {
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        return;
    }

    bool nowEmpty = std::visit(
        [&file](auto& entry) {
            if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, CustomStageEntry>) {
                auto& v = entry.files;
                v.erase(std::remove(v.begin(), v.end(), file), v.end());
                return v.empty();
            } else if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, CustomSequencesEntry>) {
                auto& v = entry.seqMetaPairs;
                v.erase(std::remove_if(v.begin(), v.end(), [&file](const auto& pair) { return pair.first == file; }),
                        v.end());
                return v.empty();
            } else {
                auto& v = entry.files;
                v.erase(std::remove_if(v.begin(), v.end(), [&file](const auto& pair) { return pair.first == file; }),
                        v.end());
                return v.empty();
            }
        },
        it->second);

    if (nowEmpty) {
        entries_.erase(it);
    }
    updateState();
}

void StagingModel::clear() {
    entries_.clear();
    updateState();
}

void StagingModel::beginGeneration(int totalFiles) {
    isGenerating_ = true;
    totalFiles_ = totalFiles;
    filesProcessed_ = 0;
    emit changed();
}

void StagingModel::reportGenerationProgress(int processed) {
    filesProcessed_ = processed;
    emit generationProgress(processed, totalFiles_);
}

void StagingModel::finishGeneration() {
    isGenerating_ = false;
    emit generationFinished();
}

void StagingModel::failGeneration(const QString& error) {
    isGenerating_ = false;
    emit generationFailed(error);
}

void StagingModel::updateState() {
    state_ = entries_.empty() ? AppState::None : AppState::ChangesStaged;
    emit changed();
}

} // namespace bitdeck
