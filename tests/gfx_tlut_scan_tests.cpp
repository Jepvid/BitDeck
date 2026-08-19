#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "core/crc64.h"
#include "core/gfx_tlut_scan.h"

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

void appendWord(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value >> 16));
    bytes.push_back(static_cast<uint8_t>(value >> 24));
}

// A little-endian ODLT resource: 64-byte header + ucode byte + padding, as
// read by Shipwright's ResourceFactoryBinaryDisplayListV0::ReadResource.
std::vector<uint8_t> makeDisplayListHeader() {
    std::vector<uint8_t> bytes;
    appendWord(bytes, 0);          // endianness = little
    appendWord(bytes, 0x4F444C54); // resourceType = ODLT
    appendWord(bytes, 0);          // gameVersion
    appendWord(bytes, 0xDEADBEEF); // magicID low
    appendWord(bytes, 0xDEADBEEF); // magicID high
    appendWord(bytes, 0);          // resourceVersion
    bytes.push_back(0);            // isCustom
    bytes.resize(0x40, 0);         // pad header to 64 bytes
    bytes.push_back(0);            // ucode byte
    bytes.resize(0x48, 0);         // pad to 8-byte alignment
    return bytes;
}

// A 0x20 G_SETTIMG_OTR_HASH command: 16 bytes across two Gfx words -- fmt
// (bits 21-23) and siz (bits 19-20) in the first word, the CRC64 hash split
// across the second word pair (high 32 bits, then low 32 bits).
void appendSetTimgOtrHash(std::vector<uint8_t>& bytes, uint32_t fmt, uint32_t siz, uint64_t hash) {
    uint32_t w0 = (0x20u << 24) | (fmt << 21) | (siz << 19);
    appendWord(bytes, w0);
    appendWord(bytes, 0);
    appendWord(bytes, static_cast<uint32_t>(hash >> 32));
    appendWord(bytes, static_cast<uint32_t>(hash));
}

void appendLoadTlut(std::vector<uint8_t>& bytes) {
    appendWord(bytes, 0xF0u << 24);
    appendWord(bytes, 0);
}

void appendUnrelatedCommand(std::vector<uint8_t>& bytes) {
    appendWord(bytes, 0xE7u << 24); // arbitrary plain (non-expanded) opcode
    appendWord(bytes, 0);
}

void appendEndDl(std::vector<uint8_t>& bytes) {
    appendWord(bytes, 0xDFu << 24);
    appendWord(bytes, 0);
}

// A G_TRI2 draw command (opcode 0x06) -- the boundary that resolves every
// texture set since the previous one against whichever TLUT is active now.
void appendTriangle(std::vector<uint8_t>& bytes) {
    appendWord(bytes, 0x06u << 24);
    appendWord(bytes, 0);
}

constexpr uint32_t kFmtRgba = 0;
constexpr uint32_t kFmtCi = 2;
constexpr uint32_t kSiz16b = 2;
constexpr uint32_t kSiz8b = 1;

std::vector<uint8_t> asBytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

void testTlutFollowedByMatchingCiTexture() {
    std::vector<uint8_t> bytes = makeDisplayListHeader();
    uint64_t tlutHash = 0x1111111122222222ULL;
    uint64_t ciHash = 0x3333333344444444ULL;

    appendSetTimgOtrHash(bytes, kFmtRgba, kSiz16b, tlutHash); // load the palette
    appendUnrelatedCommand(bytes);                            // TileSync/SetTile/... in between
    appendLoadTlut(bytes);                                    // confirms it as the active TLUT
    appendSetTimgOtrHash(bytes, kFmtCi, kSiz8b, ciHash);       // then a CI8 texture using it
    appendEndDl(bytes);

    auto info = bitdeck::scanDisplayListForTlutInfo(bytes);
    check(info.ciToTlut.count(ciHash) == 1 && info.ciToTlut.at(ciHash) == tlutHash,
          "binary: CI texture after a confirmed LOADTLUT is paired with that TLUT's hash");
    check(info.confirmedTluts.size() == 1 && info.confirmedTluts[0] == tlutHash,
          "binary: the loaded TLUT is recorded as confirmed");
}

