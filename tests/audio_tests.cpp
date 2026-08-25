#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/audio/vadpcm_codec.h"
#include "core/audio/wav_file.h"
#include "core/types/audio_sample.h"

namespace {

int g_failures = 0;

void check(bool condition, const char* description) {
    if (condition) {
        std::printf("[PASS] %s\n", description);
    } else {
        std::printf("[FAIL] %s\n", description);
        ++g_failures;
    }
}

void appendU32LE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendU16LE(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

// Mono 16-bit PCM RIFF/WAVE fixture, matching the layout readWavFile parses.
std::vector<uint8_t> makeWavFixture(uint32_t sampleRate, const std::vector<int16_t>& samples) {
    std::vector<uint8_t> data;
    for (int16_t s : samples) {
        appendU16LE(data, static_cast<uint16_t>(s));
    }

    std::vector<uint8_t> out;
    auto append4 = [&out](const char* tag) { out.insert(out.end(), tag, tag + 4); };

    append4("RIFF");
    appendU32LE(out, static_cast<uint32_t>(4 + (8 + 16) + (8 + data.size())));
    append4("WAVE");

    append4("fmt ");
    appendU32LE(out, 16);
    appendU16LE(out, 1); // PCM
    appendU16LE(out, 1); // mono
    appendU32LE(out, sampleRate);
    appendU32LE(out, sampleRate * 2); // byte rate
    appendU16LE(out, 2);              // block align
    appendU16LE(out, 16);             // bits per sample

    append4("data");
    appendU32LE(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

void testWavRoundTrip() {
    using namespace bitdeck;

    std::vector<int16_t> samples;
    for (int i = 0; i < 64; ++i) {
        samples.push_back(static_cast<int16_t>(1000 * std::sin(i * 0.3)));
    }
    std::vector<uint8_t> wavBytes = makeWavFixture(32000, samples);

    WavData wav;
    std::string error;
    check(readWavFile(wavBytes, wav, error), "WAV: valid mono 16-bit fixture parses");
    check(wav.sampleRate == 32000, "WAV: sample rate round-trips");
    check(wav.samples == samples, "WAV: sample values round-trip exactly");

    std::vector<uint8_t> notRiff = {'x', 'x', 'x', 'x'};
    WavData bad;
    check(!readWavFile(notRiff, bad, error), "WAV: non-RIFF data is rejected");
}

void testVadpcmRoundTrip() {
    using namespace bitdeck;

    std::vector<int16_t> samples;
    for (int i = 0; i < 320; ++i) {
        samples.push_back(static_cast<int16_t>(8000 * std::sin(i * 0.2)));
    }

    VadpcmEncoded encoded;
    std::string error;
    check(encodeVadpcm(samples, 4, encoded, error), "VADPCM: encode succeeds");
    check(encoded.adpcmData.size() % 9 == 0, "VADPCM: encoded size is a multiple of the 9-byte frame");

    std::vector<int16_t> decoded;
    check(decodeVadpcm(encoded, decoded, error), "VADPCM: decode succeeds");
    check(decoded.size() >= samples.size(), "VADPCM: decoded length covers every input sample");

    double sumAbsErr = 0.0;
    for (size_t i = 0; i < samples.size(); ++i) {
        sumAbsErr += std::abs(static_cast<int>(decoded[i]) - static_cast<int>(samples[i]));
    }
    double meanAbsErr = sumAbsErr / static_cast<double>(samples.size());
    check(meanAbsErr < 50.0, "VADPCM: round-trip mean error stays small relative to int16 range");

    std::string unused;
    VadpcmEncoded badPredictorCount;
    check(!encodeVadpcm(samples, 0, badPredictorCount, unused), "VADPCM: rejects predictor count below 1");
    check(!encodeVadpcm(samples, 17, badPredictorCount, unused), "VADPCM: rejects predictor count above 16");
}

void testBuildLoopState() {
    using namespace bitdeck;

    std::vector<int16_t> samples = {10, 20, 30, 40, 50};
    auto state = buildLoopState(samples, 3);
    check(state[0] == 0 && state[12] == 0, "buildLoopState: pads zeros before a short history");
    check(state[13] == 10 && state[14] == 20 && state[15] == 30, "buildLoopState: ends with the samples right before loopStart");

    std::vector<int16_t> longSamples(20);
    for (size_t i = 0; i < longSamples.size(); ++i) {
        longSamples[i] = static_cast<int16_t>(i);
    }
    auto fullState = buildLoopState(longSamples, 18);
    check(fullState[0] == 2 && fullState[15] == 17, "buildLoopState: takes the 16 samples right before loopStart");
}

void testAudioSampleHeaderLayout() {
    using namespace bitdeck;

    AudioSample sample = AudioSample::fromVadpcm({1, 2, 3}, 5, 2, 4, {10, 20, 30, 40});
    std::vector<uint8_t> bytes = sample.build();

    check(bytes.size() >= Resource::kHeaderSize, "AudioSample: output is at least one header long");
    check(bytes[4] == 'P' && bytes[5] == 'M' && bytes[6] == 'S' && bytes[7] == 'O',
          "AudioSample: resource type FourCC is OSMP (little-endian bytes)");
    check(bytes[8] == 2 && bytes[9] == 0 && bytes[10] == 0 && bytes[11] == 0,
          "AudioSample: gameVersion slot holds 2, matching SoH's sample format");
    check(bytes[24] == 0, "AudioSample: isCustom byte is 0, matching SoH's sample writer");

    size_t payload = Resource::kHeaderSize;
    check(bytes[payload + 4] == 3 && bytes[payload + 5] == 0 && bytes[payload + 6] == 0 && bytes[payload + 7] == 0,
          "AudioSample: adpcmData size field matches the 3 bytes given");
    check(bytes[payload + 8] == 1 && bytes[payload + 9] == 2 && bytes[payload + 10] == 3,
          "AudioSample: adpcmData bytes follow immediately");
}

} // namespace

int main() {
    testWavRoundTrip();
    testVadpcmRoundTrip();
    testBuildLoopState();
    testAudioSampleHeaderLayout();

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
