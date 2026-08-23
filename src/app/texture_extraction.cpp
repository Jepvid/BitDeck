#include "texture_extraction.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "../archive/arc.h"
#include "../core/crc64.h"
#include "../core/gfx_tlut_scan.h"
#include "../core/image_codec.h"
#include "../core/resource.h"
#include "../core/resource_type.h"
#include "../core/sha256.h"
#include "../core/types/background.h"
#include "../core/types/texture.h"
#include "../games/mk64_tlut_map.h"
#include "../games/mm_tlut_map.h"
#include "../games/oot_tlut_map.h"
#include "../games/sf64_tlut_map.h"
#include "game_conventions_registry.h"
#include "texture_manifest_json.h"

namespace bitdeck {

namespace {

void writeFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

int countArchiveFiles(const std::vector<std::string>& archivePaths) {
    int total = 0;
    for (const auto& archivePath : archivePaths) {
        Arc arc(archivePath);
        total += static_cast<int>(arc.listItems().size());
        arc.close();
    }
    return total;
}

struct ResourceLocation {
    std::string archivePath;
    std::string name;
};

// Every resource's path hash (for TLUT lookups by CRC64), every display
// list's CI-texture-to-TLUT pairing (see gfx_tlut_scan.h) across all of
// archivePaths, and every confirmed-but-unpaired TLUT (a flipbook slot
// filled in by game code, not archive data), grouped by its own folder.
struct TlutIndex {
    std::unordered_map<uint64_t, ResourceLocation> hashToLocation;
    std::unordered_map<uint64_t, uint64_t> ciHashToTlutHash;
    std::unordered_map<std::string, std::vector<uint64_t>> folderToConfirmedTluts;

    // Per folder, the TLUT confirmed for each flipbook segment (eye/mouth --
    // see kEyeFlipbookSegment/kMouthFlipbookSegment).
    std::unordered_map<std::string, std::unordered_map<uint32_t, uint64_t>> folderToSegmentTluts;

    // Every "*TLUT"-named resource's own filename, grouped by folder.
    std::unordered_map<std::string, std::unordered_set<std::string>> folderToTlutNames;

    // True when a game-exclusive marker path ("models/" for MK64,
    // "parameter_static/" for MM, "objects/object_anubice/" for OOT,
    // "ast_option/" for SF64) was seen in archivePaths. Gates
    // mk64_tlut_map.h/mm_tlut_map.h/oot_tlut_map.h/sf64_tlut_map.h to their
    // own archives.
    bool looksLikeMk64 = false;
    bool looksLikeMm = false;
    bool looksLikeOot = false;
    bool looksLikeSf64 = false;
};

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Matches an uppercase "...TLUT" suffix (OOT/BK64), or a lowercase "tlut"
// segment/suffix or "_palette" suffix (MK64). Case-sensitive: OOT's
// auto-generated "..._TLUT_<hexaddr>" names would also match if lowercased.
bool looksLikeTlutName(const std::string& name) {
    if (endsWith(name, "TLUT")) {
        return true;
    }
    return name.rfind("tlut_", 0) == 0 || endsWith(name, "_tlut") || name.find("_tlut_") != std::string::npos ||
           endsWith(name, "_palette");
}

// A name with a known suffix (e.g. "Tex"/"TLUT") stripped and split into its
// non-digit "core" and digit characters, so "Shop2BgTex" and "ShopBg2TLUT"
// -- the same variant, but with the digit in a different position -- both
// normalize to core="ShopBg", digits="2" and compare equal.
struct NormalizedTextureName {
    std::string core;
    std::string digits;
};

NormalizedTextureName normalizeTextureName(std::string name, const std::string& suffix) {
    if (endsWith(name, suffix)) {
        name.resize(name.size() - suffix.size());
    }
    NormalizedTextureName result;
    for (char c : name) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            result.digits += c;
        } else {
            result.core += c;
        }
    }
    return result;
}

