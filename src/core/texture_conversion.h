#pragma once

#include <optional>
#include <vector>

#include "texture_manifest_entry.h"
#include "types/texture.h"

namespace bitdeck {

// PNG -> Texture, handling replacements that aren't the manifest entry's
// original size (LOAD_AS_RAW + scale factor baked into the extended header)
// and additive font-glyph atlases (tile-multiple detection + canvas padding).
// Returns nullopt if the PNG can't be decoded, or if the entry declares a
// palette type at original size but the source PNG isn't actually indexed.
std::optional<Texture> convertPngToTexture(const std::vector<uint8_t>& pngBytes, const TextureManifestEntry& entry);

// JPEG -> Texture: always RGBA32bpp + LOAD_AS_RAW + scale factor.
std::optional<Texture> convertJpegToTexture(const std::vector<uint8_t>& jpegBytes, const TextureManifestEntry& entry);

// Dispatches on entry.textureType == JPEG32bpp.
std::optional<Texture> convertImageToTexture(const std::vector<uint8_t>& imageBytes, const TextureManifestEntry& entry);

} // namespace bitdeck