void testCiTextureBeforeAnyTlutIsUnpaired() {
    std::vector<uint8_t> bytes = makeDisplayListHeader();
    uint64_t ciHash = 0x5555555566666666ULL;

    appendSetTimgOtrHash(bytes, kFmtCi, kSiz8b, ciHash); // no TLUT loaded yet
    appendEndDl(bytes);

    auto info = bitdeck::scanDisplayListForTlutInfo(bytes);
    check(info.ciToTlut.empty(), "binary: CI texture with no preceding LOADTLUT is left unpaired, not guessed");
}

void testRgba16WithoutLoadTlutIsNotTreatedAsPalette() {
    std::vector<uint8_t> bytes = makeDisplayListHeader();
    uint64_t rgbaHash = 0x7777777788888888ULL;
    uint64_t ciHash = 0x9999999900000000ULL;

    // A genuine RGBA16 texture load (not a TLUT: nothing loads it into TMEM
    // via LOADTLUT), followed by a CI texture that must NOT inherit it.
    appendSetTimgOtrHash(bytes, kFmtRgba, kSiz16b, rgbaHash);
    appendUnrelatedCommand(bytes); // e.g. LoadBlock, not LoadTLUT
    appendSetTimgOtrHash(bytes, kFmtCi, kSiz8b, ciHash);
    appendEndDl(bytes);

    auto info = bitdeck::scanDisplayListForTlutInfo(bytes);
    check(info.ciToTlut.empty(), "binary: an RGBA16 SETTIMG never confirmed by LOADTLUT doesn't get treated as an active palette");
}

void testTwoTlutsInOneDlDisambiguateByOrder() {
    std::vector<uint8_t> bytes = makeDisplayListHeader();
    uint64_t tlutA = 0xAAAAAAAAAAAAAAAAULL;
    uint64_t tlutB = 0xBBBBBBBBBBBBBBBBULL;
    uint64_t ciUsingA = 0xC000000000000001ULL;
    uint64_t ciUsingB = 0xC000000000000002ULL;

    appendSetTimgOtrHash(bytes, kFmtRgba, kSiz16b, tlutA);
    appendLoadTlut(bytes);
    appendSetTimgOtrHash(bytes, kFmtCi, kSiz8b, ciUsingA);
    appendTriangle(bytes); // draws ciUsingA against whatever TLUT is active now (A)
    appendSetTimgOtrHash(bytes, kFmtRgba, kSiz16b, tlutB);
    appendLoadTlut(bytes);
    appendSetTimgOtrHash(bytes, kFmtCi, kSiz8b, ciUsingB);
    appendEndDl(bytes);

    auto info = bitdeck::scanDisplayListForTlutInfo(bytes);
    check(info.ciToTlut.count(ciUsingA) == 1 && info.ciToTlut.at(ciUsingA) == tlutA,
          "binary: first CI texture (drawn before the second TLUT loads) paired with the first TLUT");
    check(info.ciToTlut.count(ciUsingB) == 1 && info.ciToTlut.at(ciUsingB) == tlutB,
          "binary: second CI texture (after the TLUT reload) paired with the second TLUT");
}

