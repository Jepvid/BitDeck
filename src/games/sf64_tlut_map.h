#pragma once

#include <optional>
#include <string>

namespace bitdeck {

// Looks up archivePath in SF64's baked CI-texture -> TLUT archive-path table
// (see tools/gen_sf64_tlut_map.py). Returns nullopt for any path not in the
// table, including every non-SF64 archive.
std::optional<std::string> sf64TlutArchivePathFor(const std::string& archivePath);

} // namespace bitdeck
