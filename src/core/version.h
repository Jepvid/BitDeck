#pragma once

#include <cstdint>

namespace bitdeck {

enum class Version : int32_t {
    Unknown = -1,
    Version0 = 0, // Texture/Background base header
    Version1 = 1, // Texture extended header (textureFlags + scale floats)
    Version2 = 2, // Sequence format
    Version3 = 3, // reserved, unused
    Version4 = 4, // reserved, unused
    Version5 = 5, // reserved, unused
    Version6 = 6, // reserved, unused
    Version7 = 7, // reserved, unused
    Version8 = 8, // reserved, unused
};

inline Version versionFromValue(int32_t value) {
    if (value >= 0 && value <= 8) {
        return static_cast<Version>(value);
    }
    return Version::Unknown;
}

} // namespace bitdeck
