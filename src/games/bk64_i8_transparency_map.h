#pragma once

#include <string>

namespace bitdeck {

// True if archivePath is a BK64 I8 texture confirmed safe for the
// 'Preview I8 transparency from brightness' toggle (see
// tools/gen_bk64_transparency_map.py) -- its ASSET_<id> was referenced
// only alongside a translucent render mode wherever this scan found it.
bool bk64I8TextureIsTranslucent(const std::string& archivePath);

} // namespace bitdeck
