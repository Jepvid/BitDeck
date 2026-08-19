#include "texture_extraction.h"

#include <fstream>

#include "../archive/arc.h"
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

} // namespace

TextureManifestMap extractTexturesToFolder(const std::vector<std::string>& archivePaths,
                                            const std::filesystem::path& targetDir, TaskProgress& progress) {
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
