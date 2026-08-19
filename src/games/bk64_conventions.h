#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "game_texture_conventions.h"

namespace bitdeck {

class Bk64TextureConventions : public GameTextureConventions {
public:
    std::optional<TextureManifestEntry> resolveAdditiveEntry(
        const TextureManifestMap& manifest, const std::string& target) const override;

    void recordExtractionMetadata(
        const std::vector<std::string>& otrPaths,
        TextureManifestMap& entries,
        const ArchiveLister& listArchive) const override;
};

// Parses a BK64 sprite resource's tile-position table, at fixed offset
// 0x40+10+4 into the resource bytes. Keyed by (frame, chunk) -> (x, y).
// Returns nullopt if the data doesn't look like a valid table.
std::optional<std::map<std::pair<int, int>, std::pair<int16_t, int16_t>>> parseSpriteTilePositions(
    const std::vector<uint8_t>& data);

} // namespace bitdeck
