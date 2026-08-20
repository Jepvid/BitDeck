#pragma once

#include <optional>
#include <string>

namespace bitdeck {

// Looks up archivePath in OOT's baked CI-texture -> TLUT archive-path table
// (see tools/gen_zapd_tlut_map.py). Returns nullopt for any path not in the
// table, including every non-OOT archive.
std::optional<std::string> ootTlutArchivePathFor(const std::string& archivePath);

} // namespace bitdeck