// A CI texture's TLUT by direct name correspondence, tried across several
// per-game naming conventions (see the candidate blocks below) in the
// texture's own folder, then -- for a name ending in "Tex" -- OOT's
// core+digit match against "<Base>TLUT" in the same folder or (for a
// "..._static" folder) the sibling "..._pal_static" folder: an exact
// core+digit match (e.g. "Bg2Tex"/"Bg2TLUT"), or the sole candidate present
// when nothing matches by digit (e.g. "Skybox1Tex".."Skybox5Tex" all
// sharing plain "SkyboxTLUT"). Returns nullopt on an absent or ambiguous
// match either way.
std::optional<uint64_t> findNamedTlutMatch(const std::string& fileName, const TlutIndex& index) {
    std::string baseName = std::filesystem::path(fileName).filename().string();
    std::string folder = std::filesystem::path(fileName).parent_path().string();

    auto exactIt = index.folderToTlutNames.find(folder);
    if (exactIt != index.folderToTlutNames.end()) {
        auto isAllDigits = [](const std::string& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
        };
        // Drops a trailing "_<segment>" (not necessarily numeric -- MK64
        // uses e.g. "_1p".."_4p" for player-indexed variants), if the name
        // has one -- shared by multiple candidate-generation strategies
        // below.
        auto withoutTrailingSegment = [](const std::string& s) -> std::optional<std::string> {
            size_t pos = s.rfind('_');
            if (pos != std::string::npos) {
                return s.substr(0, pos);
            }
            return std::nullopt;
        };

        std::vector<std::string> candidates;

        // BK64/Lighthouse: a "<name>_TLUT" sibling, tried with the texture's
        // full name and with a trailing segment dropped (e.g. "..._tex_0" /
        // "..._tex_0_TLUT", or "..._0_1" / "..._0_TLUT").
        candidates.push_back(baseName + "_TLUT");
        if (auto stripped = withoutTrailingSegment(baseName)) {
            candidates.push_back(*stripped + "_TLUT");
        }

        // MK64/Spaghettikart: "_texture_" replaced by "_tlut_" (e.g.
        // "common_texture_debug_font" / "common_tlut_debug_font"), tried
        // with the full name and with a trailing segment dropped (e.g.
        // "..._font_1" sharing "..._tlut_font" with its siblings, or
        // "..._player_emblem_1p" sharing "..._tlut_player_emblem").
        size_t texturePos = baseName.find("_texture_");
        if (texturePos != std::string::npos) {
            std::string swapped = baseName.substr(0, texturePos) + "_tlut_" + baseName.substr(texturePos + 9);
            candidates.push_back(swapped);
            if (auto stripped = withoutTrailingSegment(swapped)) {
                candidates.push_back(*stripped);
            }
        }

        // MK64/Spaghettikart kart wheels: "<prefix>_frame<N>_wheel<M>"
        // paired with "<prefix>_<N>_tlut_wheel_<M>". Kart body frames --
        // "<prefix>_frame<N>" with no wheel suffix -- pair with a single
        // shared "<prefix>_palette" instead.
        size_t framePos = baseName.find("_frame");
        if (framePos != std::string::npos) {
            size_t digitsStart = framePos + 6; // length of "_frame"
            size_t digitsEnd = digitsStart;
            while (digitsEnd < baseName.size() && std::isdigit(static_cast<unsigned char>(baseName[digitsEnd]))) {
                ++digitsEnd;
            }
            const std::string wheelMarker = "_wheel";
            std::string afterFrameDigits = baseName.substr(digitsEnd);
            if (digitsEnd > digitsStart && afterFrameDigits.rfind(wheelMarker, 0) == 0) {
                std::string wheelDigits = afterFrameDigits.substr(wheelMarker.size());
                if (isAllDigits(wheelDigits)) {
                    std::string frameDigits = baseName.substr(digitsStart, digitsEnd - digitsStart);
                    candidates.push_back(baseName.substr(0, framePos) + "_" + frameDigits + "_tlut_wheel_" +
                                          wheelDigits);
                }
            } else if (digitsEnd > digitsStart && digitsEnd == baseName.size()) {
                candidates.push_back(baseName.substr(0, framePos) + "_palette");
            }
        }

        for (const auto& candidate : candidates) {
            if (exactIt->second.count(candidate) != 0) {
                return crc64(folder + "/" + candidate);
            }
        }
    }

    if (!endsWith(baseName, "Tex")) {
        return std::nullopt;
    }
    NormalizedTextureName ciName = normalizeTextureName(baseName, "Tex");

    std::vector<std::string> candidateFolders = {folder};
    const std::string staticSuffix = "_static";
    if (endsWith(folder, staticSuffix) && !endsWith(folder, "_pal_static")) {
        candidateFolders.push_back(folder.substr(0, folder.size() - staticSuffix.size()) + "_pal_static");
    }

    std::vector<std::string> allCandidates;
    std::vector<std::string> exactMatches;
    for (const auto& candidateFolder : candidateFolders) {
        auto it = index.folderToTlutNames.find(candidateFolder);
        if (it == index.folderToTlutNames.end()) {
            continue;
        }
        for (const auto& tlutName : it->second) {
            std::string fullPath = candidateFolder + "/" + tlutName;
            allCandidates.push_back(fullPath);
            NormalizedTextureName tlutNorm = normalizeTextureName(tlutName, "TLUT");
            if (tlutNorm.core == ciName.core && tlutNorm.digits == ciName.digits) {
                exactMatches.push_back(fullPath);
            }
        }
    }

    if (exactMatches.size() == 1) {
        return crc64(exactMatches.front());
    }
    if (exactMatches.empty() && allCandidates.size() == 1) {
        return crc64(allCandidates.front());
    }
    return std::nullopt;
}

