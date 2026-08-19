#include "game_conventions_registry.h"

#include "../games/bk64_conventions.h"

namespace bitdeck {

const std::vector<const GameTextureConventions*>& gameTextureConventions() {
    static const Bk64TextureConventions kBk64;
    static const std::vector<const GameTextureConventions*> kConventions = {&kBk64};
    return kConventions;
}

std::optional<TextureManifestEntry> resolveTextureEntry(const TextureManifestMap& manifest, const std::string& target) {
    auto it = manifest.find(target);
    if (it != manifest.end()) {
        return it->second;
    }
    for (const auto* convention : gameTextureConventions()) {
        auto entry = convention->resolveAdditiveEntry(manifest, target);
        if (entry.has_value()) {
            return entry;
        }
    }
    return std::nullopt;
}

} // namespace bitdeck
