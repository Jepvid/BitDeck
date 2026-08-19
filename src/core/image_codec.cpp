#include "image_codec.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include <png.h>

extern "C" {
#include <jpeglib.h>
}

namespace bitdeck {

namespace {

// -- PNG --------------------------------------------------------------

struct PngMemoryReader {
    const uint8_t* data;
    size_t size;
    size_t offset = 0;
};

struct PngMemoryWriter {
    std::vector<uint8_t> buffer;
};

void pngReadCallback(png_structp png, png_bytep outBytes, png_size_t byteCount) {
    auto* reader = static_cast<PngMemoryReader*>(png_get_io_ptr(png));
    if (reader->offset + byteCount > reader->size) {
        png_error(png, "Unexpected end of PNG data");
    }
    std::memcpy(outBytes, reader->data + reader->offset, byteCount);
    reader->offset += byteCount;
}

void pngWriteCallback(png_structp png, png_bytep data, png_size_t length) {
    auto* writer = static_cast<PngMemoryWriter*>(png_get_io_ptr(png));
    writer->buffer.insert(writer->buffer.end(), data, data + length);
}

void pngFlushCallback(png_structp /*png*/) {}

[[noreturn]] void pngErrorCallback(png_structp /*png*/, png_const_charp message) {
    throw std::runtime_error(std::string("PNG error: ") + message);
}

void pngWarningCallback(png_structp /*png*/, png_const_charp /*message*/) {}

} // namespace

RgbaImage decodePng(const std::vector<uint8_t>& data) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, pngErrorCallback, pngWarningCallback);
    if (png == nullptr) {
        throw std::runtime_error("png_create_read_struct failed");
    }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        throw std::runtime_error("png_create_info_struct failed");
    }

    RgbaImage result;
    try {
        PngMemoryReader reader{data.data(), data.size()};
        png_set_read_fn(png, &reader, pngReadCallback);
        png_read_info(png, info);

        png_uint_32 width = 0;
        png_uint_32 height = 0;
        int bitDepth = 0;
        int colorType = 0;
        png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, nullptr, nullptr, nullptr);

        if (colorType == PNG_COLOR_TYPE_PALETTE) {
            if (bitDepth < 8) {
                png_set_packing(png); // unpack sub-byte indices to 1 byte/pixel
            }
            png_read_update_info(png, info);

            result = RgbaImage::makePaletteImage(static_cast<int>(width), static_cast<int>(height));

            png_colorp palette = nullptr;
            int numPaletteEntries = 0;
            png_get_PLTE(png, info, &palette, &numPaletteEntries);

            png_bytep trns = nullptr;
            int numTrns = 0;
            png_get_tRNS(png, info, &trns, &numTrns, nullptr);

            for (int i = 0; i < numPaletteEntries && i < 256; ++i) {
                uint8_t alpha = (i < numTrns) ? trns[i] : 255;
                result.setPaletteEntry(static_cast<uint8_t>(i),
                                        RgbaColor{palette[i].red, palette[i].green, palette[i].blue, alpha});
            }

            size_t rowBytes = png_get_rowbytes(png, info);
            std::vector<uint8_t> rowStorage(rowBytes * height);
            std::vector<png_bytep> rowPointers(height);
            for (png_uint_32 y = 0; y < height; ++y) {
                rowPointers[y] = rowStorage.data() + static_cast<size_t>(y) * rowBytes;
            }
            png_read_image(png, rowPointers.data());

            for (png_uint_32 y = 0; y < height; ++y) {
                for (png_uint_32 x = 0; x < width; ++x) {
                    result.setIndex(static_cast<int>(x), static_cast<int>(y), rowPointers[y][x]);
                }
            }
        } else {
            if (bitDepth == 16) {
                png_set_strip_16(png);
            }
            if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
                png_set_expand_gray_1_2_4_to_8(png);
            }
            bool hasTrns = png_get_valid(png, info, PNG_INFO_tRNS) != 0;
            if (hasTrns) {
                png_set_tRNS_to_alpha(png);
            }
            bool hasAlpha = (colorType & PNG_COLOR_MASK_ALPHA) != 0 || hasTrns;
            if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_gray_to_rgb(png);
            }
            if (!hasAlpha) {
                png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
            }
            png_read_update_info(png, info);

            result = RgbaImage::makeChannelImage(static_cast<int>(width), static_cast<int>(height), 4);
            size_t rowBytes = png_get_rowbytes(png, info);
            std::vector<png_bytep> rowPointers(height);
            for (png_uint_32 y = 0; y < height; ++y) {
                rowPointers[y] = result.channelData.data() + static_cast<size_t>(y) * rowBytes;
            }
            png_read_image(png, rowPointers.data());
        }
    } catch (...) {
        png_destroy_read_struct(&png, &info, nullptr);
        throw;
    }

    png_destroy_read_struct(&png, &info, nullptr);
    return result;
}

