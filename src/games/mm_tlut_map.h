#pragma once

#include <optional>
#include <string>

namespace bitdeck {

// Looks up archivePath in MM's baked CI-texture -> TLUT archive-path table
// (see tools/gen_mm_tlut_map.py). Returns nullopt for any path not in the
// table, including every non-MM archive.
std::optional<std::string> mmTlutArchivePathFor(const std::string& archivePath);

} // namespace bitdeck
