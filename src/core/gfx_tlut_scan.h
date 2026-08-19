#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace bitdeck {

// Result of scanning one display list's Gfx command stream for TLUT usage.
struct DisplayListTlutInfo {
    // Each found CI4/CI8 texture's resource-path CRC64, mapped to the TLUT
    // it was paired with (the TLUT active in TMEM at that point).
    std::unordered_map<uint64_t, uint64_t> ciToTlut;

    // Every TLUT actually confirmed by a LoadTLUT/LoadTLUTCmd, even ones
    // whose paired CI texture couldn't be identified (e.g. a flipbook slot
    // referenced only by segment address, common for eye/mouth textures).
    // Lets a caller fall back to "this texture's folder has exactly one
    // confirmed TLUT" when direct pairing finds nothing.
    std::vector<uint64_t> confirmedTluts;

    // For a CI texture referenced only by segment address (XML flipbook
    // slots -- see scanXmlDisplayListForTlutInfo), the confirmed TLUT that
    // preceded that specific segment. Segment 0x08000000 is conventionally
    // the eye flipbook, 0x09000000 the mouth flipbook; see kEyeFlipbookSegment
    // / kMouthFlipbookSegment. Disambiguates a folder with two confirmed
    // TLUTs (one per flipbook) when the plain folder-singleton fallback
    // can't.
    std::unordered_map<uint32_t, uint64_t> segmentToTlut;
};

constexpr uint32_t kEyeFlipbookSegment = 0x08000000;
constexpr uint32_t kMouthFlipbookSegment = 0x09000000;

// Scans one binary DisplayList (ODLT) resource's raw archive bytes, as
// produced by the standard zapd/decomp export pipeline. Pairing is tracked
// via SETTIMG_OTR_HASH + LOADTLUT, mirroring Shipwright's
// gfx_set_timg_otr_hash_handler_custom interpreter.
DisplayListTlutInfo scanDisplayListForTlutInfo(const std::vector<uint8_t>& resourceBytes);

// Scans one XML DisplayList resource (the human-readable format used by
// custom-model mods, e.g. exported "mat_..." material display lists), as
// read by Shipwright's ResourceFactoryXMLDisplayListV0. Textures are
// referenced by literal resource path here rather than a hash; a CI
// texture referenced only by raw segment address (e.g. Path=">0x08000000",
// a flipbook slot filled in by game code, not archive data) is left
// unpaired since its identity isn't recoverable from this display list.
DisplayListTlutInfo scanXmlDisplayListForTlutInfo(const std::vector<uint8_t>& resourceBytes);

} // namespace bitdeck
