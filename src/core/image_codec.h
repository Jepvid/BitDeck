#pragma once

#include <cstdint>
#include <vector>

#include "types/texture.h"

namespace bitdeck {

// Decodes PNG bytes into an RgbaImage. Indexed (palette) PNGs decode with
// withPalette=true and raw index data preserved (needed for the
// Palette4bpp/Palette8bpp texture round-trip); every other color type
// normalizes to 4-channel 8-bit RGBA (16-bit-per-channel sources are
// reduced to 8-bit during decode, equivalent to Retro's mod=256 divisor).
// Throws std::runtime_error on a malformed PNG.
RgbaImage decodePng(const std::vector<uint8_t>& data);

// Encodes an RgbaImage back to PNG bytes. Palette images (withPalette) write
// PLTE (+tRNS if any entry has alpha < 255); channel images write RGB or
// RGBA depending on numChannels.
std::vector<uint8_t> encodePng(const RgbaImage& image);

// Decodes JPEG bytes into a 4-channel 8-bit RGBA RgbaImage (alpha forced to
// 255, matching Background's raw-JPEG-blob handling in Retro).
RgbaImage decodeJpeg(const std::vector<uint8_t>& data);

} // namespace bitdeck
