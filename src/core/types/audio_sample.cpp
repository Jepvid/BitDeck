#include "audio_sample.h"

namespace bitdeck {

AudioSample AudioSample::fromVadpcm(std::vector<uint8_t> adpcmData, uint32_t sampleCount, int32_t order,
                                     int32_t predictors, std::vector<int16_t> book) {
    AudioSample sample;
    sample.adpcmData_ = std::move(adpcmData);
    sample.sampleCount_ = sampleCount;
    sample.order_ = order;
    sample.predictors_ = predictors;
    sample.book_ = std::move(book);
    return sample;
}

void AudioSample::setLoop(uint32_t loopStart, uint32_t loopEnd, int32_t loopCount,
                           std::array<int16_t, 16> loopState) {
    loopEnabled_ = true;
    loopStart_ = loopStart;
    loopEnd_ = loopEnd;
    loopCount_ = loopCount;
    loopState_ = loopState;
}

void AudioSample::writeResourceData() {
    writeInt8(0); // codec: ADPCM
    writeInt8(0); // medium
    writeInt8(0); // unk_bit26
    writeInt8(0); // isRelocated

    writeInt32(static_cast<int32_t>(adpcmData_.size()));
    appendData(adpcmData_);

    if (loopEnabled_) {
        writeInt32(static_cast<int32_t>(loopStart_));
        writeInt32(static_cast<int32_t>(loopEnd_));
        writeInt32(loopCount_);
        writeInt32(16);
        for (int16_t value : loopState_) {
            writeInt16(value);
        }
    } else {
        writeInt32(0);
        writeInt32(static_cast<int32_t>(sampleCount_));
        writeInt32(0);
        writeInt32(0);
    }

    writeInt32(order_);
    writeInt32(predictors_);
    writeInt32(static_cast<int32_t>(book_.size()));
    for (int16_t value : book_) {
        writeInt16(value);
    }
}

} // namespace bitdeck
