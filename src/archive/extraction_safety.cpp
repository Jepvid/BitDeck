#include "extraction_safety.h"

#include "../core/path_utils.h"

namespace bitdeck {

std::optional<std::filesystem::path> safeExtractionPath(const std::filesystem::path& outputRoot,
                                                          const std::string& entryName) {
    std::filesystem::path normalizedRoot = std::filesystem::weakly_canonical(outputRoot);
    std::filesystem::path candidate =
        (normalizedRoot / normalizePath(entryName)).lexically_normal();

    auto rootIt = normalizedRoot.begin();
    auto candidateIt = candidate.begin();
    for (; rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end() || *candidateIt != *rootIt) {
            return std::nullopt;
        }
    }
    return candidate;
}

} // namespace bitdeck
