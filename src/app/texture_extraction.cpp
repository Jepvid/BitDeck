#include "texture_extraction.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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

// Indexes every resource's path hash (for TLUT lookups by CRC64), merges
// every display list's CI-texture-to-TLUT pairings (see gfx_tlut_scan.h)
// across all of archivePaths, and groups every confirmed-but-unpaired TLUT
// (a flipbook slot filled in by game code, not archive data -- see the
// "readable material" scanner) by its own folder, for the fallback below.
struct TlutIndex {
    std::unordered_map<uint64_t, ResourceLocation> hashToLocation;
    std::unordered_map<uint64_t, uint64_t> ciHashToTlutHash;
    std::unordered_map<std::string, std::vector<uint64_t>> folderToConfirmedTluts;

    // Per folder, the TLUT confirmed for each flipbook segment (eye/mouth --
    // see kEyeFlipbookSegment/kMouthFlipbookSegment). Disambiguates a folder
    // with two confirmed TLUTs when folderToConfirmedTluts alone can't.
    std::unordered_map<std::string, std::unordered_map<uint32_t, uint64_t>> folderToSegmentTluts;

    // Every "*TLUT"-named resource's own filename, grouped by folder -- for
    // matching a "<Base>Tex" texture to a "<Base>TLUT" resource by name even
    // when no display list ever loads it (some backgrounds/HUD textures are
    // drawn by direct C code, not a Gfx command stream).
    std::unordered_map<std::string, std::vector<std::string>> folderToTlutNames;
};

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
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

// A CI texture's TLUT by direct name correspondence: "<Base>TLUT" in the
// same folder, or (for a "..._static" folder) the sibling "..._pal_static"
// folder. Prefers an exact core+digit match (handles a variant set with its
// own numbered TLUT each, e.g. "Bg2Tex"/"Bg2TLUT"); falls back to "the only
// TLUT candidate around" when nothing matches by digit (handles a variant
// set that all share one common, undigited TLUT, e.g. 5 skybox frames named
// "Skybox1Tex".."Skybox5Tex" all sharing plain "SkyboxTLUT"). An ambiguous
// or absent match either way is left for the caller's other fallbacks.
std::optional<uint64_t> findNamedTlutMatch(const std::string& fileName, const TlutIndex& index) {
    std::string baseName = std::filesystem::path(fileName).filename().string();
    if (!endsWith(baseName, "Tex")) {
        return std::nullopt;
    }
    NormalizedTextureName ciName = normalizeTextureName(baseName, "Tex");

    std::string folder = std::filesystem::path(fileName).parent_path().string();
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

            std::string baseName = std::filesystem::path(fileName).filename().string();
            if (endsWith(baseName, "TLUT")) {
                std::string folder = std::filesystem::path(fileName).parent_path().string();
                index.folderToTlutNames[folder].push_back(baseName);
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
    // (non-"alt/") path's hash, so a mod override resolves for hashes
    // computed from the canonical name.
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

// Loads and decodes the TLUT resource paired with fileName, if the index
// found one -- directly if some display list named both sides, or (for a
// flipbook slot whose CI texture couldn't be identified) by falling back to
// this texture's own folder having exactly one confirmed TLUT. Caches
// decoded palettes across calls since one TLUT is typically shared by many
// CI textures in the same object.
const RgbaImage* resolveTlutPalette(const std::string& fileName, const TlutIndex& index,
                                     std::unordered_map<uint64_t, RgbaImage>& paletteCache) {
    std::optional<uint64_t> tlutHash;

    auto ciIt = index.ciHashToTlutHash.find(crc64(fileName));
    if (ciIt != index.ciHashToTlutHash.end()) {
        tlutHash = ciIt->second;
    } else if (auto named = findNamedTlutMatch(fileName, index); named.has_value()) {
        tlutHash = named;
    } else {
        std::string folder = std::filesystem::path(fileName).parent_path().string();
        auto folderIt = index.folderToConfirmedTluts.find(folder);
        if (folderIt != index.folderToConfirmedTluts.end() && folderIt->second.size() == 1) {
            tlutHash = folderIt->second.front();
        } else {
            // Ambiguous folder (0 or 2+ confirmed TLUTs): try the eye/mouth
            // flipbook segment this texture's own name suggests.
            auto segmentIt = index.folderToSegmentTluts.find(folder);
            if (segmentIt != index.folderToSegmentTluts.end()) {
                std::string baseName = std::filesystem::path(fileName).filename().string();
                std::transform(baseName.begin(), baseName.end(), baseName.begin(),
                                [](unsigned char c) { return std::tolower(c); });

                auto eyeIt = segmentIt->second.find(kEyeFlipbookSegment);
                auto mouthIt = segmentIt->second.find(kMouthFlipbookSegment);
                if (baseName.find("eye") != std::string::npos && eyeIt != segmentIt->second.end()) {
                    tlutHash = eyeIt->second;
                } else if (baseName.find("mouth") != std::string::npos && mouthIt != segmentIt->second.end()) {
                    tlutHash = mouthIt->second;
                }
            }
        }
    }

    if (!tlutHash.has_value()) {
        return nullptr;
    }

    auto cached = paletteCache.find(*tlutHash);
    if (cached != paletteCache.end()) {
        return &cached->second;
    }

    auto locationIt = index.hashToLocation.find(*tlutHash);
    if (locationIt == index.hashToLocation.end()) {
        return nullptr;
    }

    Arc tlutArc(locationIt->second.archivePath);
    auto tlutBytes = tlutArc.readFile(locationIt->second.name);
    tlutArc.close();
    if (!tlutBytes.has_value()) {
        return nullptr;
    }

    Texture tlutTexture;
    tlutTexture.open(*tlutBytes);
    if (!tlutTexture.isValid()) {
        return nullptr;
    }

    RgbaImage palette = decodeN64Texture(tlutTexture.texData(), tlutTexture.textureType(), tlutTexture.width(),
                                          tlutTexture.height());
    return &paletteCache.emplace(*tlutHash, std::move(palette)).first->second;
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
                if (texture.isValid()) {
                    RgbaImage decoded =
                        decodeN64Texture(texture.texData(), texture.textureType(), texture.width(), texture.height());
                    if (applyTlut && texture.isPalette()) {
                        const RgbaImage* palette = resolveTlutPalette(fileName, tlutIndex, tlutPaletteCache);
                        if (palette != nullptr) {
                            applyTlutPalette(decoded, *palette);
                        }
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
