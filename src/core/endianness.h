#pragma once

#include <cstdint>

namespace bitdeck {

enum class Endianness : int32_t {
    Unknown = -1,
    Little = 0,
    Big = 1,
};

// Host byte order. Always little-endian (x86_64/ARM64 desktop targets).
inline Endianness nativeEndianness() {
    return Endianness::Little;
}

inline Endianness endiannessFromValue(int32_t value) {
    switch (value) {
        case 0: return Endianness::Little;
        case 1: return Endianness::Big;
        default: return Endianness::Unknown;
    }
}

} // namespace bitdeck
