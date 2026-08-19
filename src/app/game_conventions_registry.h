#pragma once

#include <optional>
#include <vector>

#include "../games/game_texture_conventions.h"

namespace bitdeck {

// Game ports whose extra texture conventions the replace-textures flow
// consults (BK64 today; add more games' GameTextureConventions here).
const std::vector<const GameTextureConventions*>& gameTextureConventions();

// Looks a target path up in the manifest directly; on a miss, lets each
// registered game's conventions try to resolve it as an additive path.
std::optional<TextureManifestEntry> resolveTextureEntry(const TextureManifestMap& manifest, const std::string& target);

} // namespace bitdeck
