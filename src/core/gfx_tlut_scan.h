#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace bitdeck {

// Result of scanning one display list's Gfx command stream for TLUT usage.
struct DisplayListTlutInfo {
    // Each found CI4/CI8 texture's resource-path CRC64, mapped to the TLUT
    // it was paired with.
    std::unordered_map<uint64_t, uint64_t> ciToTlut;

    // Every TLUT confirmed by a LoadTLUT/LoadTLUTCmd, including ones whose
    // paired CI texture couldn't be identified (e.g. a flipbook slot
    // referenced only by segment address, common for eye/mouth textures).
    std::vector<uint64_t> confirmedTluts;

    // For a CI texture referenced only by segment address (XML flipbook
    // slots -- see scanXmlDisplayListForTlutInfo), the confirmed TLUT that
    // preceded that specific segment. Segment 0x08000000 is conventionally
    // the eye flipbook, 0x09000000 the mouth flipbook; see
    // kEyeFlipbookSegment/kMouthFlipbookSegment.
    std::unordered_map<uint32_t, uint64_t> segmentToTlut;
};

constexpr uint32_t kEyeFlipbookSegment = 0x08000000;
constexpr uint32_t kMouthFlipbookSegment = 0x09000000;

enum class TextureBlendMode { Opaque, Translucent, Ambiguous };

// Result of scanning one display list's Gfx command stream for the render
// mode (SETOTHERMODE_L or RDPSETOTHERMODE, RDP render-mode field) active
// when each I/IA-format texture was drawn.
struct DisplayListTransparencyInfo {
    std::unordered_map<uint64_t, TextureBlendMode> textureToBlendMode;
};

// Scans one binary DisplayList (ODLT) resource's raw archive bytes for
// SETTIMG_OTR_HASH loads of I/IA-format textures, classified by whether the
// most recent SETOTHERMODE_L/RDPSETOTHERMODE render-mode word active at
// their draw command includes FORCE_BL (blend-with-framebuffer; every
// translucent N64 render mode sets it, every opaque one leaves it clear). A
// texture drawn only by a runtime-built Gfx array in game code, never an
// archive resource, isn't found here.
DisplayListTransparencyInfo scanDisplayListForTransparencyInfo(const std::vector<uint8_t>& resourceBytes);

// Scans one binary DisplayList resource's raw archive bytes for the render
// mode active at any draw command in it, independent of texture identity
// (no SETTIMG_OTR_HASH pairing, unlike scanDisplayListForTransparencyInfo).
// Returns nullopt if the DL has no draw command. See
// i8TextureIsTranslucent in texture_extraction.cpp.
std::optional<TextureBlendMode> scanDisplayListOwnBlendMode(const std::vector<uint8_t>& resourceBytes);

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
// unpaired.
DisplayListTlutInfo scanXmlDisplayListForTlutInfo(const std::vector<uint8_t>& resourceBytes);

} // namespace bitdeck
