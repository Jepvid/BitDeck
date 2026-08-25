#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bitdeck {

struct VadpcmEncoded {
    std::vector<uint8_t> adpcmData;
    int32_t order = 0;
    int32_t predictors = 0;
    std::vector<int16_t> book;
};

// Encodes 16-bit PCM samples as VADPCM. samples.size() is padded with
// trailing zeros up to a multiple of 16 (one VADPCM frame) before encoding.
// predictorCount must be between 1 and 16.
bool encodeVadpcm(const std::vector<int16_t>& samples, int predictorCount, VadpcmEncoded& out, std::string& error);

// Decodes a VadpcmEncoded's adpcmData back to 16-bit PCM samples, using its
// own order/predictors/book.
bool decodeVadpcm(const VadpcmEncoded& encoded, std::vector<int16_t>& outSamples, std::string& error);

// The 16 decoded samples immediately preceding loopStart, used as the
// decoder's warm-up state when a loop wraps back to loopStart. Samples
// before index 0 (loopStart < 16) are zero.
std::array<int16_t, 16> buildLoopState(const std::vector<int16_t>& samples, uint32_t loopStart);

} // namespace bitdeck
