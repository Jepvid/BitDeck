#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../games/game_texture_conventions.h"
#include "background_worker.h"

namespace bitdeck {

// Extracts every Texture/Background resource found in archivePaths into
// targetDir as PNG/JPG files (recreating each entry's archive-relative
// subfolders), enriches the result via each registered game's
// recordExtractionMetadata() (e.g. BK64 tile positions), writes
// targetDir/manifest.json, and returns the manifest. targetDir is deleted
// and recreated first if it already exists.
TextureManifestMap extractTexturesToFolder(const std::vector<std::string>& archivePaths,
                                            const std::filesystem::path& targetDir, TaskProgress& progress);

} // namespace bitdeck
