#include "mm_i8_transparency_map.h"

#include <unordered_set>

namespace bitdeck {

namespace {

const char* const kMmTranslucentI8Textures[] = {
    "objects/gameplay_keep/gEffDust1Tex",
    "objects/gameplay_keep/gEffDust2Tex",
    "objects/gameplay_keep/gEffDust3Tex",
    "objects/gameplay_keep/gEffDust4Tex",
    "objects/gameplay_keep/gEffDust5Tex",
    "objects/gameplay_keep/gEffDust6Tex",
    "objects/gameplay_keep/gEffDust7Tex",
    "objects/gameplay_keep/gEffDust8Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame10Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame1Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame2Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame3Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame4Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame5Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame6Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame7Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame8Tex",
    "objects/gameplay_keep/gEffEnemyDeathFlame9Tex",
    "objects/gameplay_keep/gEffWaterRippleTex",
    "objects/gameplay_keep/gEffWaterSplash1Tex",
    "objects/gameplay_keep/gEffWaterSplash2Tex",
    "objects/gameplay_keep/gEffWaterSplash3Tex",
    "objects/gameplay_keep/gEffWaterSplash4Tex",
    "objects/gameplay_keep/gEffWaterSplash5Tex",
    "objects/gameplay_keep/gEffWaterSplash6Tex",
    "objects/gameplay_keep/gEffWaterSplash7Tex",
    "objects/gameplay_keep/gEffWaterSplash8Tex",
    "objects/gameplay_keep/gameplay_keep_Tex_054F20",
    "objects/object_warp1/gWarpBossWarpActivationBeamTex",
    "objects/object_warp1/gWarpBossWarpGlowTex",
    "overlays/ovl_Oceff_Wipe7/sSongofHealingEffectTex",
};

} // namespace

bool mmI8TextureIsTranslucent(const std::string& archivePath) {
    static const std::unordered_set<std::string> kSet(
        std::begin(kMmTranslucentI8Textures), std::end(kMmTranslucentI8Textures));
    return kSet.count(archivePath) != 0;
}

} // namespace bitdeck