std::vector<uint8_t> encodePng(const RgbaImage& image) {
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, pngErrorCallback, pngWarningCallback);
    if (png == nullptr) {
        throw std::runtime_error("png_create_write_struct failed");
    }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_write_struct(&png, nullptr);
        throw std::runtime_error("png_create_info_struct failed");
    }

    PngMemoryWriter writer;
    try {
        png_set_write_fn(png, &writer, pngWriteCallback, pngFlushCallback);

        if (image.withPalette) {
            png_set_IHDR(png, info, static_cast<png_uint_32>(image.width), static_cast<png_uint_32>(image.height), 8,
                         PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                         PNG_FILTER_TYPE_DEFAULT);

            std::vector<png_color> palette(256);
            std::vector<uint8_t> trns(256);
            bool needsTrns = false;
            for (size_t i = 0; i < 256; ++i) {
                const RgbaColor& c = image.palette[i];
                palette[i] = {c.r, c.g, c.b};
                trns[i] = c.a;
                needsTrns = needsTrns || c.a != 255;
            }
            png_set_PLTE(png, info, palette.data(), 256);
            if (needsTrns) {
                png_set_tRNS(png, info, trns.data(), 256, nullptr);
            }

            png_write_info(png, info);

            std::vector<png_bytep> rowPointers(static_cast<size_t>(image.height));
            for (int y = 0; y < image.height; ++y) {
                rowPointers[static_cast<size_t>(y)] =
                    const_cast<uint8_t*>(image.indexData.data() + static_cast<size_t>(y) * static_cast<size_t>(image.width));
            }
            png_write_image(png, rowPointers.data());
        } else {
            int colorType = image.numChannels == 4   ? PNG_COLOR_TYPE_RGBA
                             : image.numChannels == 3 ? PNG_COLOR_TYPE_RGB
                             : image.numChannels == 2 ? PNG_COLOR_TYPE_GRAY_ALPHA
                                                       : PNG_COLOR_TYPE_GRAY;
            png_set_IHDR(png, info, static_cast<png_uint_32>(image.width), static_cast<png_uint_32>(image.height), 8,
                         colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
            png_write_info(png, info);

            size_t rowBytes = static_cast<size_t>(image.width) * static_cast<size_t>(image.numChannels);
            std::vector<png_bytep> rowPointers(static_cast<size_t>(image.height));
            for (int y = 0; y < image.height; ++y) {
                rowPointers[static_cast<size_t>(y)] =
                    const_cast<uint8_t*>(image.channelData.data() + static_cast<size_t>(y) * rowBytes);
            }
            png_write_image(png, rowPointers.data());
        }

        png_write_end(png, info);
    } catch (...) {
        png_destroy_write_struct(&png, &info);
        throw;
    }

    png_destroy_write_struct(&png, &info);
    return writer.buffer;
}

// -- JPEG -------------------------------------------------------------

RgbaImage decodeJpeg(const std::vector<uint8_t>& data) {
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = [](j_common_ptr info) {
        char buffer[JMSG_LENGTH_MAX];
        (*info->err->format_message)(info, buffer);
        throw std::runtime_error(std::string("JPEG error: ") + buffer);
    };

    jpeg_create_decompress(&cinfo);
    try {
        jpeg_mem_src(&cinfo, data.data(), static_cast<unsigned long>(data.size()));
        jpeg_read_header(&cinfo, TRUE);
        cinfo.out_color_space = JCS_RGB;
        jpeg_start_decompress(&cinfo);

        int width = static_cast<int>(cinfo.output_width);
        int height = static_cast<int>(cinfo.output_height);
        int channels = cinfo.output_components;

        RgbaImage result = RgbaImage::makeChannelImage(width, height, 4);
        std::vector<uint8_t> rowBuffer(static_cast<size_t>(width) * static_cast<size_t>(channels));
        JSAMPROW rowPointer[1] = {rowBuffer.data()};

        while (cinfo.output_scanline < cinfo.output_height) {
            int y = static_cast<int>(cinfo.output_scanline);
            jpeg_read_scanlines(&cinfo, rowPointer, 1);
            for (int x = 0; x < width; ++x) {
                size_t idx = static_cast<size_t>(x) * static_cast<size_t>(channels);
                result.setRgba(x, y, rowBuffer[idx], rowBuffer[idx + 1], rowBuffer[idx + 2], 255);
            }
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return result;
    } catch (...) {
        jpeg_destroy_decompress(&cinfo);
        throw;
    }
}

} // namespace bitdeck
