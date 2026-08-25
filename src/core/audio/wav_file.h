#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bitdeck {

struct WavData {
    uint32_t sampleRate = 0;
    std::vector<int16_t> samples;
};

// Reads a mono, 16-bit PCM RIFF/WAVE file. Returns false with error set for
// anything else (stereo, non-PCM, 8/24/32-bit, malformed chunks).
bool readWavFile(const std::vector<uint8_t>& bytes, WavData& out, std::string& error);

} // namespace bitdeck
