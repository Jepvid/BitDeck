#pragma once

#include <optional>
#include <string>

namespace bitdeck {

// Looks up archivePath in MK64/Spaghettikart's baked CI-texture -> TLUT
// archive-path table (see tools/gen_mk64_tlut_map.py). Returns nullopt for
// any path not in the table, including every non-MK64 archive.
std::optional<std::string> mk64TlutArchivePathFor(const std::string& archivePath);

} // namespace bitdeck
