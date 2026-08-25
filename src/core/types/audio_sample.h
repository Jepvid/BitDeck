#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../resource.h"

namespace bitdeck {

// OSMP resource: a VADPCM-encoded audio sample with its codebook and
// optional loop points. Write-only.
class AudioSample : public Resource {
public:
    AudioSample() : Resource(ResourceType::SohAudioSample, 0, Version::Version2) { isCustom = false; }

    static AudioSample fromVadpcm(std::vector<uint8_t> adpcmData, uint32_t sampleCount, int32_t order,
                                   int32_t predictors, std::vector<int16_t> book);

    void setLoop(uint32_t loopStart, uint32_t loopEnd, int32_t loopCount, std::array<int16_t, 16> loopState);

protected:
    void writeResourceData() override;

private:
    std::vector<uint8_t> adpcmData_;
    uint32_t sampleCount_ = 0;
    int32_t order_ = 0;
    int32_t predictors_ = 0;
    std::vector<int16_t> book_;
    bool loopEnabled_ = false;
    uint32_t loopStart_ = 0;
    uint32_t loopEnd_ = 0;
    int32_t loopCount_ = 0;
    std::array<int16_t, 16> loopState_{};
};

} // namespace bitdeck
