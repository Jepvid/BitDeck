#include "wav_file.h"

#include <cstring>

namespace bitdeck {

namespace {

uint16_t readU16LE(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t readU32LE(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

} // namespace

bool readWavFile(const std::vector<uint8_t>& bytes, WavData& out, std::string& error) {
    if (bytes.size() < 12) {
        error = "WAV header too small.";
        return false;
    }
    if (std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        error = "Not a RIFF/WAVE file.";
        return false;
    }

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;

    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const uint8_t* chunk = bytes.data() + offset;
        uint32_t chunkSize = readU32LE(chunk + 4);
        if (offset + 8 + chunkSize > bytes.size()) {
            error = "Invalid chunk size.";
            return false;
        }

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (chunkSize < 16) {
                error = "Invalid fmt chunk.";
                return false;
            }
            audioFormat = readU16LE(chunk + 8);
            numChannels = readU16LE(chunk + 10);
            sampleRate = readU32LE(chunk + 12);
            bitsPerSample = readU16LE(chunk + 22);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataOffset = static_cast<uint32_t>(offset + 8);
            dataSize = chunkSize;
        }

        offset += 8 + chunkSize;
        if (chunkSize & 1) {
            offset += 1;
        }
    }

    if (audioFormat != 1) {
        error = "WAV must be PCM format.";
        return false;
    }
    if (numChannels != 1) {
        error = "WAV must be mono.";
        return false;
    }
    if (bitsPerSample != 16) {
        error = "WAV must be 16-bit PCM.";
        return false;
    }
    if (dataOffset == 0 || dataSize == 0) {
        error = "Missing data chunk.";
        return false;
    }
    if (static_cast<size_t>(dataOffset) + dataSize > bytes.size()) {
        error = "Invalid data range.";
        return false;
    }
    if (dataSize % 2 != 0) {
        error = "Data size is not 16-bit aligned.";
        return false;
    }

    out.sampleRate = sampleRate;
    out.samples.resize(dataSize / 2);
    for (size_t i = 0; i < out.samples.size(); i++) {
        const uint8_t* samplePtr = bytes.data() + dataOffset + i * 2;
        out.samples[i] = static_cast<int16_t>(readU16LE(samplePtr));
    }

    return true;
}

} // namespace bitdeck
