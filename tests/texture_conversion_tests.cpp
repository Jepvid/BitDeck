#include <cmath>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include <jpeglib.h>
}

#include "core/image_codec.h"
#include "core/texture_conversion.h"

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

using namespace bitdeck;

std::vector<uint8_t> solidPng(int width, int height, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    RgbaImage image = RgbaImage::makeChannelImage(width, height, 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.setRgba(x, y, r, g, b, a);
        }
    }
    return encodePng(image);
}

std::vector<uint8_t> solidPalettePng(int width, int height, uint8_t index, RgbaColor color) {
    RgbaImage image = RgbaImage::makePaletteImage(width, height);
    image.setPaletteEntry(index, color);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.setIndex(x, y, index);
        }
    }
    return encodePng(image);
}

void testExactMultiple() {
    RgbaImage image8x8 = RgbaImage::makeChannelImage(8, 8, 4);
    check(exactMultiple(image8x8, 4, 4) == std::optional<int>(2), "exactMultiple: 8x8 is 2x a 4x4 tile");
    check(exactMultiple(image8x8, 8, 8) == std::optional<int>(1), "exactMultiple: 8x8 is 1x itself");
    check(!exactMultiple(image8x8, 3, 3).has_value(), "exactMultiple: 8x8 is not a multiple of 3x3");
    check(!exactMultiple(image8x8, 0, 4).has_value(), "exactMultiple: zero width rejected");

    RgbaImage image6x8 = RgbaImage::makeChannelImage(6, 8, 4);
    check(!exactMultiple(image6x8, 4, 4).has_value(), "exactMultiple: mismatched w/h ratio rejected (6/4 != 8/4)");
}

void testPadCanvas() {
    RgbaImage image = RgbaImage::makeChannelImage(2, 2, 4);
    image.setRgba(0, 0, 1, 2, 3, 255);

    RgbaImage same = padCanvas(image, 2, 2);
    check(same.width == 2 && same.height == 2 && same.r(0, 0) == 1, "padCanvas: same size returns image unchanged");

    RgbaImage padded = padCanvas(image, 4, 4);
    check(padded.width == 4 && padded.height == 4, "padCanvas: pads to the requested size");
    check(padded.r(0, 0) == 1 && padded.g(0, 0) == 2 && padded.b(0, 0) == 3,
          "padCanvas: source pixels copied at (0,0)");
    check(padded.r(3, 3) == 0 && padded.a(3, 3) == 0, "padCanvas: padding is transparent");
}

void testExactSizeReplacement() {
    TextureManifestEntry entry;
    entry.textureType = TextureType::RGBA32bpp;
    entry.textureWidth = 2;
    entry.textureHeight = 2;

    auto texture = convertPngToTexture(solidPng(2, 2, 10, 20, 30), entry);
    check(texture.has_value(), "exact-size: conversion succeeds");
    if (texture) {
        check(texture->gameVersion() == Version::Version0, "exact-size: no scale needed, base header");
        check(texture->width() == 2 && texture->height() == 2, "exact-size: dimensions match");
        check(texture->textureType() == TextureType::RGBA32bpp, "exact-size: type unchanged");
    }
}

void testHdResizeAndTypeReassignment() {
    // Manifest declares Grayscale8bpp at 2x2; replacement is a 4x4 RGBA PNG.
    // This exercises Retro's own quirk: the payload gets encoded as
    // RGBA32bpp (since a non-palette resize always forces that for the
    // actual pixel data), but the *declared* textureType field is set back
    // to the manifest's original type afterward -- LOAD_AS_RAW is what
    // tells the game engine the payload doesn't match the declared type's
    // normal pixel layout. Replicated exactly, not "fixed".
    TextureManifestEntry entry;
    entry.textureType = TextureType::Grayscale8bpp;
    entry.textureWidth = 2;
    entry.textureHeight = 2;

    auto texture = convertPngToTexture(solidPng(4, 4, 50, 60, 70), entry);
    check(texture.has_value(), "HD resize: conversion succeeds");
    if (texture) {
        check(texture->width() == 4 && texture->height() == 4, "HD resize: keeps the replacement's real dimensions");
        check(texture->gameVersion() == Version::Version1, "HD resize: extended header used");
        check(texture->textureFlags() == Texture::kLoadAsRaw, "HD resize: LOAD_AS_RAW set");
        // hByteScale = (4/2) * (RGBA32bpp.mult / Grayscale8bpp.mult) = 2 * (4/1) = 8
        // vPixelScale = 4/2 = 2 (pixel count ratio, no multiplier involved)
        check(std::abs(texture->textureHByteScale() - 8.0) < 1e-5,
              "HD resize: hByteScale accounts for the byte-width difference between the encoded and declared types");
        check(std::abs(texture->textureVPixelScale() - 2.0) < 1e-5, "HD resize: vPixelScale is a plain 2x pixel ratio");
        check(texture->textureType() == TextureType::Grayscale8bpp,
              "HD resize: declared type reassigned back to the manifest's original (Retro's quirk)");
        // The payload itself must still be readable back out as RGBA32 --
        // decoding using the *declared* Grayscale8bpp type would misread
        // it, which is exactly what LOAD_AS_RAW exists to signal downstream.
        RgbaImage decoded = decodeN64Texture(texture->texData(), TextureType::RGBA32bpp, texture->width(), texture->height());
        check(decoded.r(0, 0) == 50 && decoded.g(0, 0) == 60 && decoded.b(0, 0) == 70,
              "HD resize: payload bytes are genuinely RGBA32 pixel data");
    }
}