TlutIndex buildTlutIndex(const std::vector<std::string>& archivePaths) {
    TlutIndex index;
    std::unordered_set<uint64_t> confirmedTlutHashes;

    for (const auto& archivePath : archivePaths) {
        Arc arc(archivePath);
        arc.listItems([&](const std::string& fileName, const std::vector<uint8_t>& data) {
            index.hashToLocation[crc64(fileName)] = ResourceLocation{archivePath, fileName};
            if (fileName.rfind("models/", 0) == 0) {
                index.looksLikeMk64 = true;
            } else if (fileName.rfind("parameter_static/", 0) == 0) {
                index.looksLikeMm = true;
            } else if (fileName.rfind("objects/object_anubice/", 0) == 0) {
                index.looksLikeOot = true;
            } else if (fileName.rfind("ast_option/", 0) == 0) {
                index.looksLikeSf64 = true;
            }

            std::string baseName = std::filesystem::path(fileName).filename().string();
            if (looksLikeTlutName(baseName)) {
                std::string folder = std::filesystem::path(fileName).parent_path().string();
                index.folderToTlutNames[folder].insert(baseName);
            }

            if (data.empty()) {
                return;
            }

            DisplayListTlutInfo dlInfo;
            if (data[0] == '<') {
                dlInfo = scanXmlDisplayListForTlutInfo(data);
            } else {
                Resource sniffer;
                sniffer.rawLoad = true;
                sniffer.open(data);
                if (sniffer.resourceType() != ResourceType::DisplayList) {
                    return;
                }
                dlInfo = scanDisplayListForTlutInfo(data);
            }

            for (const auto& [ciHash, tlutHash] : dlInfo.ciToTlut) {
                index.ciHashToTlutHash[ciHash] = tlutHash;
            }
            for (uint64_t tlutHash : dlInfo.confirmedTluts) {
                confirmedTlutHashes.insert(tlutHash);
            }
            if (!dlInfo.segmentToTlut.empty()) {
                std::string folder = std::filesystem::path(fileName).parent_path().string();
                auto& segmentMap = index.folderToSegmentTluts[folder];
                for (const auto& [segment, tlutHash] : dlInfo.segmentToTlut) {
                    segmentMap[segment] = tlutHash;
                }
            }
        });
        arc.close();
    }

    // Re-indexes every "alt/"-namespaced entry under its canonical
    // (non-"alt/") path's hash too.
    std::vector<std::pair<uint64_t, ResourceLocation>> altOverrides;
    for (const auto& [hash, location] : index.hashToLocation) {
        if (location.name.rfind("alt/", 0) == 0) {
            altOverrides.emplace_back(crc64(location.name.substr(4)), location);
        }
    }
    for (auto& [canonicalHash, location] : altOverrides) {
        index.hashToLocation[canonicalHash] = std::move(location);
    }

    for (uint64_t tlutHash : confirmedTlutHashes) {
        auto locationIt = index.hashToLocation.find(tlutHash);
        if (locationIt == index.hashToLocation.end()) {
            continue;
        }
        std::string folder = std::filesystem::path(locationIt->second.name).parent_path().string();
        index.folderToConfirmedTluts[folder].push_back(tlutHash);
    }

    return index;
}

