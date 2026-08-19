#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/sha256.h"
#include "core/types/background.h"
#include "core/types/sequence.h"
#include "core/types/texture.h"
#include "games/bk64_conventions.h"

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

void testSha256() {
    // Known SHA-256 test vectors (FIPS 180-4 / RFC examples).
    check(bitdeck::sha256Hex({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256(\"\") matches known vector");

    std::vector<uint8_t> abc = {'a', 'b', 'c'};
    check(bitdeck::sha256Hex(abc) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256(\"abc\") matches known vector");
}

void testTextureHeaderRoundTrip() {
    using namespace bitdeck;

    Texture tex(TextureType::RGBA32bpp, 4, 2, std::vector<uint8_t>(4 * 2 * 4, 0x42));
    std::vector<uint8_t> bytes = tex.build();
    check(bytes.size() == Resource::kHeaderSize + 4 + 4 + 4 + 4 + 4 * 2 * 4,
          "Texture (Version0) build() size matches header + payload");

    Texture readBack;
    readBack.open(bytes);
    check(readBack.isValid(), "Texture round-trip: isValid()");
    check(readBack.textureType() == TextureType::RGBA32bpp, "Texture round-trip: textureType");
    check(readBack.width() == 4 && readBack.height() == 2, "Texture round-trip: dimensions");
    check(readBack.texData().size() == 4 * 2 * 4, "Texture round-trip: texData size");
    check(readBack.texData() == tex.texData(), "Texture round-trip: texData contents");
}

void testTextureExtendedHeaderRoundTrip() {
    using namespace bitdeck;

    Texture tex(TextureType::RGBA32bpp, 8, 8, std::vector<uint8_t>(8 * 8 * 4, 0x11));
    tex.setTextureScale(1.5, 2.5);
    std::vector<uint8_t> bytes = tex.build();

    Texture readBack;
    readBack.open(bytes);
    check(readBack.isValid(), "Texture (Version1) round-trip: isValid()");
    check(readBack.gameVersion() == Version::Version1, "Texture (Version1) round-trip: gameVersion");
    check(std::abs(readBack.textureHByteScale() - 1.5) < 1e-5, "Texture (Version1) round-trip: hByteScale");
    check(std::abs(readBack.textureVPixelScale() - 2.5) < 1e-5, "Texture (Version1) round-trip: vPixelScale");
}

void testRgba16Codec() {
    using namespace bitdeck;

    RgbaImage source = RgbaImage::makeChannelImage(2, 2, 4);
    source.setRgba(0, 0, 248, 0, 0, 255);   // pure red, opaque
    source.setRgba(1, 0, 0, 248, 0, 255);   // pure green, opaque
    source.setRgba(0, 1, 0, 0, 248, 0);     // pure blue, transparent
    source.setRgba(1, 1, 255, 255, 255, 255); // white, opaque

    std::vector<uint8_t> texData = encodeN64Texture(source, TextureType::RGBA16bpp);
    check(texData.size() == static_cast<size_t>(textureBufferSize(TextureType::RGBA16bpp, 2, 2)),
          "RGBA16bpp encode: buffer size");

    RgbaImage decoded = decodeN64Texture(texData, TextureType::RGBA16bpp, 2, 2);
    check(decoded.r(0, 0) >= 248 && decoded.g(0, 0) == 0 && decoded.b(0, 0) == 0 && decoded.a(0, 0) == 255,
          "RGBA16bpp round-trip: red pixel");
    check(decoded.r(0, 1) == 0 && decoded.g(0, 1) == 0 && decoded.b(0, 1) >= 248 && decoded.a(0, 1) == 0,
          "RGBA16bpp round-trip: transparent blue pixel");
}

void testPalette4Codec() {
    using namespace bitdeck;

    RgbaImage source = RgbaImage::makePaletteImage(4, 1);
    source.setIndex(0, 0, 1);
    source.setIndex(1, 0, 2);
    source.setIndex(2, 0, 3);
    source.setIndex(3, 0, 4);

    std::vector<uint8_t> texData = encodeN64Texture(source, TextureType::Palette4bpp);
    check(texData.size() == 2, "Palette4bpp encode: 4 pixels pack into 2 bytes");
    check(texData[0] == 0x12 && texData[1] == 0x34, "Palette4bpp encode: nibble packing");

    RgbaImage decoded = decodeN64Texture(texData, TextureType::Palette4bpp, 4, 1);
    check(decoded.index(0, 0) == 1 && decoded.index(1, 0) == 2 && decoded.index(2, 0) == 3 &&
              decoded.index(3, 0) == 4,
          "Palette4bpp round-trip: indices");
}

void testBackgroundRoundTrip() {
    using namespace bitdeck;

    std::vector<uint8_t> fakeJpeg = {0xFF, 0xD8, 0xFF, 0xD9};
    Background bg(fakeJpeg);
    std::vector<uint8_t> bytes = bg.build();

    Background readBack;
    readBack.open(bytes);
    check(readBack.isValid(), "Background round-trip: isValid()");
    check(readBack.texData() == fakeJpeg, "Background round-trip: texData contents");
}

void testSequenceFromSeqFile() {
    using namespace bitdeck;

    std::vector<uint8_t> rawSeq = {0x01, 0x02, 0x03};
    std::string meta = "my_sequence\n0x0A\nfanfare\n";
    Sequence seq = Sequence::fromSeqFile(rawSeq, meta);
    check(seq.path() == "my_sequence_fanfare", "Sequence::fromSeqFile: path composed from name+type");

    std::vector<uint8_t> bytes = seq.build();
    check(bytes.size() == Resource::kHeaderSize + 4 + 3 + 1 + 1 + 1 + 4 + 1, "Sequence build() size");
}

void testBk64ParseSpriteTilePositions() {
    using namespace bitdeck;

    // Build a minimal fake resource: 0x40 header (zeroed) + 10 bytes filler
    // + 4 bytes filler, then positionCount=2, two (i16,i16) positions,
    // frameCount=1, one u16 count=2.
    std::vector<uint8_t> data(0x40 + 10 + 4, 0);
    auto pushU32 = [&data](uint32_t v) {
        data.push_back(static_cast<uint8_t>(v & 0xFF));
        data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    auto pushI16 = [&data](int16_t v) {
        data.push_back(static_cast<uint8_t>(v & 0xFF));
        data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto pushU16 = [&data](uint16_t v) {
        data.push_back(static_cast<uint8_t>(v & 0xFF));
        data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };

    pushU32(2);       // positionCount
    pushI16(10); pushI16(20);
    pushI16(30); pushI16(40);
    pushU32(1);        // frameCount
    pushU16(2);         // frame 0 has 2 chunks

    auto tiles = parseSpriteTilePositions(data);
    check(tiles.has_value(), "parseSpriteTilePositions: parses valid table");
    if (tiles) {
        check(tiles->at({0, 0}) == std::make_pair(static_cast<int16_t>(10), static_cast<int16_t>(20)),
              "parseSpriteTilePositions: frame0/chunk0 position");
        check(tiles->at({0, 1}) == std::make_pair(static_cast<int16_t>(30), static_cast<int16_t>(40)),
              "parseSpriteTilePositions: frame0/chunk1 position");
    }
}

void testBk64ResolveAdditiveEntry() {
    using namespace bitdeck;

    Bk64TextureConventions conventions;
    TextureManifestMap manifest;
    manifest["icons/item_16_16"] = TextureManifestEntry{"abc", TextureType::RGBA32bpp, 16, 16, std::nullopt, std::nullopt};

    auto resolved = conventions.resolveAdditiveEntry(manifest, "icons/item_16_16_BLUE");
    check(resolved.has_value() && resolved->kind == TextureEntryKind::Additive,
          "Bk64 resolveAdditiveEntry: color-variant match");

    auto unresolved = conventions.resolveAdditiveEntry(manifest, "icons/does_not_exist");
    check(!unresolved.has_value(), "Bk64 resolveAdditiveEntry: no match returns nullopt");
}

} // namespace

int main() {
    testSha256();
    testTextureHeaderRoundTrip();
    testTextureExtendedHeaderRoundTrip();
    testRgba16Codec();
    testPalette4Codec();
    testBackgroundRoundTrip();
    testSequenceFromSeqFile();
    testBk64ParseSpriteTilePositions();
    testBk64ResolveAdditiveEntry();

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
