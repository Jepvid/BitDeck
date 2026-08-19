#pragma once

#include <map>
#include <string>

#include "background_worker.h"
#include "staging_model.h"

namespace bitdeck {

// Packs every staged entry into a new .otr/.o2r archive at outputPath,
// reporting per-file progress via progress. Runs on whatever thread calls
// it (pair with runInBackground() to keep it off the GUI thread).
//
// CustomTexturesEntry paths are prefixed with "alt/" when prependAlt is set
// (matching Retro's per-generation toggle); other entry kinds ignore it.
void generateArchive(const std::map<std::string, StageEntry>& entries, const std::string& outputPath, bool compress,
                      bool prependAlt, TaskProgress& progress);

} // namespace bitdeck
