#include "folder_scan.h"

#include "path_utils.h"
#include "sha256.h"

namespace bitdeck {

std::vector<ScannedFile> scanFolder(const std::filesystem::path& root,
                                     const std::map<std::string, std::string>& knownHashes,
                                     const std::function<bool(const std::filesystem::path&)>& accept) {
    std::vector<ScannedFile> results;
    if (root.empty() || !std::filesystem::exists(root)) {
        return results;
    }

    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(root)) {
        if (!dirEntry.is_regular_file()) {
            continue;
        }
        if (accept && !accept(dirEntry.path())) {
            continue;
        }

        std::filesystem::path relative = std::filesystem::relative(dirEntry.path(), root);
        std::string relativeKey = normalizePath(relative.string());
        std::string dirKey = normalizePath(relative.parent_path().string());
        if (dirKey == ".") {
            dirKey.clear();
        }

        std::string hash = sha256Hex(readFileBytes(dirEntry.path()));

        FileStageStatus status = FileStageStatus::New;
        auto known = knownHashes.find(relativeKey);
        if (known != knownHashes.end()) {
            status = known->second == hash ? FileStageStatus::UpToDate : FileStageStatus::Modified;
        }

        results.push_back(ScannedFile{dirEntry.path(), relativeKey, dirKey, hash, status});
    }

    return results;
}

} // namespace bitdeck
