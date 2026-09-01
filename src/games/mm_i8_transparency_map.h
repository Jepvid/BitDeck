#pragma once

#include <string>

namespace bitdeck {

// True if archivePath is a MM I8 texture confirmed safe for the
// 'Preview I8 transparency from brightness' toggle (see
// tools/gen_transparency_map.py) -- drawn with a blended render mode
// wherever this scan found its draw call, never an opaque one.
bool mmI8TextureIsTranslucent(const std::string& archivePath);

} // namespace bitdeck
