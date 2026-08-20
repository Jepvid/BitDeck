#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "../resource.h"

namespace bitdeck {

// N64-style pixel format for an OTEX resource.
enum class TextureType : int32_t {
    Error = 0,
    RGBA32bpp = 1,
    RGBA16bpp = 2,
    Palette4bpp = 3,
    Palette8bpp = 4,
    Grayscale4bpp = 5,
    Grayscale8bpp = 6,
    GrayscaleAlpha4bpp = 7,
    GrayscaleAlpha8bpp = 8,
    GrayscaleAlpha16bpp = 9,
    JPEG32bpp = 10,
};

double textureTypePixelMultiplier(TextureType type);
int32_t textureTypeBitSize(TextureType type);

// Truncating byte size for a width x height texture of this type.
inline int32_t textureBufferSize(TextureType type, int32_t width, int32_t height) {
    return static_cast<int32_t>(static_cast<double>(width) * static_cast<double>(height) *
                                 textureTypePixelMultiplier(type));
}

// Throws on an unrecognized value.
TextureType textureTypeFromValue(int32_t value);

bool textureTypeHasAlpha(TextureType type);
bool textureTypeIsPalette(TextureType type);

struct RgbaColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

// A decoded image: either raw per-channel pixel bytes, or a palette index
// buffer for indexed images. Pixel data only -- no PNG/JPEG file codec.
struct RgbaImage {
    int width = 0;
    int height = 0;

    // Non-palette images: 1 (gray), 2 (gray+alpha), 3 (rgb), or 4 (rgba)
    // bytes/pixel, row-major in channelData.
    int numChannels = 4;
    std::vector<uint8_t> channelData;

    // Palette (indexed) images: one index byte/pixel in indexData, with
    // up to 256 colors in palette.
    bool withPalette = false;
    std::vector<uint8_t> indexData;
    std::vector<RgbaColor> palette;

    static RgbaImage makeChannelImage(int width, int height, int numChannels);
    static RgbaImage makePaletteImage(int width, int height);

    // Missing channels fall back to grayscale replication (g/b) or opaque
    // (a); palette images resolve through the palette.
    uint8_t r(int x, int y) const;
    uint8_t g(int x, int y) const;
    uint8_t b(int x, int y) const;
    uint8_t a(int x, int y) const;
    void setRgba(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // Index lookup clamped to width-1/height-1. Both return 0 rather than
    // reading out of bounds if called on a non-palette image.
    uint8_t indexSafe(int x, int y) const;
    uint8_t index(int x, int y) const;
    void setIndex(int x, int y, uint8_t value);
    void setPaletteEntry(uint8_t index, RgbaColor color);
};

// Encodes a decoded image into N64 texture bytes for the given type.
// channelScale divides source channel values before packing: 256 for a
// 16-bit-per-channel source image, 1 for 8-bit.
std::vector<uint8_t> encodeN64Texture(const RgbaImage& image, TextureType type, int channelScale = 1);

// Decodes N64 texture bytes into an RgbaImage. Palette types come back with
// a synthetic grayscale palette (index*16 per channel) unless overridden by
// applyTlutPalette(). Returns an empty (width=0) image for JPEG32bpp/Error.
RgbaImage decodeN64Texture(const std::vector<uint8_t>& texData, TextureType type, int width, int height);

// Overwrites a palette image's color table (indices baseIndex.. up to
// 16x16 = 256 entries) from a separately-decoded TLUT texture's pixels.
// Callable more than once at different baseIndex values to fill a single
// image's color table from multiple TLUT sources.
void applyTlutPalette(RgbaImage& image, const RgbaImage& tlutRgba, int baseIndex = 0);

// Returns k such that image is exactly k*width x k*height, or nullopt.
std::optional<int> exactMultiple(const RgbaImage& image, int width, int height);

// Pastes image at (0,0) onto a transparent width x height RGBA canvas
// (palette sources become plain RGBA). Returns image unchanged if it's
// already width x height.
RgbaImage padCanvas(const RgbaImage& image, int width, int height);

class Texture : public Resource {
public:
    static constexpr int32_t kLoadAsRaw = 1 << 0;

    Texture() : Resource(ResourceType::Texture, 0, Version::Version0) {}
    Texture(TextureType type, int32_t width, int32_t height, std::vector<uint8_t> texData)
        : Resource(ResourceType::Texture, 0, Version::Version0),
          textureType_(type), width_(width), height_(height),
          texDataSize_(static_cast<int32_t>(texData.size())), texData_(std::move(texData)) {}

    static Texture empty() { return Texture(TextureType::Error, 0, 0, {}); }

    TextureType textureType() const { return textureType_; }
    void setTextureType(TextureType type) { textureType_ = type; }
    int32_t width() const { return width_; }
    int32_t height() const { return height_; }
    void setDimensions(int32_t width, int32_t height) { width_ = width; height_ = height; }
    const std::vector<uint8_t>& texData() const { return texData_; }
    void setTexData(std::vector<uint8_t> data) {
        texData_ = std::move(data);
        texDataSize_ = static_cast<int32_t>(texData_.size());
    }
    bool isPalette() const { return textureTypeIsPalette(textureType_); }
    bool hasAlpha() const { return textureTypeHasAlpha(textureType_); }

    // Forces the extended (Version1) header variant.
    void setTextureFlags(int32_t flags) {
        textureFlags_ = flags;
        gameVersion_ = Version::Version1;
    }
    void setTextureScale(double hByteScale, double vPixelScale) {
        textureHByteScale_ = hByteScale;
        textureVPixelScale_ = vPixelScale;
        gameVersion_ = Version::Version1;
    }
    int32_t textureFlags() const { return textureFlags_; }
    double textureHByteScale() const { return textureHByteScale_; }
    double textureVPixelScale() const { return textureVPixelScale_; }

    int32_t getTMEMSize() const;
    static int32_t getMaxTMEMSize(bool isPalette) { return isPalette ? 2048 : 4096; }

protected:
    void writeResourceData() override;
    void readResourceData() override;

private:
    TextureType textureType_ = TextureType::Error;
    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t textureFlags_ = 0;
    double textureHByteScale_ = 1;
    double textureVPixelScale_ = 1;
    int32_t texDataSize_ = 0;
    std::vector<uint8_t> texData_;
};

} // namespace bitdeck