// Finds the TLUT hash paired with fileName: a baked ground-truth match
// (MK64, MM, OOT, SF64), a display list that named both sides, a
// name-matched candidate, this folder's one confirmed TLUT, or the
// eye/mouth flipbook segment its own name suggests. Returns nullopt if none
// apply.
std::optional<uint64_t> findTlutHash(const std::string& fileName, const TlutIndex& index) {
    if (index.looksLikeMk64) {
        if (auto baked = mk64TlutArchivePathFor(fileName); baked.has_value()) {
            return crc64(*baked);
        }
    }
    if (index.looksLikeMm) {
        if (auto baked = mmTlutArchivePathFor(fileName); baked.has_value()) {
            return crc64(*baked);
        }
    }
    if (index.looksLikeOot) {
        if (auto baked = ootTlutArchivePathFor(fileName); baked.has_value()) {
            return crc64(*baked);
        }
    }
    if (index.looksLikeSf64) {
        if (auto baked = sf64TlutArchivePathFor(fileName); baked.has_value()) {
            return crc64(*baked);
        }
    }

    auto ciIt = index.ciHashToTlutHash.find(crc64(fileName));
    if (ciIt != index.ciHashToTlutHash.end()) {
        return ciIt->second;
    }
    if (auto named = findNamedTlutMatch(fileName, index); named.has_value()) {
        return named;
    }

    std::string folder = std::filesystem::path(fileName).parent_path().string();
    auto folderIt = index.folderToConfirmedTluts.find(folder);
    if (folderIt != index.folderToConfirmedTluts.end() && folderIt->second.size() == 1) {
        return folderIt->second.front();
    }

    // Ambiguous folder (0 or 2+ confirmed TLUTs): try the eye/mouth
    // flipbook segment this texture's own name suggests.
    auto segmentIt = index.folderToSegmentTluts.find(folder);
    if (segmentIt == index.folderToSegmentTluts.end()) {
        return std::nullopt;
    }
    std::string baseName = std::filesystem::path(fileName).filename().string();
    std::transform(baseName.begin(), baseName.end(), baseName.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    auto eyeIt = segmentIt->second.find(kEyeFlipbookSegment);
    auto mouthIt = segmentIt->second.find(kMouthFlipbookSegment);
    if (baseName.find("eye") != std::string::npos && eyeIt != segmentIt->second.end()) {
        return eyeIt->second;
    }
    if (baseName.find("mouth") != std::string::npos && mouthIt != segmentIt->second.end()) {
        return mouthIt->second;
    }
    return std::nullopt;
}

// Returns the kart's shared "<prefix>_palette" character palette's archive
// path for a wheel-frame texture named "<prefix>_frame<N>_wheel<M>", or
// nullopt for any other texture name.
std::optional<std::string> findKartCharacterPaletteName(const std::string& fileName, const TlutIndex& index) {
    std::string baseName = std::filesystem::path(fileName).filename().string();
    std::string folder = std::filesystem::path(fileName).parent_path().string();

    size_t framePos = baseName.find("_frame");
    size_t wheelPos = baseName.find("_wheel");
    if (framePos == std::string::npos || wheelPos == std::string::npos || wheelPos <= framePos) {
        return std::nullopt;
    }

    std::string candidate = baseName.substr(0, framePos) + "_palette";
    auto it = index.folderToTlutNames.find(folder);
    if (it != index.folderToTlutNames.end() && it->second.count(candidate) != 0) {
        return folder + "/" + candidate;
    }
    return std::nullopt;
}

// Matches MK64's "texture_red_shell_<N>" naming.
bool isMk64RedShellTexture(const std::string& fileName) {
    std::string baseName = std::filesystem::path(fileName).filename().string();
    return baseName.rfind("texture_red_shell_", 0) == 0;
}

// Swaps each palette entry's red and green channels, replicating
// SpaghettiKart's init_red_shell_texture() (src/racing/actors.c): red
// shells have no stored palette of their own, only common_tlut_green_shell
// with red and green swapped at runtime.
RgbaImage swapRedGreenChannels(const RgbaImage& palette) {
    RgbaImage swapped = palette;
    for (int y = 0; y < swapped.height; ++y) {
        for (int x = 0; x < swapped.width; ++x) {
            swapped.setRgba(x, y, palette.g(x, y), palette.r(x, y), palette.b(x, y), palette.a(x, y));
        }
    }
    return swapped;
}

// Loads, decodes, and caches (keyed by tlutHash) the palette resource at
// that hash. Reuses one open Arc per archive path across every call.
const RgbaImage* loadPaletteByHash(uint64_t tlutHash, const TlutIndex& index,
                                    std::unordered_map<uint64_t, RgbaImage>& paletteCache,
                                    std::unordered_map<std::string, std::unique_ptr<Arc>>& archiveCache) {
    auto cached = paletteCache.find(tlutHash);
    if (cached != paletteCache.end()) {
        return &cached->second;
    }

    auto locationIt = index.hashToLocation.find(tlutHash);
    if (locationIt == index.hashToLocation.end()) {
        return nullptr;
    }

    auto& arc = archiveCache[locationIt->second.archivePath];
    if (!arc) {
        arc = std::make_unique<Arc>(locationIt->second.archivePath);
    }
    auto bytes = arc->readFile(locationIt->second.name);
    if (!bytes.has_value()) {
        return nullptr;
    }

    Texture tlutTexture;
    tlutTexture.open(*bytes);
    if (!tlutTexture.isValid()) {
        return nullptr;
    }

    // Palette width comes from the resource's real texData byte count, not
    // its own declared width/height (seen as low as width=16 declared over
    // a 512-byte/256-color payload).
    int realColors = static_cast<int>(
        static_cast<double>(tlutTexture.texData().size()) / textureTypePixelMultiplier(tlutTexture.textureType()));
    RgbaImage palette = decodeN64Texture(tlutTexture.texData(), tlutTexture.textureType(), realColors, 1);
    return &paletteCache.emplace(tlutHash, std::move(palette)).first->second;
}

// Resolves and applies fileName's TLUT(s) onto decoded in place. Most
// textures get one palette at index 0; kart wheel frames get their
// matching character's kart palette at index 0 and their own
// locally-matched wheel palette stacked right after it (see
// findKartCharacterPaletteName).
void resolveAndApplyTlut(RgbaImage& decoded, const std::string& fileName, const TlutIndex& index,
                          std::unordered_map<uint64_t, RgbaImage>& paletteCache,
                          std::unordered_map<std::string, std::unique_ptr<Arc>>& archiveCache) {
    std::optional<uint64_t> tlutHash = findTlutHash(fileName, index);
    if (!tlutHash.has_value()) {
        return;
    }

    std::optional<std::string> characterPaletteName = findKartCharacterPaletteName(fileName, index);

    if (characterPaletteName.has_value()) {
        uint64_t characterHash = crc64(*characterPaletteName);
        if (const RgbaImage* characterPalette = loadPaletteByHash(characterHash, index, paletteCache, archiveCache)) {
            applyTlutPalette(decoded, *characterPalette, 0);
            if (const RgbaImage* secondaryPalette = loadPaletteByHash(*tlutHash, index, paletteCache, archiveCache)) {
                applyTlutPalette(decoded, *secondaryPalette, characterPalette->width * characterPalette->height);
            }
            return;
        }
    }

    if (const RgbaImage* palette = loadPaletteByHash(*tlutHash, index, paletteCache, archiveCache)) {
        if (isMk64RedShellTexture(fileName)) {
            applyTlutPalette(decoded, swapRedGreenChannels(*palette), 0);
        } else {
            applyTlutPalette(decoded, *palette, 0);
        }
    }
}

} // namespace

