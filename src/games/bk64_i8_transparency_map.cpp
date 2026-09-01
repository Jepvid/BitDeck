#include "bk64_i8_transparency_map.h"

#include <unordered_set>

namespace bitdeck {

namespace {

const char* const kBk64TranslucentI8Textures[] = {
    "assets/sprite/ASSET_42A_UNNAMED_0_0",
    "assets/sprite/ASSET_42A_UNNAMED_10_0",
    "assets/sprite/ASSET_42A_UNNAMED_10_1",
    "assets/sprite/ASSET_42A_UNNAMED_11_0",
    "assets/sprite/ASSET_42A_UNNAMED_11_1",
    "assets/sprite/ASSET_42A_UNNAMED_12_0",
    "assets/sprite/ASSET_42A_UNNAMED_12_1",
    "assets/sprite/ASSET_42A_UNNAMED_13_0",
    "assets/sprite/ASSET_42A_UNNAMED_14_0",
    "assets/sprite/ASSET_42A_UNNAMED_15_0",
    "assets/sprite/ASSET_42A_UNNAMED_16_0",
    "assets/sprite/ASSET_42A_UNNAMED_17_0",
    "assets/sprite/ASSET_42A_UNNAMED_18_0",
    "assets/sprite/ASSET_42A_UNNAMED_1_0",
    "assets/sprite/ASSET_42A_UNNAMED_2_0",
    "assets/sprite/ASSET_42A_UNNAMED_3_0",
    "assets/sprite/ASSET_42A_UNNAMED_3_1",
    "assets/sprite/ASSET_42A_UNNAMED_4_0",
    "assets/sprite/ASSET_42A_UNNAMED_4_1",
    "assets/sprite/ASSET_42A_UNNAMED_5_0",
    "assets/sprite/ASSET_42A_UNNAMED_5_1",
    "assets/sprite/ASSET_42A_UNNAMED_6_0",
    "assets/sprite/ASSET_42A_UNNAMED_6_1",
    "assets/sprite/ASSET_42A_UNNAMED_7_0",
    "assets/sprite/ASSET_42A_UNNAMED_7_1",
    "assets/sprite/ASSET_42A_UNNAMED_8_0",
    "assets/sprite/ASSET_42A_UNNAMED_8_1",
    "assets/sprite/ASSET_42A_UNNAMED_9_0",
    "assets/sprite/ASSET_42A_UNNAMED_9_1",
    "assets/sprite/ASSET_42A_UNNAMED_9_2",
    "assets/sprite/ASSET_700_DUST_0_0",
    "assets/sprite/ASSET_700_DUST_10_0",
    "assets/sprite/ASSET_700_DUST_11_0",
    "assets/sprite/ASSET_700_DUST_12_0",
    "assets/sprite/ASSET_700_DUST_13_0",
    "assets/sprite/ASSET_700_DUST_14_0",
    "assets/sprite/ASSET_700_DUST_1_0",
    "assets/sprite/ASSET_700_DUST_2_0",
    "assets/sprite/ASSET_700_DUST_3_0",
    "assets/sprite/ASSET_700_DUST_4_0",
    "assets/sprite/ASSET_700_DUST_5_0",
    "assets/sprite/ASSET_700_DUST_6_0",
    "assets/sprite/ASSET_700_DUST_7_0",
    "assets/sprite/ASSET_700_DUST_8_0",
    "assets/sprite/ASSET_700_DUST_9_0",
    "assets/sprite/ASSET_702_UNNAMED_0_0",
    "assets/sprite/ASSET_702_UNNAMED_10_0",
    "assets/sprite/ASSET_702_UNNAMED_11_0",
    "assets/sprite/ASSET_702_UNNAMED_1_0",
    "assets/sprite/ASSET_702_UNNAMED_2_0",
    "assets/sprite/ASSET_702_UNNAMED_3_0",
    "assets/sprite/ASSET_702_UNNAMED_4_0",
    "assets/sprite/ASSET_702_UNNAMED_5_0",
    "assets/sprite/ASSET_702_UNNAMED_6_0",
    "assets/sprite/ASSET_702_UNNAMED_7_0",
    "assets/sprite/ASSET_702_UNNAMED_8_0",
    "assets/sprite/ASSET_702_UNNAMED_9_0",
    "assets/sprite/ASSET_70A_BUBBLE_1_0_0",
    "assets/sprite/ASSET_70B_BUBBLE_2_0_0",
    "assets/sprite/ASSET_70C_RIPPLE_0_0",
    "assets/sprite/ASSET_70C_RIPPLE_0_1",
    "assets/sprite/ASSET_70C_RIPPLE_0_2",
    "assets/sprite/ASSET_70D_SMOKE_1_0_0",
    "assets/sprite/ASSET_70D_SMOKE_1_10_0",
    "assets/sprite/ASSET_70D_SMOKE_1_11_0",
    "assets/sprite/ASSET_70D_SMOKE_1_12_0",
    "assets/sprite/ASSET_70D_SMOKE_1_13_0",
    "assets/sprite/ASSET_70D_SMOKE_1_14_0",
    "assets/sprite/ASSET_70D_SMOKE_1_15_0",
    "assets/sprite/ASSET_70D_SMOKE_1_16_0",
    "assets/sprite/ASSET_70D_SMOKE_1_17_0",
    "assets/sprite/ASSET_70D_SMOKE_1_18_0",
    "assets/sprite/ASSET_70D_SMOKE_1_19_0",
    "assets/sprite/ASSET_70D_SMOKE_1_1_0",
    "assets/sprite/ASSET_70D_SMOKE_1_2_0",
    "assets/sprite/ASSET_70D_SMOKE_1_3_0",
    "assets/sprite/ASSET_70D_SMOKE_1_4_0",
    "assets/sprite/ASSET_70D_SMOKE_1_5_0",
    "assets/sprite/ASSET_70D_SMOKE_1_6_0",
    "assets/sprite/ASSET_70D_SMOKE_1_7_0",
    "assets/sprite/ASSET_70D_SMOKE_1_8_0",
    "assets/sprite/ASSET_70D_SMOKE_1_9_0",
};

} // namespace

bool bk64I8TextureIsTranslucent(const std::string& archivePath) {
    static const std::unordered_set<std::string> kSet(
        std::begin(kBk64TranslucentI8Textures), std::end(kBk64TranslucentI8Textures));
    return kSet.count(archivePath) != 0;
}

} // namespace bitdeck
