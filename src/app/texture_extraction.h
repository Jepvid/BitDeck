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
//
// When applyTlut is set, CI4/CI8 textures are colored using the palette
// paired with them in the archive's own display lists (see
// gfx_tlut_scan.h). Textures with no unambiguous pairing found are
// extracted as-is (their default synthetic grayscale palette), same as when
// the toggle is off.
TextureManifestMap extractTexturesToFolder(const std::vector<std::string>& archivePaths,
                                            const std::filesystem::path& targetDir, TaskProgress& progress,
                                            bool applyTlut = false);

} // namespace bitdeck