TextureManifestMap extractTexturesToFolder(const std::vector<std::string>& archivePaths,
                                            const std::filesystem::path& targetDir, TaskProgress& progress,
                                            bool applyTlut) {
    TextureManifestMap manifest;
    if (archivePaths.empty()) {
        return manifest;
    }

    if (std::filesystem::exists(targetDir)) {
        std::filesystem::remove_all(targetDir);
    }
    std::filesystem::create_directories(targetDir);

    int total = countArchiveFiles(archivePaths);
    int processed = 0;

    TlutIndex tlutIndex;
    std::unordered_map<uint64_t, RgbaImage> tlutPaletteCache;
    std::unordered_map<std::string, std::unique_ptr<Arc>> tlutArchiveCache;
    if (applyTlut) {
        tlutIndex = buildTlutIndex(archivePaths);
    }

    for (const auto& archivePath : archivePaths) {
        Arc arc(archivePath);
        arc.listItems([&](const std::string& fileName, const std::vector<uint8_t>& data) {
            Resource sniffer;
            sniffer.rawLoad = true;
            sniffer.open(data);

            if (sniffer.resourceType() == ResourceType::Texture) {
                Texture texture;
                texture.open(data);
                if (texture.isValid() && texture.textureType() == TextureType::JPEG32bpp) {
                    // Raw passthrough, not decoded as an N64 pixel format or
                    // as JPEG (e.g. SM64's non-image ipl3_raw font blobs).
                    writeFileBytes(targetDir / (fileName + ".raw"), texture.texData());
                } else if (texture.isValid()) {
                    RgbaImage decoded =
                        decodeN64Texture(texture.texData(), texture.textureType(), texture.width(), texture.height());
                    if (applyTlut && texture.isPalette()) {
                        resolveAndApplyTlut(decoded, fileName, tlutIndex, tlutPaletteCache, tlutArchiveCache);
                    }
                    std::vector<uint8_t> pngBytes = encodePng(decoded);
                    writeFileBytes(targetDir / (fileName + ".png"), pngBytes);

                    TextureManifestEntry entry;
                    entry.hash = sha256Hex(pngBytes);
                    entry.textureType = texture.textureType();
                    entry.textureWidth = texture.width();
                    entry.textureHeight = texture.height();
                    manifest[fileName] = entry;
                }
            } else if (sniffer.resourceType() == ResourceType::SohBackground) {
                Background background;
                background.open(data);
                if (background.isValid()) {
                    // Written as-is (already a JPEG blob); decoded only to
                    // recover width/height for the manifest.
                    writeFileBytes(targetDir / (fileName + ".jpg"), background.texData());
                    RgbaImage decoded = decodeJpeg(background.texData());

                    TextureManifestEntry entry;
                    entry.hash = sha256Hex(background.texData());
                    entry.textureType = TextureType::JPEG32bpp;
                    entry.textureWidth = decoded.width;
                    entry.textureHeight = decoded.height;
                    manifest[fileName] = entry;
                }
            }

            progress.reportProgress(++processed, total);
        });
        arc.close();
    }

    ArchiveLister lister = [](const std::string& archivePath, const ArchiveFileVisitor& visitor) {
        Arc arc(archivePath);
        arc.listItems(visitor);
        arc.close();
    };
    for (const auto* convention : gameTextureConventions()) {
        convention->recordExtractionMetadata(archivePaths, manifest, lister);
    }

    QByteArray manifestJson = writeManifestJson(manifest);
    writeFileBytes(targetDir / "manifest.json", std::vector<uint8_t>(manifestJson.begin(), manifestJson.end()));

    return manifest;
}

} // namespace bitdeck
