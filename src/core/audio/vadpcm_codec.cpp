#include "vadpcm_codec.h"

#include <sstream>

extern "C" {
#include "codec/vadpcm.h"
}

namespace bitdeck {

namespace {

std::string vadpcmErrorString(vadpcm_error err) {
    const char* text = vadpcm_error_name(err);
    if (text) {
        return text;
    }
    std::ostringstream oss;
    oss << "vadpcm error " << static_cast<int>(err);
    return oss.str();
}

} // namespace

bool encodeVadpcm(const std::vector<int16_t>& samples, int predictorCount, VadpcmEncoded& out, std::string& error) {
    if (predictorCount < 1 || predictorCount > kVADPCMMaxPredictorCount) {
        error = "Predictor count must be between 1 and 16.";
        return false;
    }

    size_t totalSamples = samples.size();
    size_t frameCount = (totalSamples + kVADPCMFrameSampleCount - 1) / kVADPCMFrameSampleCount;
    size_t paddedSamples = frameCount * kVADPCMFrameSampleCount;
    size_t encodedBytes = frameCount * kVADPCMFrameByteSize;
    size_t codebookVecs = static_cast<size_t>(predictorCount) * kVADPCMEncodeOrder;

    std::vector<vadpcm_vector> codebook(codebookVecs);
    std::vector<uint8_t> encoded(encodedBytes);

    vadpcm_params params{};
    params.predictor_count = predictorCount;

    std::vector<int16_t> input;
    const int16_t* inputPtr = nullptr;
    if (paddedSamples > 0) {
        input = samples;
        input.resize(paddedSamples, 0);
        inputPtr = input.data();
    }

    vadpcm_error err = vadpcm_encode(&params, codebook.data(), frameCount, encoded.data(), inputPtr, nullptr);
    if (err != kVADPCMErrNone) {
        error = "VADPCM encode failed: " + vadpcmErrorString(err);
        return false;
    }

    size_t bookCount = codebookVecs * kVADPCMVectorSampleCount;
    std::vector<int16_t> book(bookCount);
    for (size_t i = 0; i < codebookVecs; i++) {
        for (size_t j = 0; j < kVADPCMVectorSampleCount; j++) {
            book[i * kVADPCMVectorSampleCount + j] = codebook[i].v[j];
        }
    }

    out.adpcmData = std::move(encoded);
    out.order = kVADPCMEncodeOrder;
    out.predictors = predictorCount;
    out.book = std::move(book);
    return true;
}

bool decodeVadpcm(const VadpcmEncoded& encoded, std::vector<int16_t>& outSamples, std::string& error) {
    if (encoded.order <= 0 || encoded.predictors <= 0) {
        error = "Invalid VADPCM codebook.";
        return false;
    }
    if (encoded.adpcmData.size() % kVADPCMFrameByteSize != 0) {
        error = "Invalid VADPCM data size.";
        return false;
    }
    size_t expectedBook =
        static_cast<size_t>(encoded.order) * static_cast<size_t>(encoded.predictors) * kVADPCMVectorSampleCount;
    if (encoded.book.size() < expectedBook) {
        error = "VADPCM codebook is incomplete.";
        return false;
    }

    size_t codebookVecs = static_cast<size_t>(encoded.order) * static_cast<size_t>(encoded.predictors);
    std::vector<vadpcm_vector> codebook(codebookVecs);
    for (size_t i = 0; i < codebookVecs; i++) {
        for (size_t j = 0; j < kVADPCMVectorSampleCount; j++) {
            codebook[i].v[j] = encoded.book[i * kVADPCMVectorSampleCount + j];
        }
    }

    size_t frameCount = encoded.adpcmData.size() / kVADPCMFrameByteSize;
    outSamples.resize(frameCount * kVADPCMFrameSampleCount);
    if (frameCount == 0) {
        return true;
    }

    vadpcm_vector state{};
    vadpcm_error err = vadpcm_decode(encoded.predictors, encoded.order, codebook.data(), &state, frameCount,
                                      outSamples.data(), encoded.adpcmData.data());
    if (err != kVADPCMErrNone) {
        error = "VADPCM decode failed: " + vadpcmErrorString(err);
        return false;
    }
    return true;
}

std::array<int16_t, 16> buildLoopState(const std::vector<int16_t>& samples, uint32_t loopStart) {
    std::array<int16_t, 16> state{};
    if (samples.empty()) {
        return state;
    }

    if (loopStart >= 16) {
        for (size_t i = 0; i < 16; i++) {
            state[i] = samples[loopStart - 16 + i];
        }
    } else {
        size_t pad = 16 - loopStart;
        for (size_t i = 0; i < pad; i++) {
            state[i] = 0;
        }
        for (size_t i = 0; i < loopStart; i++) {
            state[pad + i] = samples[i];
        }
    }

    return state;
}

} // namespace bitdeck
