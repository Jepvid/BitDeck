#include "texture.h"

#include <algorithm>
#include <cmath>

namespace bitdeck {

double textureTypePixelMultiplier(TextureType type) {
    switch (type) {
        case TextureType::Error: return 0.0;
        case TextureType::RGBA32bpp: return 4.0;
        case TextureType::RGBA16bpp: return 2.0;
        case TextureType::Palette4bpp: return 0.5;
        case TextureType::Palette8bpp: return 1.0;
        case TextureType::Grayscale4bpp: return 0.5;
        case TextureType::Grayscale8bpp: return 1.0;
        case TextureType::GrayscaleAlpha4bpp: return 0.5;
        case TextureType::GrayscaleAlpha8bpp: return 1.0;
        case TextureType::GrayscaleAlpha16bpp: return 2.0;
        case TextureType::JPEG32bpp: return 0.0;
    }
    return 0.0;
}

int32_t textureTypeBitSize(TextureType type) {
    return static_cast<int32_t>(std::lround(8.0 * textureTypePixelMultiplier(type)));
}

TextureType textureTypeFromValue(int32_t value) {
    switch (value) {
        case 0: return TextureType::Error;
        case 1: return TextureType::RGBA32bpp;
        case 2: return TextureType::RGBA16bpp;
        case 3: return TextureType::Palette4bpp;
        case 4: return TextureType::Palette8bpp;
        case 5: return TextureType::Grayscale4bpp;
        case 6: return TextureType::Grayscale8bpp;
        case 7: return TextureType::GrayscaleAlpha4bpp;
        case 8: return TextureType::GrayscaleAlpha8bpp;
        case 9: return TextureType::GrayscaleAlpha16bpp;
        case 10: return TextureType::JPEG32bpp;
        default:
            throw std::runtime_error("Unknown TextureType value: " + std::to_string(value));
    }
}

bool textureTypeHasAlpha(TextureType type) {
    return textureTypeIsPalette(type) || type == TextureType::RGBA32bpp || type == TextureType::RGBA16bpp ||
           type == TextureType::GrayscaleAlpha16bpp || type == TextureType::GrayscaleAlpha8bpp ||
           type == TextureType::GrayscaleAlpha4bpp;
}

bool textureTypeIsPalette(TextureType type) {
    return type == TextureType::Palette4bpp || type == TextureType::Palette8bpp;
}

// ---------------------------------------------------------------------------
// RgbaImage
// ---------------------------------------------------------------------------

RgbaImage RgbaImage::makeChannelImage(int width, int height, int numChannels) {
    RgbaImage image;
    image.width = width;
    image.height = height;
    image.numChannels = numChannels;
    image.channelData.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(numChannels), 0);
    return image;
}

RgbaImage RgbaImage::makePaletteImage(int width, int height) {
    RgbaImage image;
    image.width = width;
    image.height = height;
    image.withPalette = true;
    image.indexData.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    image.palette.assign(256, RgbaColor{});
    return image;
}

uint8_t RgbaImage::r(int x, int y) const {
    if (withPalette) {
        return palette[index(x, y)].r;
    }
    size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * static_cast<size_t>(numChannels);
    return channelData[idx + 0];
}

uint8_t RgbaImage::g(int x, int y) const {
    if (withPalette) {
        return palette[index(x, y)].g;
    }
    if (numChannels >= 2) {
        size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * static_cast<size_t>(numChannels);
        return channelData[idx + 1];
    }
    return r(x, y);
}

uint8_t RgbaImage::b(int x, int y) const {
    if (withPalette) {
        return palette[index(x, y)].b;
    }
    if (numChannels >= 3) {
        size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * static_cast<size_t>(numChannels);
        return channelData[idx + 2];
    }
    return r(x, y);
}

uint8_t RgbaImage::a(int x, int y) const {
    if (withPalette) {
        return palette[index(x, y)].a;
    }
    if (numChannels >= 4) {
        size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * static_cast<size_t>(numChannels);
        return channelData[idx + 3];
    }
    return 255;
}

void RgbaImage::setRgba(int x, int y, uint8_t rv, uint8_t gv, uint8_t bv, uint8_t av) {
    size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * static_cast<size_t>(numChannels);
    if (numChannels >= 1) channelData[idx + 0] = rv;
    if (numChannels >= 2) channelData[idx + 1] = (numChannels == 2) ? av : gv;
    if (numChannels >= 3) channelData[idx + 2] = bv;
    if (numChannels >= 4) channelData[idx + 3] = av;
}

