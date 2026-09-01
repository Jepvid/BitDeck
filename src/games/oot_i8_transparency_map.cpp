#include "oot_i8_transparency_map.h"

#include <unordered_set>

namespace bitdeck {

namespace {

const char* const kOotTranslucentI8Textures[] = {
    "objects/gameplay_keep/gCircleGlowLTex",
    "objects/gameplay_keep/gCircleGlowRTex",
    "objects/gameplay_keep/gCircleGlowSLTex",
    "objects/gameplay_keep/gCircleGlowSRTex",
    "objects/gameplay_keep/gDust1Tex",
    "objects/gameplay_keep/gDust2Tex",
    "objects/gameplay_keep/gDust3Tex",
    "objects/gameplay_keep/gDust4Tex",
    "objects/gameplay_keep/gDust5Tex",
    "objects/gameplay_keep/gDust6Tex",
    "objects/gameplay_keep/gDust7Tex",
    "objects/gameplay_keep/gDust8Tex",
    "objects/gameplay_keep/gEffBubble1Tex",
    "objects/gameplay_keep/gEffBubble2Tex",
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
    "objects/gameplay_keep/gEffLightning1Tex",
    "objects/gameplay_keep/gEffLightning2Tex",
    "objects/gameplay_keep/gEffLightning3Tex",
    "objects/gameplay_keep/gEffLightning4Tex",
    "objects/gameplay_keep/gEffLightning5Tex",
    "objects/gameplay_keep/gEffLightning6Tex",
    "objects/gameplay_keep/gEffLightning7Tex",
    "objects/gameplay_keep/gEffLightning8Tex",
    "objects/gameplay_keep/gEffUnknown4Tex",
    "objects/gameplay_keep/gEffUnknown5Tex",
    "objects/gameplay_keep/gEffWaterRippleTex",
    "objects/gameplay_keep/gEffWaterSplash1Tex",
    "objects/gameplay_keep/gEffWaterSplash2Tex",
    "objects/gameplay_keep/gEffWaterSplash3Tex",
    "objects/gameplay_keep/gEffWaterSplash4Tex",
    "objects/gameplay_keep/gEffWaterSplash5Tex",
    "objects/gameplay_keep/gEffWaterSplash6Tex",
    "objects/gameplay_keep/gEffWaterSplash7Tex",
    "objects/gameplay_keep/gEffWaterSplash8Tex",
    "objects/gameplay_keep/gUnknownCircle6Tex",
    "objects/gameplay_keep/gUnknownEffBlureTex",
    "objects/object_bv/gBarinadeSparkBall1Tex",
    "objects/object_bv/gBarinadeSparkBall2Tex",
    "objects/object_bv/gBarinadeSparkBall3Tex",
    "objects/object_bv/gBarinadeSparkBall4Tex",
    "objects/object_bv/gBarinadeSparkBall5Tex",
    "objects/object_bv/gBarinadeSparkBall6Tex",
    "objects/object_bv/gBarinadeSparkBall7Tex",
    "objects/object_bv/gBarinadeSparkBall8Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark1Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark2Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark3Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark4Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark5Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark6Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark7Tex",
    "objects/object_spot02_objects/gEffSunGraveSpark8Tex",
    "overlays/ovl_Arrow_Light/s1Tex",
    "overlays/ovl_Arrow_Light/s2Tex",
};

} // namespace

bool ootI8TextureIsTranslucent(const std::string& archivePath) {
    static const std::unordered_set<std::string> kSet(
        std::begin(kOotTranslucentI8Textures), std::end(kOotTranslucentI8Textures));
    return kSet.count(archivePath) != 0;
}

} // namespace bitdeck
