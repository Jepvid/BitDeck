#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../resource.h"

namespace bitdeck {

// OSEQ resource: a SoH audio sequence (MIDI-like N64 sound sequence binary)
// built from a raw .seq binary plus a sidecar metadata text file. Write-only.
class Sequence : public Resource {
public:
    Sequence()
        : Resource(ResourceType::SohAudioSequence, 0, Version::Version2) {}

    // metaText: sidecar file contents, newline-separated.
    //   line 0: name (trimmed; '/' replaced with '|')
    //   line 1: hex font index (optional "0x" prefix, case-insensitive)
    //   line 2: optional type string, lowercased/trimmed, default "bgm"
    // cachePolicy is 2 when type == "bgm", else 1. medium is always 2,
    // sequenceNum always 0, numFonts always 1.
    static Sequence fromSeqFile(std::vector<uint8_t> rawBinary, const std::string& metaText);

    // Archive-relative filename fragment: "<name>_<type>". Final entry path
    // is "<stageKey>/<path>".
    const std::string& path() const { return path_; }

protected:
    void writeResourceData() override;

private:
    int32_t size_ = 0;
    std::vector<uint8_t> rawBinary_;
    int8_t sequenceNum_ = 0;
    int8_t medium_ = 2;
    int8_t cachePolicy_ = 1;
    int32_t numFonts_ = 1;
    std::vector<uint8_t> fontIndices_;
    std::string path_;
};

} // namespace bitdeck