uint8_t RgbaImage::index(int x, int y) const {
    size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    return idx < indexData.size() ? indexData[idx] : 0;
}

uint8_t RgbaImage::indexSafe(int x, int y) const {
    int cx = std::clamp(x, 0, width - 1);
    int cy = std::clamp(y, 0, height - 1);
    size_t idx = static_cast<size_t>(cy) * static_cast<size_t>(width) + static_cast<size_t>(cx);
    return idx < indexData.size() ? indexData[idx] : 0;
}

void RgbaImage::setIndex(int x, int y, uint8_t value) {
    indexData[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = value;
}

void RgbaImage::setPaletteEntry(uint8_t index, RgbaColor color) {
    palette[index] = color;
}

// ---------------------------------------------------------------------------
// Encode: RgbaImage -> N64 texel bytes
// ---------------------------------------------------------------------------

std::vector<uint8_t> encodeN64Texture(const RgbaImage& image, TextureType type, int channelScale) {
    int width = image.width;
    int height = image.height;
    std::vector<uint8_t> texData(static_cast<size_t>(textureBufferSize(type, width, height)), 0);
    int mod = channelScale;

    switch (type) {
        case TextureType::RGBA16bpp: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 2;
                    int r = image.r(x, y) / 8;
                    int g = image.g(x, y) / 8;
                    int b = image.b(x, y) / 8;
                    int alphaBit = image.a(x, y) != 0 ? 1 : 0;
                    int data = (r << 11) | (g << 6) | (b << 1) | alphaBit;
                    texData[pos + 0] = static_cast<uint8_t>((data & 0xFF00) >> 8);
                    texData[pos + 1] = static_cast<uint8_t>(data & 0x00FF);
                }
            }
            break;
        }
        case TextureType::RGBA32bpp: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
                    switch (image.numChannels) {
                        case 1: {
                            uint8_t g = static_cast<uint8_t>(image.r(x, y) / mod);
                            texData[pos + 0] = g; texData[pos + 1] = g; texData[pos + 2] = g; texData[pos + 3] = 0xFF;
                            break;
                        }
                        case 2: {
                            uint8_t g = static_cast<uint8_t>(image.r(x, y) / mod);
                            uint8_t a = static_cast<uint8_t>(image.g(x, y) / mod);
                            texData[pos + 0] = g; texData[pos + 1] = g; texData[pos + 2] = g; texData[pos + 3] = a;
                            break;
                        }
                        case 3: {
                            texData[pos + 0] = static_cast<uint8_t>(image.r(x, y) / mod);
                            texData[pos + 1] = static_cast<uint8_t>(image.g(x, y) / mod);
                            texData[pos + 2] = static_cast<uint8_t>(image.b(x, y) / mod);
                            texData[pos + 3] = 0xFF;
                            break;
                        }
                        case 4:
                        default: {
                            texData[pos + 0] = static_cast<uint8_t>(image.r(x, y) / mod);
                            texData[pos + 1] = static_cast<uint8_t>(image.g(x, y) / mod);
                            texData[pos + 2] = static_cast<uint8_t>(image.b(x, y) / mod);
                            texData[pos + 3] = static_cast<uint8_t>(image.a(x, y) / mod);
                            break;
                        }
                    }
                }
            }
            break;
        }
        case TextureType::Palette4bpp: {
            // Retro's Dart encoder double-writes each output byte here (wrong-pairing bug); not replicated.
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 2) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) / 2;
                    uint8_t cr1 = image.index(x, y);
                    uint8_t cr2 = image.indexSafe(x + 1, y);
                    texData[pos] = static_cast<uint8_t>((cr1 << 4) | cr2);
                }
            }
            break;
        }
        case TextureType::Palette8bpp: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    texData[pos] = image.index(x, y);
                }
            }
            break;
        }
        case TextureType::Grayscale4bpp: {
            // See Palette4bpp above: Retro's Dart encoder has a double-write
            // bug here too, not replicated.
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 2) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) / 2;
                    int r1 = image.r(x, y);
                    int r2 = image.r(x + 1, y);
                    texData[pos] = static_cast<uint8_t>(((r1 / 16) << 4) + (r2 / 16));
                }
            }
            break;
        }
        case TextureType::Grayscale8bpp: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    texData[pos] = image.r(x, y);
                }
            }
            break;
        }
        case TextureType::GrayscaleAlpha4bpp: {
            // See Palette4bpp above: Retro's Dart encoder has a double-write
            // bug here too, not replicated.
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 2) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) / 2;
                    int r1 = image.r(x, y);
                    int r2 = image.r(x + 1, y);
                    int alphaBit1 = image.a(x, y) != 0 ? 1 : 0;
                    int alphaBit2 = image.a(x + 1, y) != 0 ? 1 : 0;
                    int nib1 = ((r1 / 32) << 1) + alphaBit1;
                    int nib2 = ((r2 / 32) << 1) + alphaBit2;
                    texData[pos] = static_cast<uint8_t>((nib1 << 4) | nib2);
                }
            }
            break;
        }
        case TextureType::GrayscaleAlpha8bpp: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    texData[pos] = static_cast<uint8_t>(((image.r(x, y) / 16) << 4) + (image.a(x, y) / 16));
                }
            }
            break;
        }
        case TextureType::GrayscaleAlpha16bpp: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 2;
                    texData[pos + 0] = image.r(x, y);
                    texData[pos + 1] = image.a(x, y);
                }
            }
            break;
        }
        case TextureType::Error:
            throw std::runtime_error("Unknown texture type");
        case TextureType::JPEG32bpp:
            // Encoded blob, not a pixel-format payload.
            break;
    }

    return texData;
}

