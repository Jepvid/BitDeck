#pragma once

#include <cstdint>

namespace bitdeck {

// FourCC-style magic values ("OTEX", "OSEQ", ...) stored as int32 at
// resource header offset 0x04.
enum class ResourceType : int32_t {
    Unknown = -1,
    Archive = 0x4F415243,             // OARC (unused)
    DisplayList = 0x4F444C54,         // ODLT
    Vertex = 0x4F565458,              // OVTX
    Matrix = 0x4F4D5458,              // OMTX
    Array = 0x4F415252,               // OARR
    Blob = 0x4F424C42,                // OBLB
    Texture = 0x4F544558,             // OTEX
    SohAnimation = 0x4F414E4D,        // OANM
    SohPlayerAnimation = 0x4F50414D,  // OPAM
    SohRoom = 0x4F524F4D,             // OROM
    SohCollisionHeader = 0x4F434F4C,  // OCOL
    SohSkeleton = 0x4F534B4C,         // OSKL
    SohSkeletonLimb = 0x4F534C42,     // OSLB
    SohPath = 0x4F505448,             // OPTH
    SohCutscene = 0x4F435654,         // OCUT
    SohText = 0x4F545854,             // OTXT
    SohAudio = 0x4F415544,            // OAUD
    SohAudioSample = 0x4F534D50,      // OSMP
    SohAudioSoundFont = 0x4F534654,   // OSFT
    SohAudioSequence = 0x4F534551,    // OSEQ
    SohBackground = 0x4F424749,       // OBGI
};

inline ResourceType resourceTypeFromValue(int32_t value) {
    switch (value) {
        case static_cast<int32_t>(ResourceType::Archive):
        case static_cast<int32_t>(ResourceType::DisplayList):
        case static_cast<int32_t>(ResourceType::Vertex):
        case static_cast<int32_t>(ResourceType::Matrix):
        case static_cast<int32_t>(ResourceType::Array):
        case static_cast<int32_t>(ResourceType::Blob):
        case static_cast<int32_t>(ResourceType::Texture):
        case static_cast<int32_t>(ResourceType::SohAnimation):
        case static_cast<int32_t>(ResourceType::SohPlayerAnimation):
        case static_cast<int32_t>(ResourceType::SohRoom):
        case static_cast<int32_t>(ResourceType::SohCollisionHeader):
        case static_cast<int32_t>(ResourceType::SohSkeleton):
        case static_cast<int32_t>(ResourceType::SohSkeletonLimb):
        case static_cast<int32_t>(ResourceType::SohPath):
        case static_cast<int32_t>(ResourceType::SohCutscene):
        case static_cast<int32_t>(ResourceType::SohText):
        case static_cast<int32_t>(ResourceType::SohAudio):
        case static_cast<int32_t>(ResourceType::SohAudioSample):
        case static_cast<int32_t>(ResourceType::SohAudioSoundFont):
        case static_cast<int32_t>(ResourceType::SohAudioSequence):
        case static_cast<int32_t>(ResourceType::SohBackground):
            return static_cast<ResourceType>(value);
        default:
            return ResourceType::Unknown;
    }
}

} // namespace bitdeck