void testCiTextureLoadedBeforeItsOwnTlut() {
    // Mirrors gLinkAdultRightHandHoldingHookshotNearDL from the real OOT
    // decomp source: gsDPLoadTextureBlock loads the CI texture's pixels
    // first, gsDPLoadTLUT_pal256 loads its palette second -- the opposite
    // order from the "TLUT then CI" pattern most display lists use. A
    // scanner that only looks backward for "whichever TLUT is currently
    // active" would wrongly pair this texture with the *previous* group's
    // TLUT instead of its own.
    std::vector<uint8_t> bytes = makeDisplayListHeader();
    uint64_t previousGroupTlut = 0x1010101010101010ULL;
    uint64_t ciHash = 0x2020202020202020ULL;
    uint64_t ownTlut = 0x3030303030303030ULL;

    appendSetTimgOtrHash(bytes, kFmtRgba, kSiz16b, previousGroupTlut);
    appendLoadTlut(bytes);
    appendTriangle(bytes); // ends the previous texture group

    appendSetTimgOtrHash(bytes, kFmtCi, kSiz8b, ciHash); // texture pixels loaded first
    appendSetTimgOtrHash(bytes, kFmtRgba, kSiz16b, ownTlut);
    appendLoadTlut(bytes); // ...then its own palette, before this group's draw
    appendTriangle(bytes);
    appendEndDl(bytes);

    auto info = bitdeck::scanDisplayListForTlutInfo(bytes);
    check(info.ciToTlut.count(ciHash) == 1 && info.ciToTlut.at(ciHash) == ownTlut,
          "binary: a CI texture loaded before its own TLUT is paired with that TLUT, not the previous group's");
}

void testXmlDisplayListDirectPairing() {
    // Mirrors a real custom-equipment "mat_" DL: a real-path TLUT load,
    // confirmed by LoadTLUTCmd, then a real-path CI8 texture.
    std::string xml =
        "<DisplayList Version=\"0\">"
        "<SetTextureImage Path=\"objects/object_link_boy/object_link_boyTLUT_00CB40\" Format=\"G_IM_FMT_RGBA\" Size=\"G_IM_SIZ_16b\" Width=\"1\"/>"
        "<LoadTLUTCmd Tile=\"7\" Count=\"255\"/>"
        "<SetTextureImage Path=\"objects/object_link_boy/gLinkAdultSwordPommelTex\" Format=\"G_IM_FMT_CI\" Size=\"G_IM_SIZ_8b_LOAD_BLOCK\" Width=\"1\"/>"
        "<EndDisplayList/>"
        "</DisplayList>";

    auto info = bitdeck::scanXmlDisplayListForTlutInfo(asBytes(xml));
    uint64_t ciHash = bitdeck::crc64("objects/object_link_boy/gLinkAdultSwordPommelTex");
    uint64_t tlutHash = bitdeck::crc64("objects/object_link_boy/object_link_boyTLUT_00CB40");

    check(info.ciToTlut.count(ciHash) == 1 && info.ciToTlut.at(ciHash) == tlutHash,
          "xml: CI texture referenced by literal path is paired with its literal-path TLUT");
    check(info.confirmedTluts.size() == 1 && info.confirmedTluts[0] == tlutHash,
          "xml: the loaded TLUT is recorded as confirmed");
}

void testXmlFlipbookSegmentPlaceholderIsUnpaired() {
    // Mirrors a real eye-blink "mat_" DL: the TLUT is a real path, but the
    // CI texture is a flipbook slot (segment address, filled in by game
    // code) rather than a named resource -- can't be paired from this DL.
    std::string xml =
        "<DisplayList Version=\"0\">"
        "<SetTextureImage Path=\"objects/object_link_boy/gSalemAdultEyesTLUT\" Format=\"G_IM_FMT_RGBA\" Size=\"G_IM_SIZ_16b\" Width=\"1\"/>"
        "<LoadTLUTCmd Tile=\"7\" Count=\"255\"/>"
        "<SetTextureImage Path=\">0x08000000\" Format=\"G_IM_FMT_CI\" Size=\"G_IM_SIZ_8b_LOAD_BLOCK\" Width=\"1\"/>"
        "<EndDisplayList/>"
        "</DisplayList>";

    auto info = bitdeck::scanXmlDisplayListForTlutInfo(asBytes(xml));
    uint64_t tlutHash = bitdeck::crc64("objects/object_link_boy/gSalemAdultEyesTLUT");

    check(info.ciToTlut.empty(), "xml: a flipbook (segment-address) CI reference is left unpaired, not guessed");
    check(info.confirmedTluts.size() == 1 && info.confirmedTluts[0] == tlutHash,
          "xml: the TLUT is still recorded as confirmed even though its CI texture couldn't be identified -- "
          "lets a caller fall back to \"this folder's one confirmed TLUT\" for flipbook slots");
}

