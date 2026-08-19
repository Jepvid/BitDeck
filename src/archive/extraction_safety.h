#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace bitdeck {

// Resolves an archive entry name against outputRoot, or nullopt if the
// resolved path would fall outside outputRoot (zip-slip protection).
// outputRoot must exist.
std::optional<std::filesystem::path> safeExtractionPath(const std::filesystem::path& outputRoot,
                                                          const std::string& entryName);

} // namespace bitdeck