// ---------------------------------------------------------------------------
// Decode: N64 texel bytes -> RgbaImage
// ---------------------------------------------------------------------------

RgbaImage decodeN64Texture(const std::vector<uint8_t>& texData, TextureType type, int width, int height) {
    switch (type) {
        case TextureType::RGBA32bpp: {
            RgbaImage image = RgbaImage::makeChannelImage(width, height, 4);
            size_t count = std::min(image.channelData.size(), texData.size());
            std::copy_n(texData.begin(), count, image.channelData.begin());
            return image;
        }
        case TextureType::RGBA16bpp: {
            RgbaImage image = RgbaImage::makeChannelImage(width, height, 4);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 2;
                    int data = texData[pos + 1] | (texData[pos] << 8);
                    int r = (data & 0xF800) >> 11;
                    int g = (data & 0x07C0) >> 6;
                    int b = (data & 0x003E) >> 1;
                    int a = data & 0x01;
                    image.setRgba(x, y, static_cast<uint8_t>(r * 8), static_cast<uint8_t>(g * 8),
                                  static_cast<uint8_t>(b * 8), static_cast<uint8_t>(a * 255));
                }
            }
            return image;
        }
        case TextureType::Palette4bpp: {
            RgbaImage image = RgbaImage::makePaletteImage(width, height);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 2) {
                    for (int i = 0; i < 2; ++i) {
                        size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) / 2;
                        uint8_t paletteIndex = (i == 0) ? static_cast<uint8_t>((texData[pos] & 0xF0) >> 4)
                                                         : static_cast<uint8_t>(texData[pos] & 0x0F);
                        image.setIndex(x + i, y, paletteIndex);
                        image.setPaletteEntry(paletteIndex, RgbaColor{static_cast<uint8_t>(paletteIndex * 16),
                                                                       static_cast<uint8_t>(paletteIndex * 16),
                                                                       static_cast<uint8_t>(paletteIndex * 16), 255});
                    }
                }
            }
            return image;
        }
        case TextureType::Palette8bpp: {
            RgbaImage image = RgbaImage::makePaletteImage(width, height);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    uint8_t grayscale = texData[pos];
                    image.setIndex(x, y, grayscale);
                    image.setPaletteEntry(grayscale, RgbaColor{grayscale, grayscale, grayscale, 255});
                }
            }
            return image;
        }
        case TextureType::Grayscale4bpp: {
            RgbaImage image = RgbaImage::makeChannelImage(width, height, 3);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 2) {
                    for (int i = 0; i < 2; ++i) {
                        size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) / 2;
                        uint8_t grayscale = (i == 0) ? static_cast<uint8_t>(texData[pos] & 0xF0)
                                                      : static_cast<uint8_t>((texData[pos] & 0x0F) << 4);
                        image.setRgba(x + i, y, grayscale, grayscale, grayscale, 0);
                    }
                }
            }
            return image;
        }
        case TextureType::Grayscale8bpp: {
            RgbaImage image = RgbaImage::makeChannelImage(width, height, 3);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    uint8_t grayscale = texData[pos];
                    image.setRgba(x, y, grayscale, grayscale, grayscale, 0);
                }
            }
            return image;
        }
        case TextureType::GrayscaleAlpha4bpp: {
            RgbaImage image = RgbaImage::makeChannelImage(width, height, 4);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 2) {
                    for (int i = 0; i < 2; ++i) {
                        size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) / 2;
                        int nibble = (i == 0) ? ((texData[pos] & 0xF0) >> 4) : (texData[pos] & 0x0F);
                        uint8_t grayscale = static_cast<uint8_t>(((nibble & 0x0E) >> 1) * 32);
                        uint8_t alpha = static_cast<uint8_t>((nibble & 0x01) * 255);
                        image.setRgba(x + i, y, grayscale, grayscale, grayscale, alpha);
                    }
                }
            }
            return image;
        }
        case TextureType::GrayscaleAlpha8bpp: {
            RgbaImage image = RgbaImage::makeChannelImage(width, height, 4);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    uint8_t grayscale = static_cast<uint8_t>(texData[pos] & 0xF0);
                    uint8_t alpha = static_cast<uint8_t>((texData[pos] & 0x0F) << 4);
                    image.setRgba(x, y, grayscale, grayscale, grayscale, alpha);
                }
            }
            return image;
        }
        case TextureType::GrayscaleAlpha16bpp: {
            RgbaImage image = RgbaImage::makeChannelImage(width, height, 4);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pos = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 2;
                    uint8_t grayscale = texData[pos];
                    uint8_t alpha = texData[pos + 1];
                    image.setRgba(x, y, grayscale, grayscale, grayscale, alpha);
                }
            }
            return image;
        }
        case TextureType::Error:
        case TextureType::JPEG32bpp:
        default:
            return RgbaImage{}; // width=0: not decoded
    }
}

