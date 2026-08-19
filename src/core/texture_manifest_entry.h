#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "types/texture.h"

namespace bitdeck {

enum class TextureEntryKind {
    Replacement,
    Additive,
    AdditiveFontGlyph,
};

struct TextureManifestEntry {
    std::string hash;
    TextureType textureType = TextureType::Error;
    int32_t textureWidth = 0;
    int32_t textureHeight = 0;
    std::optional<int32_t> tileWidth;
    std::optional<int32_t> tileHeight;
    TextureEntryKind kind = TextureEntryKind::Replacement;
    std::optional<std::string> targetName;

    // Content hash of the source file as of the most recent scan. Not
    // persisted to the manifest.
    std::optional<std::string> sourceHash;
};

} // namespace bitdeck
