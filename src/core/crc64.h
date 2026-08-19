#pragma once

#include <cstdint>
#include <string>

namespace bitdeck {

// CRC64 of a string, matching Shipwright/libultraship's resource-path hash
// (StrHash64.cpp) used to resolve OTR_HASH pointers in display lists.
uint64_t crc64(const std::string& text);

} // namespace bitdeck
