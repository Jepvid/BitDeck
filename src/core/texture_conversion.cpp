#include "texture_conversion.h"

#include "image_codec.h"

namespace bitdeck {

std::optional<Texture> convertPngToTexture(const std::vector<uint8_t>& pngBytes, const TextureManifestEntry& entry) {
    RgbaImage image;
    try {
        image = decodePng(pngBytes);
    } catch (const std::exception&) {
        return std::nullopt;
    }

    Texture texture;

    if (entry.kind == TextureEntryKind::AdditiveFontGlyph) {
        std::optional<int> k = exactMultiple(image, entry.textureWidth, entry.textureHeight);
        if (!k.has_value() && entry.tileWidth.has_value() && entry.tileHeight.has_value()) {
            k = exactMultiple(image, *entry.tileWidth, *entry.tileHeight);
        }
        if (!k.has_value()) {
            return std::nullopt; // not an integer multiple of its mask chunk or drawn tile
        }

        int alignedWidth = (entry.textureWidth + 3) & ~3;
        image = padCanvas(image, *k * alignedWidth, *k * entry.textureHeight);

        texture.setTextureType(TextureType::RGBA32bpp);
        texture.setTextureFlags(Texture::kLoadAsRaw);
        texture.setTextureScale(static_cast<double>(*k), static_cast<double>(*k));
        texture.setDimensions(image.width, image.height);
        texture.setTexData(encodeN64Texture(image, TextureType::RGBA32bpp));
        return texture;
    }

    // A tiled (but non-additive-glyph) entry whose replacement isn't an
    // exact multiple of the full texture: pad to an exact multiple of one
    // tile instead, if possible.
    if (entry.tileWidth.has_value() && entry.tileHeight.has_value() &&
        !exactMultiple(image, entry.textureWidth, entry.textureHeight).has_value()) {
        std::optional<int> k = exactMultiple(image, *entry.tileWidth, *entry.tileHeight);
        if (k.has_value()) {
            image = padCanvas(image, *k * entry.textureWidth, *k * entry.textureHeight);
        }
    }

    texture.setTextureType(entry.textureType);
    bool isPalette = image.withPalette &&
                      (entry.textureType == TextureType::Palette4bpp || entry.textureType == TextureType::Palette8bpp);

    bool isNotOriginalSize = entry.textureWidth != image.width || entry.textureHeight != image.height;
    bool isAdditive = entry.kind == TextureEntryKind::Additive;
    if (isAdditive) {
        isPalette = false;
    }

    if (isNotOriginalSize || isAdditive) {
        texture.setTextureFlags(Texture::kLoadAsRaw);
        if (!image.withPalette || !isPalette) {
            texture.setTextureType(TextureType::RGBA32bpp);
        }
        double hByteScale = (static_cast<double>(image.width) / entry.textureWidth) *
                             (textureTypePixelMultiplier(texture.textureType()) / textureTypePixelMultiplier(entry.textureType));
        double vPixelScale = static_cast<double>(image.height) / entry.textureHeight;
        texture.setTextureScale(hByteScale, vPixelScale);
    }

    texture.setDimensions(image.width, image.height);
    texture.setTexData(encodeN64Texture(image, texture.textureType()));

    if (entry.textureType == TextureType::Palette8bpp || entry.textureType == TextureType::Palette4bpp) {
        if (isPalette) {
            texture.setTextureType(entry.textureType);
        } else if (!isNotOriginalSize && !isAdditive) {
            return std::nullopt; // declared as palette but the source PNG isn't indexed
        }
    } else if (!isAdditive) {
        texture.setTextureType(entry.textureType);
    }

    return texture;
}

std::optional<Texture> convertJpegToTexture(const std::vector<uint8_t>& jpegBytes, const TextureManifestEntry& entry) {
    RgbaImage image;
    try {
        image = decodeJpeg(jpegBytes);
    } catch (const std::exception&) {
        return std::nullopt;
    }

    Texture texture;
    texture.setTextureType(TextureType::RGBA32bpp);
    texture.setTextureFlags(Texture::kLoadAsRaw);
    double hByteScale = (static_cast<double>(image.width) / entry.textureWidth) *
                         (textureTypePixelMultiplier(TextureType::RGBA32bpp) / textureTypePixelMultiplier(TextureType::RGBA16bpp));
    double vPixelScale = static_cast<double>(image.height) / entry.textureHeight;
    texture.setTextureScale(hByteScale, vPixelScale);
    texture.setDimensions(image.width, image.height);
    texture.setTexData(encodeN64Texture(image, TextureType::RGBA32bpp));
    return texture;
}

std::optional<Texture> convertImageToTexture(const std::vector<uint8_t>& imageBytes, const TextureManifestEntry& entry) {
    return entry.textureType == TextureType::JPEG32bpp ? convertJpegToTexture(imageBytes, entry)
                                                         : convertPngToTexture(imageBytes, entry);
}

} // namespace bitdeck
