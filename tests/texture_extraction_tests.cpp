#include <QCoreApplication>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "app/texture_extraction.h"
#include "app/texture_manifest_json.h"
#include "archive/arc.h"
#include "core/image_codec.h"
#include "core/types/texture.h"

namespace {

int g_failures = 0;

void check(bool condition, const char* description) {
    if (condition) {
        std::printf("[PASS] %s\n", description);
    } else {
        std::printf("[FAIL] %s\n", description);
        ++g_failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    using namespace bitdeck;

    // Build a tiny .otr containing one RGBA32bpp texture at a nested path,
    // matching what a real game archive's directory layout looks like.
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "bitdeck_extraction_test";
    std::filesystem::create_directories(dir);
    std::filesystem::path archivePath = dir / "source.otr";
    std::filesystem::remove(archivePath);

    RgbaImage source = RgbaImage::makeChannelImage(2, 2, 4);
    source.setRgba(0, 0, 10, 20, 30, 255);
    source.setRgba(1, 0, 40, 50, 60, 255);
    source.setRgba(0, 1, 70, 80, 90, 255);
    source.setRgba(1, 1, 100, 110, 120, 255);
    Texture texture(TextureType::RGBA32bpp, 2, 2, encodeN64Texture(source, TextureType::RGBA32bpp));

    {
        Arc arc(archivePath.string());
        arc.addFile("textures/icons/gem", texture.build(), false);
        arc.close();
    }

    std::filesystem::path targetDir = dir / "extracted";
    TaskProgress progress;
    TextureManifestMap manifest =
        extractTexturesToFolder({archivePath.string()}, targetDir, progress);

    check(manifest.size() == 1, "extractTexturesToFolder: manifest has exactly 1 entry");
    check(manifest.count("textures/icons/gem") == 1, "extractTexturesToFolder: entry keyed by archive path");
    if (manifest.count("textures/icons/gem") == 1) {
        const auto& entry = manifest.at("textures/icons/gem");
        check(entry.textureType == TextureType::RGBA32bpp, "extractTexturesToFolder: texture type recorded");
        check(entry.textureWidth == 2 && entry.textureHeight == 2, "extractTexturesToFolder: dimensions recorded");
    }

    std::filesystem::path pngPath = targetDir / "textures" / "icons" / "gem.png";
    check(std::filesystem::exists(pngPath), "extractTexturesToFolder: PNG written at the nested archive path");
    if (std::filesystem::exists(pngPath)) {
        std::ifstream file(pngPath, std::ios::binary);
        std::vector<uint8_t> pngBytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        RgbaImage decoded = decodePng(pngBytes);
        check(decoded.r(0, 0) == 10 && decoded.g(0, 0) == 20 && decoded.b(0, 0) == 30,
              "extractTexturesToFolder: extracted PNG pixel data matches the source");
    }

    std::filesystem::path manifestPath = targetDir / "manifest.json";
    check(std::filesystem::exists(manifestPath), "extractTexturesToFolder: manifest.json written");
    if (std::filesystem::exists(manifestPath)) {
        std::ifstream file(manifestPath, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        TextureManifestMap reparsed = parseManifestJson(QByteArray::fromStdString(text));
        check(reparsed.size() == 1 && reparsed.count("textures/icons/gem") == 1,
              "manifest.json: round-trips through parseManifestJson");
    }

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