void testAdditiveFontGlyphTiling() {
    TextureManifestEntry entry;
    entry.textureType = TextureType::RGBA32bpp;
    entry.textureWidth = 4;
    entry.textureHeight = 4;
    entry.kind = TextureEntryKind::AdditiveFontGlyph;

    // 8x8 = exactly 2x a 4x4 glyph tile.
    auto texture = convertPngToTexture(solidPng(8, 8, 5, 6, 7), entry);
    check(texture.has_value(), "additive font glyph: exact tile multiple succeeds");
    if (texture) {
        check(texture->textureType() == TextureType::RGBA32bpp, "additive font glyph: always RGBA32bpp");
        check(std::abs(texture->textureHByteScale() - 2.0) < 1e-5 && std::abs(texture->textureVPixelScale() - 2.0) < 1e-5,
              "additive font glyph: scale is the tile multiple (2x)");
        // alignedWidth = (4+3)&~3 = 4, so expected canvas is 2*4 x 2*4 = 8x8 (no padding needed here).
        check(texture->width() == 8 && texture->height() == 8, "additive font glyph: canvas matches the aligned size");
    }

    // 9x9 is not an integer multiple of the 4x4 glyph -- rejected.
    auto rejected = convertPngToTexture(solidPng(9, 9, 1, 2, 3), entry);
    check(!rejected.has_value(), "additive font glyph: non-multiple size is rejected");
}

void testPaletteEntry() {
    TextureManifestEntry entry;
    entry.textureType = TextureType::Palette8bpp;
    entry.textureWidth = 2;
    entry.textureHeight = 2;

    auto indexedTexture = convertPngToTexture(solidPalettePng(2, 2, 7, RgbaColor{200, 100, 50, 255}), entry);
    check(indexedTexture.has_value(), "palette entry: genuinely indexed PNG accepted");
    if (indexedTexture) {
        check(indexedTexture->textureType() == TextureType::Palette8bpp, "palette entry: stays Palette8bpp");
        check(indexedTexture->gameVersion() == Version::Version0, "palette entry: exact size, no scale header");
    }

    auto rejected = convertPngToTexture(solidPng(2, 2, 200, 100, 50), entry);
    check(!rejected.has_value(), "palette entry: non-indexed PNG at original size is rejected");
}

// Solid-color JPEG fixture via libjpeg directly (the shipped codec only
// decodes JPEG, since Retro never encodes one).
std::vector<uint8_t> solidJpeg(int width, int height, uint8_t r, uint8_t g, uint8_t b) {
    jpeg_compress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned char* outBuffer = nullptr;
    unsigned long outSize = 0;
    jpeg_mem_dest(&cinfo, &outBuffer, &outSize);

    cinfo.image_width = static_cast<JDIMENSION>(width);
    cinfo.image_height = static_cast<JDIMENSION>(height);
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 100, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    std::vector<uint8_t> row(static_cast<size_t>(width) * 3);
    for (int x = 0; x < width; ++x) {
        row[static_cast<size_t>(x) * 3 + 0] = r;
        row[static_cast<size_t>(x) * 3 + 1] = g;
        row[static_cast<size_t>(x) * 3 + 2] = b;
    }
    JSAMPROW rowPointer[1] = {row.data()};
    while (cinfo.next_scanline < cinfo.image_height) {
        jpeg_write_scanlines(&cinfo, rowPointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    std::vector<uint8_t> bytes(outBuffer, outBuffer + outSize);
    jpeg_destroy_compress(&cinfo);
    free(outBuffer);
    return bytes;
}

void testJpegConversion() {
    // Backgrounds are always resized relative to a (smaller) N64-native
    // entry; the formula divides by RGBA16bpp's multiplier specifically
    // (not the entry's own type), matching processJPEG exactly.
    TextureManifestEntry entry;
    entry.textureType = TextureType::JPEG32bpp;
    entry.textureWidth = 4;
    entry.textureHeight = 2;

    auto texture = convertImageToTexture(solidJpeg(8, 4, 90, 100, 110), entry);
    check(texture.has_value(), "JPEG conversion: succeeds");
    if (texture) {
        check(texture->textureType() == TextureType::RGBA32bpp, "JPEG conversion: always RGBA32bpp");
        check(texture->gameVersion() == Version::Version1, "JPEG conversion: always uses the extended header");
        check(texture->textureFlags() == Texture::kLoadAsRaw, "JPEG conversion: LOAD_AS_RAW set");
        check(texture->width() == 8 && texture->height() == 4, "JPEG conversion: keeps the source's real dimensions");
        // hByteScale = (8/4) * (RGBA32bpp.mult / RGBA16bpp.mult) = 2 * (4/2) = 4
        // vPixelScale = 4/2 = 2
        check(std::abs(texture->textureHByteScale() - 4.0) < 1e-5,
              "JPEG conversion: hByteScale uses RGBA16bpp's multiplier, not the entry's");
        check(std::abs(texture->textureVPixelScale() - 2.0) < 1e-5, "JPEG conversion: vPixelScale is a plain size ratio");
    }
}

} // namespace

int main() {
    testExactMultiple();
    testPadCanvas();
    testExactSizeReplacement();
    testHdResizeAndTypeReassignment();
    testAdditiveFontGlyphTiling();
    testPaletteEntry();
    testJpegConversion();

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
