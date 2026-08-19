#include <QCoreApplication>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

#include "app/archive_generator.h"
#include "archive/arc.h"
#include "core/image_codec.h"
#include "core/resource.h"
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

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    file << text;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    using namespace bitdeck;

    std::filesystem::path dir = std::filesystem::temp_directory_path() / "bitdeck_generator_test";
    std::filesystem::create_directories(dir);

    // A raw passthrough file.
    std::filesystem::path rawFile = dir / "readme.txt";
    writeTextFile(rawFile, "hello from a staged file");

    // A .seq + .meta pair.
    std::filesystem::path seqFile = dir / "song.seq";
    writeFile(seqFile, {0x01, 0x02, 0x03, 0x04});
    std::filesystem::path metaFile = dir / "song.meta";
    writeTextFile(metaFile, "my_song\n0x02\nfanfare\n");

    // A 2x2 PNG matching a texture manifest entry exactly (no scaling needed).
    RgbaImage source = RgbaImage::makeChannelImage(2, 2, 4);
    source.setRgba(0, 0, 255, 0, 0, 255);
    source.setRgba(1, 0, 0, 255, 0, 255);
    source.setRgba(0, 1, 0, 0, 255, 255);
    source.setRgba(1, 1, 255, 255, 255, 255);
    std::filesystem::path pngFile = dir / "icon.png";
    writeFile(pngFile, encodePng(source));

    TextureManifestEntry manifestEntry;
    manifestEntry.textureType = TextureType::RGBA32bpp;
    manifestEntry.textureWidth = 2;
    manifestEntry.textureHeight = 2;

    // A 4x4 HD replacement for the same 2x2-declared texture: exercises the
    // scale-factor path (LOAD_AS_RAW + textureHByteScale/VPixelScale) that
    // makes HD texture packs work.
    RgbaImage hdSource = RgbaImage::makeChannelImage(4, 4, 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            hdSource.setRgba(x, y, 10, 20, 30, 255);
        }
    }
    std::filesystem::path hdPngFile = dir / "hd_icon.png";
    writeFile(hdPngFile, encodePng(hdSource));

    TextureManifestEntry hdManifestEntry;
    hdManifestEntry.textureType = TextureType::RGBA32bpp;
    hdManifestEntry.textureWidth = 2;
    hdManifestEntry.textureHeight = 2;
    hdManifestEntry.targetName = "textures/hd/icon";

    std::map<std::string, StageEntry> entries;
    entries["custom/files"] = CustomStageEntry{{rawFile}};
    entries["custom/music"] = CustomSequencesEntry{{{seqFile, metaFile}}};
    entries["textures/icons"] = CustomTexturesEntry{{{pngFile, manifestEntry}}};
    entries["textures/hd"] = CustomTexturesEntry{{{hdPngFile, hdManifestEntry}}};

    int totalExpected = 0;
    for (const auto& [key, entry] : entries) {
        totalExpected += static_cast<int>(stageEntryFileCount(entry));
    }
    check(totalExpected == 4, "test setup: 4 files staged across the entry kinds");

    std::filesystem::path outputPath = dir / "generated.o2r";
    std::filesystem::remove(outputPath);

    TaskProgress progress;
    int progressCalls = 0;
    int lastProcessed = 0;
    QObject::connect(&progress, &TaskProgress::progress, [&](int processed, int total) {
        ++progressCalls;
        lastProcessed = processed;
        check(total == totalExpected, "generateArchive: progress total matches file count");
    });

    generateArchive(entries, outputPath.string(), /*compress=*/true, /*prependAlt=*/true, progress);

    check(progressCalls == 4, "generateArchive: progress reported once per file");
    check(lastProcessed == 4, "generateArchive: final progress reaches the total");
    check(std::filesystem::exists(outputPath), "generateArchive: output archive file was created");

    bitdeck::Arc readBack(outputPath.string());
    std::map<std::string, std::vector<uint8_t>> found;
    readBack.listItems(
        [&](const std::string& name, const std::vector<uint8_t>& data) { found[name] = data; });
    readBack.close();

    check(found.size() == 4, "generated archive: contains exactly 4 entries");

    auto rawIt = found.find("custom/files/readme.txt");
    check(rawIt != found.end(), "generated archive: raw file present at custom/files/readme.txt");
    if (rawIt != found.end()) {
        std::string content(rawIt->second.begin(), rawIt->second.end());
        check(content == "hello from a staged file", "generated archive: raw file content matches exactly");
    }

    auto seqIt = found.find("custom/music/my_song_fanfare");
    check(seqIt != found.end(), "generated archive: sequence present at custom/music/my_song_fanfare");
    if (seqIt != found.end()) {
        // Sequence has no readResourceData (write-only type), so just sanity check the payload exists.
        check(seqIt->second.size() > Resource::kHeaderSize, "generated archive: sequence resource has a payload");
    }

    // prependAlt was on, so even the fallback (key + stem) naming gets "alt/".
    auto texIt = found.find("alt/textures/icons/icon");
    check(texIt != found.end(), "generated archive: texture present at alt/textures/icons/icon (prependAlt)");
    if (texIt != found.end()) {
        Texture readTexture;
        readTexture.open(texIt->second);
        check(readTexture.isValid(), "generated archive: texture resource is valid");
        check(readTexture.textureType() == TextureType::RGBA32bpp, "generated archive: texture type round-trips");
        check(readTexture.width() == 2 && readTexture.height() == 2, "generated archive: texture dimensions round-trip");
        check(readTexture.gameVersion() == Version::Version0,
              "generated archive: exact-size texture uses the base header (no scale needed)");
        RgbaImage decoded = decodeN64Texture(readTexture.texData(), readTexture.textureType(), readTexture.width(),
                                              readTexture.height());
        check(decoded.r(0, 0) == 255 && decoded.g(0, 0) == 0 && decoded.b(0, 0) == 0,
              "generated archive: texture pixel data round-trips");
    }

    // The HD (4x4) replacement for a 2x2-declared texture: proves the
    // scale-factor path that makes HD texture packs actually work.
    auto hdIt = found.find("alt/textures/hd/icon");
    check(hdIt != found.end(), "generated archive: HD texture present at alt/textures/hd/icon");
    if (hdIt != found.end()) {
        Texture hdTexture;
        hdTexture.open(hdIt->second);
        check(hdTexture.isValid(), "generated archive: HD texture resource is valid");
        check(hdTexture.width() == 4 && hdTexture.height() == 4,
              "generated archive: HD texture keeps the replacement's real 4x4 dimensions");
        check(hdTexture.gameVersion() == Version::Version1,
              "generated archive: HD texture uses the extended (Version1) header");
        check(hdTexture.textureFlags() == Texture::kLoadAsRaw, "generated archive: HD texture sets LOAD_AS_RAW");
        check(std::abs(hdTexture.textureHByteScale() - 2.0) < 1e-5 &&
                  std::abs(hdTexture.textureVPixelScale() - 2.0) < 1e-5,
              "generated archive: HD texture's scale factor is exactly 2x (4x4 replacing 2x2)");
        RgbaImage decoded = decodeN64Texture(hdTexture.texData(), hdTexture.textureType(), hdTexture.width(),
                                              hdTexture.height());
        check(decoded.r(0, 0) == 10 && decoded.g(0, 0) == 20 && decoded.b(0, 0) == 30,
              "generated archive: HD texture pixel data round-trips at full 4x4 resolution");
    }

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
