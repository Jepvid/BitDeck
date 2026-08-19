#include "gfx_tlut_scan.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "crc64.h"
#include "endianness.h"

namespace bitdeck {

namespace {

constexpr size_t kHeaderSize = 0x40;

// Gfx opcodes (top byte of w0), fast3dex2 microcode profile values (matches
// compiled OOT/SoH archives).
constexpr uint8_t kOpSetTimgOtrHash = 0x20;
constexpr uint8_t kOpSetTimg = 0xFD; // plain SETTIMG: w1 is a raw segmented pointer, not an OTR hash
constexpr uint8_t kOpTri1 = 0x05;
constexpr uint8_t kOpTri2 = 0x06;
constexpr uint8_t kOpQuad = 0x07;
constexpr uint8_t kOpLoadTlut = 0xF0;
constexpr uint8_t kOpEndDl = 0xDF;

bool isDrawOpcode(uint8_t opcode) {
    return opcode == kOpTri1 || opcode == kOpTri2 || opcode == kOpQuad;
}

// 128-bit (two 8-byte-word) commands; the second word pair holds a CRC64 hash.
bool isExpandedOpcode(uint8_t opcode) {
    switch (opcode) {
        case 0x20: case 0x31: case 0x32: case 0x33: case 0x35: case 0x36: case 0x42:
            return true;
        default:
            return false;
    }
}

uint32_t readWord(const std::vector<uint8_t>& data, size_t offset, Endianness endianness) {
    uint32_t b0 = data[offset];
    uint32_t b1 = data[offset + 1];
    uint32_t b2 = data[offset + 2];
    uint32_t b3 = data[offset + 3];
    if (endianness == Endianness::Big) {
        return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    }
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

// True for a raw segment-address placeholder (e.g. ">0x08000000", a
// flipbook slot filled in by game code) rather than a real resource path --
// matches Shipwright's own ResourceFactoryXMLDisplayListV0 check exactly.
bool isSegmentPlaceholder(std::string_view path) {
    return path.size() >= 3 && path[0] == '>' && path[1] == '0' && (path[2] == 'x' || path[2] == 'X');
}

std::optional<uint32_t> parseSegmentPlaceholder(std::string_view path) {
    if (!isSegmentPlaceholder(path)) {
        return std::nullopt;
    }
    try {
        return static_cast<uint32_t>(std::stoul(std::string(path.substr(1)), nullptr, 16));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// The closing '>' of a self-closing tag starting at tagStart, skipping over
// any '>' that appears inside a quoted attribute value (e.g.
// Path=">0x08000000"), which a plain content.find('>') would stop at early.
size_t findTagEnd(std::string_view content, size_t tagStart) {
    bool inQuotes = false;
    for (size_t i = tagStart; i < content.size(); ++i) {
        if (content[i] == '"') {
            inQuotes = !inQuotes;
        } else if (content[i] == '>' && !inQuotes) {
            return i;
        }
    }
    return std::string_view::npos;
}

std::optional<std::string> extractAttr(std::string_view tag, std::string_view attrName) {
    std::string needle = std::string(attrName) + "=\"";
    size_t start = tag.find(needle);
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    start += needle.size();
    size_t end = tag.find('"', start);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string(tag.substr(start, end - start));
}

// Tracks every CI/flipbook texture set since the last draw command, then
// resolves them all against the TLUT active at that draw.
struct DrawBoundaryTracker {
    bool hasMostRecentTlut = false;
    uint64_t mostRecentTlutHash = 0;

    struct PendingTexture {
        bool isSegment;
        uint64_t hash;
        uint32_t segmentBase;
    };
    std::vector<PendingTexture> pendingSinceLastDraw;

    DisplayListTlutInfo info;

    void confirmTlut(uint64_t hash) {
        mostRecentTlutHash = hash;
        hasMostRecentTlut = true;
        info.confirmedTluts.push_back(hash);
    }

    void observeCi(uint64_t hash) {
        pendingSinceLastDraw.push_back({false, hash, 0});
    }

    void observeSegmentCi(uint32_t segmentBase) {
        pendingSinceLastDraw.push_back({true, 0, segmentBase});
    }

    void flushAtDrawBoundary() {
        if (hasMostRecentTlut) {
            for (const auto& texture : pendingSinceLastDraw) {
                if (texture.isSegment) {
                    info.segmentToTlut[texture.segmentBase] = mostRecentTlutHash;
                } else {
                    info.ciToTlut[texture.hash] = mostRecentTlutHash;
                }
            }
        }
        pendingSinceLastDraw.clear();
    }
};

} // namespace

DisplayListTlutInfo scanDisplayListForTlutInfo(const std::vector<uint8_t>& resourceBytes) {
    if (resourceBytes.size() < kHeaderSize + 1) {
        return {};
    }

    Endianness endianness = endiannessFromValue(resourceBytes[0]);

    size_t pos = kHeaderSize + 1; // header + ucode byte
    pos = (pos + 7) & ~static_cast<size_t>(7); // pad to 8-byte alignment

    DrawBoundaryTracker tracker;
    bool hasPendingTlut = false;
    uint64_t pendingTlutHash = 0;

    while (pos + 8 <= resourceBytes.size()) {
        uint32_t w0 = readWord(resourceBytes, pos, endianness);
        uint32_t w1 = readWord(resourceBytes, pos + 4, endianness);
        pos += 8;
        auto opcode = static_cast<uint8_t>(w0 >> 24);

        if (isExpandedOpcode(opcode)) {
            if (pos + 8 > resourceBytes.size()) {
                break;
            }
            uint32_t hashHi = readWord(resourceBytes, pos, endianness);
            uint32_t hashLo = readWord(resourceBytes, pos + 4, endianness);
            pos += 8;

            if (opcode == kOpSetTimgOtrHash) {
                uint32_t fmt = (w0 >> 21) & 0x7;
                uint32_t siz = (w0 >> 19) & 0x3;
                auto hash = (static_cast<uint64_t>(hashHi) << 32) | hashLo;

                if (fmt == 0 && siz == 2) { // RGBA16bpp: candidate TLUT load
                    pendingTlutHash = hash;
                    hasPendingTlut = true;
                } else if (fmt == 2) { // CI4/CI8
                    tracker.observeCi(hash);
                }
            }
        } else if (opcode == kOpSetTimg) {
            // A plain (unpatched) SETTIMG: w1 is the original N64 segmented
            // pointer as compiled, not an OTR hash -- this is how a decomp
            // archive's own flipbook slots (eye/mouth) look, unlike the XML
            // ">0x..." placeholder used by custom-model mods.
            uint32_t fmt = (w0 >> 21) & 0x7;
            uint32_t segmentBase = w1 & 0xFF000000;
            if (fmt == 2 && (segmentBase == kEyeFlipbookSegment || segmentBase == kMouthFlipbookSegment)) {
                tracker.observeSegmentCi(segmentBase);
            }
        } else if (opcode == kOpLoadTlut) {
            if (hasPendingTlut) {
                tracker.confirmTlut(pendingTlutHash);
                hasPendingTlut = false;
            }
        } else if (isDrawOpcode(opcode)) {
            tracker.flushAtDrawBoundary();
        } else if (opcode == kOpEndDl) {
            break;
        }
    }

    tracker.flushAtDrawBoundary();
    return tracker.info;
}

DisplayListTlutInfo scanXmlDisplayListForTlutInfo(const std::vector<uint8_t>& resourceBytes) {
    std::string_view content(reinterpret_cast<const char*>(resourceBytes.data()), resourceBytes.size());

    DrawBoundaryTracker tracker;
    bool hasPendingTlut = false;
    uint64_t pendingTlutHash = 0;

    size_t pos = 0;
    while (pos < content.size()) {
        size_t nextSetTimg = content.find("<SetTextureImage", pos);
        size_t nextLoadTlut = content.find("<LoadTLUTCmd", pos);
        // Matches both <Triangle1 and <Triangles2 -- either marks a draw.
        size_t nextTriangle = content.find("<Triangle", pos);

        size_t next = std::min({nextSetTimg, nextLoadTlut, nextTriangle});
        if (next == std::string_view::npos) {
            break;
        }

        if (next == nextTriangle) {
            tracker.flushAtDrawBoundary();
            pos = nextTriangle + 1;
            continue;
        }

        if (next == nextLoadTlut) {
            if (hasPendingTlut) {
                tracker.confirmTlut(pendingTlutHash);
                hasPendingTlut = false;
            }
            pos = nextLoadTlut + 1;
            continue;
        }

        size_t tagEnd = findTagEnd(content, nextSetTimg);
        if (tagEnd == std::string_view::npos) {
            break;
        }
        std::string_view tag = content.substr(nextSetTimg, tagEnd - nextSetTimg);
        pos = tagEnd + 1;

        auto path = extractAttr(tag, "Path");
        auto format = extractAttr(tag, "Format");
        auto size = extractAttr(tag, "Size");
        if (!path.has_value() || !format.has_value()) {
            continue;
        }

        if (*format == "G_IM_FMT_RGBA" && size.has_value() && *size == "G_IM_SIZ_16b" &&
            !isSegmentPlaceholder(*path)) {
            pendingTlutHash = crc64(*path);
            hasPendingTlut = true;
        } else if (*format == "G_IM_FMT_CI") {
            if (auto segment = parseSegmentPlaceholder(*path); segment.has_value()) {
                tracker.observeSegmentCi(*segment);
            } else {
                tracker.observeCi(crc64(*path));
            }
        }
    }

    tracker.flushAtDrawBoundary();
    return tracker.info;
}

} // namespace bitdeck