void applyTlutPalette(RgbaImage& image, const RgbaImage& tlutRgba, int baseIndex) {
    if (!image.withPalette) {
        return;
    }
    for (int y = 0; y < tlutRgba.height; ++y) {
        for (int x = 0; x < tlutRgba.width; ++x) {
            int index = baseIndex + y * tlutRgba.width + x;
            if (index < 0 || index >= 16 * 16) {
                continue;
            }
            image.setPaletteEntry(static_cast<uint8_t>(index),
                                   RgbaColor{tlutRgba.r(x, y), tlutRgba.g(x, y), tlutRgba.b(x, y), tlutRgba.a(x, y)});
        }
    }
}

std::optional<int> exactMultiple(const RgbaImage& image, int width, int height) {
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    if (image.width % width != 0 || image.height % height != 0) {
        return std::nullopt;
    }
    int k = image.width / width;
    return (k > 0 && image.height / height == k) ? std::optional<int>(k) : std::nullopt;
}

RgbaImage padCanvas(const RgbaImage& image, int width, int height) {
    if (image.width == width && image.height == height) {
        return image;
    }
    RgbaImage canvas = RgbaImage::makeChannelImage(width, height, 4);
    int copyWidth = std::min(image.width, width);
    int copyHeight = std::min(image.height, height);
    for (int y = 0; y < copyHeight; ++y) {
        for (int x = 0; x < copyWidth; ++x) {
            canvas.setRgba(x, y, image.r(x, y), image.g(x, y), image.b(x, y), image.a(x, y));
        }
    }
    return canvas;
}

// ---------------------------------------------------------------------------
// Texture (OTEX resource header + payload)
// ---------------------------------------------------------------------------

void Texture::writeResourceData() {
    writeInt32(static_cast<int32_t>(textureType_));
    writeInt32(width_);
    writeInt32(height_);
    if (gameVersion_ == Version::Version1) {
        writeInt32(textureFlags_);
        writeFloat32(static_cast<float>(textureHByteScale_));
        writeFloat32(static_cast<float>(textureVPixelScale_));
    }
    writeInt32(texDataSize_);
    appendData(texData_);
}

void Texture::readResourceData() {
    textureType_ = textureTypeFromValue(readInt32());
    width_ = readInt32();
    height_ = readInt32();
    if (gameVersion_ == Version::Version1) {
        textureFlags_ = readInt32();
        textureHByteScale_ = readFloat32();
        textureVPixelScale_ = readFloat32();
    }
    texDataSize_ = readInt32();
    texData_ = readBytes(static_cast<size_t>(texDataSize_));
}

int32_t Texture::getTMEMSize() const {
    int32_t bitSize = textureTypeBitSize(textureType_);
    if (bitSize == 0) {
        return 0;
    }
    int32_t texelsPerRow = 64 / bitSize;
    int32_t rows = (width_ + texelsPerRow - 1) / texelsPerRow; // ceil
    return rows * height_;
}

} // namespace bitdeck
