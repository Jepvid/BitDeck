#include "bk64_conventions.h"

#include <regex>

namespace bitdeck {

namespace {

TextureManifestEntry additiveFrom(const TextureManifestEntry& tmpl, TextureEntryKind kind) {
    TextureManifestEntry entry;
    entry.hash = "";
    entry.textureType = tmpl.textureType;
    entry.textureWidth = tmpl.textureWidth;
    entry.textureHeight = tmpl.textureHeight;
    entry.tileWidth = tmpl.tileWidth;
    entry.tileHeight = tmpl.tileHeight;
    entry.kind = kind;
    return entry;
}

std::optional<TextureManifestEntry> lookup(const TextureManifestMap& manifest, const std::string& key) {
    auto it = manifest.find(key);
    if (it == manifest.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace

std::optional<TextureManifestEntry> Bk64TextureConventions::resolveAdditiveEntry(
    const TextureManifestMap& manifest, const std::string& target) const {
    static const std::regex kColorVariantPattern(R"(^(.+_\d+_\d+)_(BLUE|GREEN|ORANGE|PURPLE|YELLOW)$)");
    static const std::regex kBoldFontPattern(R"(^(.*)boldfont/(.+_\d+_\d+)_[0-9A-Fa-f]+$)");
    static const std::regex kLangScopedPattern(R"(^(.*)lang/[^/]+/(.+)$)");

    std::smatch colorMatch;
    if (std::regex_match(target, colorMatch, kColorVariantPattern)) {
        auto tmpl = lookup(manifest, colorMatch[1].str());
        if (tmpl) {
            return additiveFrom(*tmpl, TextureEntryKind::Additive);
        }
    }

    std::smatch boldMatch;
    if (std::regex_match(target, boldMatch, kBoldFontPattern)) {
        auto tmpl = lookup(manifest, boldMatch[1].str() + "sprite/" + boldMatch[2].str());
        if (tmpl) {
            return additiveFrom(*tmpl, TextureEntryKind::AdditiveFontGlyph);
        }
    }

    std::smatch langMatch;
    if (std::regex_match(target, langMatch, kLangScopedPattern)) {
        auto tmpl = lookup(manifest, langMatch[1].str() + langMatch[2].str());
        if (tmpl) {
            return additiveFrom(*tmpl, TextureEntryKind::Additive);
        }
    }

    return std::nullopt;
}

void Bk64TextureConventions::recordExtractionMetadata(
    const std::vector<std::string>& otrPaths,
    TextureManifestMap& entries,
    const ArchiveLister& listArchive) const {
    static const std::regex kSpriteChunkPattern(R"(^(.+)_(\d+)_(\d+)$)");

    struct WantedChunk {
        std::string fullKey;
        int frame;
        int chunk;
    };
    std::map<std::string, std::vector<WantedChunk>> wanted;
    for (const auto& [key, entry] : entries) {
        std::smatch match;
        if (!std::regex_match(key, match, kSpriteChunkPattern)) {
            continue;
        }
        wanted[match[1].str()].push_back({key, std::stoi(match[2].str()), std::stoi(match[3].str())});
    }
    if (wanted.empty()) {
        return;
    }

    for (const auto& otrPath : otrPaths) {
        listArchive(otrPath, [&](const std::string& fileName, const std::vector<uint8_t>& data) {
            auto wantedIt = wanted.find(fileName);
            if (wantedIt == wanted.end()) {
                return;
            }
            auto tiles = parseSpriteTilePositions(data);
            if (!tiles) {
                return;
            }
            for (const auto& chunk : wantedIt->second) {
                auto tileIt = tiles->find({chunk.frame, chunk.chunk});
                auto entryIt = entries.find(chunk.fullKey);
                if (tileIt == tiles->end() || entryIt == entries.end()) {
                    continue;
                }
                int16_t tileWidth = tileIt->second.first;
                int16_t tileHeight = tileIt->second.second;
                TextureManifestEntry& entry = entryIt->second;
                bool valid = tileWidth > 0 && tileHeight > 0 &&
                             tileWidth <= entry.textureWidth && tileHeight <= entry.textureHeight &&
                             (tileWidth != entry.textureWidth || tileHeight != entry.textureHeight);
                if (valid) {
                    entry.tileWidth = tileWidth;
                    entry.tileHeight = tileHeight;
                }
            }
        });
    }
}

std::optional<std::map<std::pair<int, int>, std::pair<int16_t, int16_t>>> parseSpriteTilePositions(
    const std::vector<uint8_t>& data) {
    size_t offset = 0x40 + 10 + 4;
    if (data.size() < offset + 4) {
        return std::nullopt;
    }

    auto readU32LE = [&data](size_t o) {
        return static_cast<uint32_t>(data[o]) | (static_cast<uint32_t>(data[o + 1]) << 8) |
               (static_cast<uint32_t>(data[o + 2]) << 16) | (static_cast<uint32_t>(data[o + 3]) << 24);
    };
    auto readI16LE = [&data](size_t o) -> int16_t {
        uint16_t v = static_cast<uint16_t>(data[o]) | (static_cast<uint16_t>(data[o + 1]) << 8);
        return static_cast<int16_t>(v);
    };
    auto readU16LE = [&data](size_t o) -> uint16_t {
        return static_cast<uint16_t>(data[o]) | (static_cast<uint16_t>(data[o + 1]) << 8);
    };

    uint32_t positionCount = readU32LE(offset);
    offset += 4;
    if (positionCount == 0 || positionCount > 0x4000 ||
        data.size() < offset + static_cast<size_t>(positionCount) * 4 + 4) {
        return std::nullopt;
    }

    std::vector<std::pair<int16_t, int16_t>> positions;
    positions.reserve(positionCount);
    for (uint32_t i = 0; i < positionCount; ++i) {
        positions.emplace_back(readI16LE(offset), readI16LE(offset + 2));
        offset += 4;
    }

    uint32_t frameCount = readU32LE(offset);
    offset += 4;
    if (frameCount == 0 || frameCount > 0x1000 || data.size() < offset + static_cast<size_t>(frameCount) * 2) {
        return std::nullopt;
    }

    std::map<std::pair<int, int>, std::pair<int16_t, int16_t>> tiles;
    size_t index = 0;
    for (uint32_t frame = 0; frame < frameCount; ++frame) {
        uint16_t count = readU16LE(offset + frame * 2);
        for (uint32_t chunk = 0; chunk < count; ++chunk) {
            if (index < positions.size()) {
                tiles[{static_cast<int>(frame), static_cast<int>(chunk)}] = positions[index];
            }
            ++index;
        }
    }
    return tiles;
}

} // namespace bitdeck
