#include "archive_generator.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "../archive/arc.h"
#include "../core/path_utils.h"
#include "../core/texture_conversion.h"
#include "../core/types/sequence.h"

namespace bitdeck {

namespace {

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path.string());
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string readFileText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path.string());
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

void addCustomStageEntry(Arc& arc, const std::string& key, const CustomStageEntry& entry, bool compress,
                          int& processed, int total, TaskProgress& progress) {
    for (const auto& file : entry.files) {
        std::string archivePath = normalizePath(key + "/" + file.filename().string());
        arc.addFile(archivePath, readFileBytes(file), compress);
        progress.reportProgress(++processed, total);
    }
}

void addCustomSequencesEntry(Arc& arc, const std::string& key, const CustomSequencesEntry& entry, bool compress,
                              int& processed, int total, TaskProgress& progress) {
    for (const auto& [seqFile, metaFile] : entry.seqMetaPairs) {
        Sequence sequence = Sequence::fromSeqFile(readFileBytes(seqFile), readFileText(metaFile));
        std::string archivePath = normalizePath(key + "/" + sequence.path());
        arc.addFile(archivePath, sequence.build(), compress);
        progress.reportProgress(++processed, total);
    }
}

void addCustomTexturesEntry(Arc& arc, const std::string& key, const CustomTexturesEntry& entry, bool compress,
                             bool prependAlt, int& processed, int total, TaskProgress& progress) {
    for (const auto& [imageFile, manifestEntry] : entry.files) {
        std::optional<Texture> texture = convertImageToTexture(readFileBytes(imageFile), manifestEntry);
        if (texture.has_value()) {
            std::string fileName = manifestEntry.targetName.has_value() ? *manifestEntry.targetName
                                                                          : key + "/" + imageFile.stem().string();
            std::string archivePath = normalizePath(prependAlt ? "alt/" + fileName : fileName);
            arc.addFile(archivePath, texture->build(), compress);
        }
        progress.reportProgress(++processed, total);
    }
}

} // namespace

void generateArchive(const std::map<std::string, StageEntry>& entries, const std::string& outputPath, bool compress,
                      bool prependAlt, TaskProgress& progress) {
    int total = 0;
    for (const auto& [key, entry] : entries) {
        total += static_cast<int>(stageEntryFileCount(entry));
    }

    // Removes any existing file at outputPath so Arc creates a fresh
    // archive instead of opening the old one for read/append.
    if (std::filesystem::exists(outputPath)) {
        std::filesystem::remove(outputPath);
    }

    Arc arc(outputPath);
    int processed = 0;
    for (const auto& [key, entry] : entries) {
        if (const auto* e = std::get_if<CustomStageEntry>(&entry)) {
            addCustomStageEntry(arc, key, *e, compress, processed, total, progress);
        } else if (const auto* seqEntry = std::get_if<CustomSequencesEntry>(&entry)) {
            addCustomSequencesEntry(arc, key, *seqEntry, compress, processed, total, progress);
        } else {
            addCustomTexturesEntry(arc, key, std::get<CustomTexturesEntry>(entry), compress, prependAlt, processed,
                                    total, progress);
        }
    }
    arc.close();
}

} // namespace bitdeck
