#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" {
#include <jpeglib.h>
}

#include "core/image_codec.h"

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

// Solid-color JPEG fixture, built with libjpeg directly (only decode lives
// in the shipped codec, since Retro never encodes JPEGs).
std::vector<uint8_t> encodeSolidJpegForTest(int width, int height, uint8_t r, uint8_t g, uint8_t b) {
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

void testPngRgbaRoundTrip() {
    using namespace bitdeck;

    RgbaImage image = RgbaImage::makeChannelImage(3, 2, 4);
    image.setRgba(0, 0, 255, 0, 0, 255);
    image.setRgba(1, 0, 0, 255, 0, 128);
    image.setRgba(2, 0, 0, 0, 255, 0);
    image.setRgba(0, 1, 10, 20, 30, 255);
    image.setRgba(1, 1, 40, 50, 60, 255);
    image.setRgba(2, 1, 70, 80, 90, 255);

    std::vector<uint8_t> pngBytes = encodePng(image);
    check(!pngBytes.empty(), "PNG RGBA: encode produced non-empty bytes");
    check(pngBytes.size() >= 8 && pngBytes[0] == 0x89 && pngBytes[1] == 'P' && pngBytes[2] == 'N' &&
              pngBytes[3] == 'G',
          "PNG RGBA: output has the PNG magic header");

    RgbaImage decoded = decodePng(pngBytes);
    check(decoded.width == 3 && decoded.height == 2, "PNG RGBA: decoded dimensions match");
    check(decoded.numChannels == 4, "PNG RGBA: decoded as 4 channels");
    bool pixelsMatch = decoded.r(0, 0) == 255 && decoded.g(0, 0) == 0 && decoded.b(0, 0) == 0 &&
                        decoded.a(0, 0) == 255 && decoded.a(1, 0) == 128 && decoded.r(0, 1) == 10 &&
                        decoded.g(2, 1) == 80;
    check(pixelsMatch, "PNG RGBA: round-tripped pixels match exactly (lossless)");
}

void testPngPaletteRoundTrip() {
    using namespace bitdeck;

    RgbaImage image = RgbaImage::makePaletteImage(2, 2);
    image.setPaletteEntry(0, RgbaColor{255, 0, 0, 255});
    image.setPaletteEntry(1, RgbaColor{0, 255, 0, 128});
    image.setIndex(0, 0, 0);
    image.setIndex(1, 0, 1);
    image.setIndex(0, 1, 1);
    image.setIndex(1, 1, 0);

    std::vector<uint8_t> pngBytes = encodePng(image);
    RgbaImage decoded = decodePng(pngBytes);

    check(decoded.withPalette, "PNG palette: decoded as an indexed image");
    check(decoded.index(0, 0) == 0 && decoded.index(1, 0) == 1 && decoded.index(0, 1) == 1 &&
              decoded.index(1, 1) == 0,
          "PNG palette: round-tripped indices match exactly");
    check(decoded.palette[0].r == 255 && decoded.palette[0].g == 0 && decoded.palette[0].a == 255,
          "PNG palette: entry 0 color+alpha round-trips");
    check(decoded.palette[1].g == 255 && decoded.palette[1].a == 128,
          "PNG palette: entry 1 alpha (tRNS) round-trips");
}

void testJpegDecode() {
    using namespace bitdeck;

    std::vector<uint8_t> jpegBytes = encodeSolidJpegForTest(8, 8, 200, 40, 10);
    check(!jpegBytes.empty(), "JPEG fixture: encode produced non-empty bytes");

    RgbaImage decoded = decodeJpeg(jpegBytes);
    check(decoded.width == 8 && decoded.height == 8, "JPEG decode: dimensions match");
    check(decoded.numChannels == 4, "JPEG decode: normalized to 4 channels");

    // Lossy compression, so allow some tolerance rather than an exact match.
    auto close = [](int a, int b) { return std::abs(a - b) <= 8; };
    bool colorClose = close(decoded.r(4, 4), 200) && close(decoded.g(4, 4), 40) && close(decoded.b(4, 4), 10);
    check(colorClose, "JPEG decode: center pixel color is close to the source");
    check(decoded.a(4, 4) == 255, "JPEG decode: alpha forced to opaque");
}

} // namespace

int main() {
    testPngRgbaRoundTrip();
    testPngPaletteRoundTrip();
    testJpegDecode();

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
