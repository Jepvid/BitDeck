#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../core/texture_manifest_entry.h"

namespace bitdeck {

using TextureManifestMap = std::map<std::string, TextureManifestEntry>;

// Invoked once per file inside an archive during listing.
using ArchiveFileVisitor = std::function<void(const std::string& fileName, const std::vector<uint8_t>& data)>;

// Lists every file inside the archive at archivePath, invoking visitor for each.
using ArchiveLister = std::function<void(const std::string& archivePath, const ArchiveFileVisitor& visitor)>;

// Pluggable per-game texture-path conventions. Each game port (BK64, SOH,
// ...) gets its own implementation registered with the replace-textures flow.
class GameTextureConventions {
public:
    virtual ~GameTextureConventions() = default;

    // Resolves a source-image target path that has no direct manifest entry
    // into an additive/derived entry from a template entry, or nullopt if
    // this game's conventions don't recognize the path.
    virtual std::optional<TextureManifestEntry> resolveAdditiveEntry(
        const TextureManifestMap& manifest, const std::string& target) const = 0;

    // After extracting textures from otrPaths into entries, lets this game
    // enrich entries with extra metadata (e.g. tile dimensions) read
    // directly from the archives.
    virtual void recordExtractionMetadata(
        const std::vector<std::string>& otrPaths,
        TextureManifestMap& entries,
        const ArchiveLister& listArchive) const = 0;
};

} // namespace bitdeck