void testXmlSegmentPlaceholderRecordsPerSegmentTlut() {
    // Two flipbook slots (eye then mouth) in the same DL, each behind its
    // own confirmed TLUT -- exercises DisplayListTlutInfo::segmentToTlut,
    // which lets a caller disambiguate a folder with two confirmed TLUTs by
    // matching a texture's own name ("Eye"/"Mouth") to the right segment.
    std::string xml =
        "<DisplayList Version=\"0\">"
        "<SetTextureImage Path=\"objects/object_link_boy/gSalemAdultEyesTLUT\" Format=\"G_IM_FMT_RGBA\" Size=\"G_IM_SIZ_16b\" Width=\"1\"/>"
        "<LoadTLUTCmd Tile=\"7\" Count=\"255\"/>"
        "<SetTextureImage Path=\">0x08000000\" Format=\"G_IM_FMT_CI\" Size=\"G_IM_SIZ_8b_LOAD_BLOCK\" Width=\"1\"/>"
        "<Triangle1 V00=\"0\" V01=\"1\" V02=\"2\"/>" // draws the eye slot against the TLUT active now
        "<SetTextureImage Path=\"objects/object_link_boy/gSalemAdultMouthTLUT\" Format=\"G_IM_FMT_RGBA\" Size=\"G_IM_SIZ_16b\" Width=\"1\"/>"
        "<LoadTLUTCmd Tile=\"7\" Count=\"255\"/>"
        "<SetTextureImage Path=\">0x09000000\" Format=\"G_IM_FMT_CI\" Size=\"G_IM_SIZ_8b_LOAD_BLOCK\" Width=\"1\"/>"
        "<EndDisplayList/>"
        "</DisplayList>";

    auto info = bitdeck::scanXmlDisplayListForTlutInfo(asBytes(xml));
    uint64_t eyeTlutHash = bitdeck::crc64("objects/object_link_boy/gSalemAdultEyesTLUT");
    uint64_t mouthTlutHash = bitdeck::crc64("objects/object_link_boy/gSalemAdultMouthTLUT");

    check(info.ciToTlut.empty(), "xml: both flipbook slots stay unpaired directly (segment address, not a name)");
    check(info.segmentToTlut.count(bitdeck::kEyeFlipbookSegment) == 1 &&
              info.segmentToTlut.at(bitdeck::kEyeFlipbookSegment) == eyeTlutHash,
          "xml: eye segment (0x08000000) recorded against the TLUT confirmed just before it");
    check(info.segmentToTlut.count(bitdeck::kMouthFlipbookSegment) == 1 &&
              info.segmentToTlut.at(bitdeck::kMouthFlipbookSegment) == mouthTlutHash,
          "xml: mouth segment (0x09000000) recorded against its own TLUT, not the eye one");
}

void testCrc64MatchesShipwrightKnownVectors() {
    // Cross-checked against Shipwright's own CRC64() for these exact strings.
    check(bitdeck::crc64("") == 0xffffffffffffffffULL, "crc64(\"\") matches Shipwright's CRC64");
    check(bitdeck::crc64("objects/object_link_child/gLinkChildHandTLUT") == 0x30fce263f2b778feULL,
          "crc64() of a real archive path matches Shipwright's CRC64");
}

} // namespace

int main() {
    testTlutFollowedByMatchingCiTexture();
    testCiTextureBeforeAnyTlutIsUnpaired();
    testRgba16WithoutLoadTlutIsNotTreatedAsPalette();
    testTwoTlutsInOneDlDisambiguateByOrder();
    testCiTextureLoadedBeforeItsOwnTlut();
    testXmlDisplayListDirectPairing();
    testXmlFlipbookSegmentPlaceholderIsUnpaired();
    testXmlSegmentPlaceholderRecordsPerSegmentTlut();
    testCrc64MatchesShipwrightKnownVectors();

    if (g_failures > 0) {
        std::printf("\n%d check(s) failed.\n", g_failures);
        return 1;
    }
    std::printf("\nAll checks passed.\n");
    return 0;
}
