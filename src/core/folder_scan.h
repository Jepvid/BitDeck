#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace bitdeck {

enum class FileStageStatus { New, Modified, UpToDate };

struct ScannedFile {
    std::filesystem::path absolutePath;
    std::string relativeKey; // normalized, forward-slash, relative to root
    std::string dirKey;      // relativeKey's parent dir, "" for root
    std::string contentHash; // sha256 hex
    FileStageStatus status;
};

// Recursively walks root, hashing every regular file accepted by accept (or
// every regular file if accept is null) and classifying it against
// knownHashes (relativeKey -> previously-staged sha256 hex). Returns an
// empty vector if root doesn't exist.
std::vector<ScannedFile> scanFolder(const std::filesystem::path& root,
                                     const std::map<std::string, std::string>& knownHashes,
                                     const std::function<bool(const std::filesystem::path&)>& accept = nullptr);

} // namespace bitdeck
