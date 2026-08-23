#include "sf64_tlut_map.h"

#include <iterator>
#include <unordered_map>
#include <utility>

namespace bitdeck {

namespace {

// Ground-truth CI-texture -> TLUT archive path pairs, derived from
// the SF64 decomp's assets/yaml/**/*.yaml ("tlut:" numeric offset fields).
const std::pair<const char*, const char*> kSf64TlutPairs[] = {
    {"ast_allies/aBillMarkTex", "ast_allies/aBillMarkTLUT"},
    {"ast_allies/aJamesMarkTex", "ast_allies/aJamesMarkTLUT"},
    {"ast_allies/aKattMarkTex", "ast_allies/aKattMarkTLUT"},
    {"ast_blue_marine/aBlueMarineLifeIconTex", "ast_blue_marine/aBlueMarineLifeIconTLUT"},
    {"ast_common/aBoostGaugeCoolTex", "ast_common/aBoostGaugeCoolTLUT"},
    {"ast_common/aBoostGaugeOverheatTex", "ast_common/aBoostGaugeOverheatTLUT"},
    {"ast_common/aFalcoMarkTex", "ast_common/aFalcoMarkTLUT"},
    {"ast_common/aIncomingMsgButtonTex", "ast_common/aIncomingMsgButtonTLUT"},
    {"ast_common/aIncomingMsgSignal1Tex", "ast_common/aIncomingMsgSignal1TLUT"},
    {"ast_common/aIncomingMsgSignal2Tex", "ast_common/aIncomingMsgSignal2TLUT"},
    {"ast_common/aIncomingMsgSignal3Tex", "ast_common/aIncomingMsgSignal3TLUT"},
    {"ast_common/aMsgWindowBgTex", "ast_common/aMsgWindowBgTLUT"},
    {"ast_common/aPeppyMarkTex", "ast_common/aPeppyMarkTLUT"},
    {"ast_common/aRadarFrameTex", "ast_common/aRadarFrameTLUT"},
    {"ast_common/aShieldGaugeTex", "ast_common/aShieldGaugeTLUT"},
    {"ast_common/aSlippyMarkTex", "ast_common/aSlippyMarkTLUT"},
    {"ast_common/aVsBombIconTex", "ast_common/aVsBombIconTLUT"},
    {"ast_common/aXTex", "ast_common/aXTLUT"},
    {"ast_landmaster/aLandmasterLifeIconTex", "ast_landmaster/aLandmasterLifeIconTLUT"},
    {"ast_macbeth/D_MA_60012A0", "ast_macbeth/D_MA_60013A0"},
    {"ast_macbeth/D_MA_6001578", "ast_macbeth/D_MA_6001978"},
    {"ast_macbeth/D_MA_6001B38", "ast_macbeth/D_MA_6001C38"},
    {"ast_macbeth/D_MA_6001C78", "ast_macbeth/D_MA_6002078"},
    {"ast_macbeth/D_MA_6002118", "ast_macbeth/D_MA_6002518"},
    {"ast_macbeth/D_MA_60026F8", "ast_macbeth/D_MA_60027F8"},
    {"ast_macbeth/D_MA_6002C20", "ast_macbeth/D_MA_6002E20"},
    {"ast_macbeth/D_MA_6002E60", "ast_macbeth/D_MA_6002F60"},
    {"ast_macbeth/D_MA_6002FF0", "ast_macbeth/D_MA_6003030"},
    {"ast_macbeth/D_MA_6003138", "ast_macbeth/D_MA_6003238"},
    {"ast_macbeth/D_MA_6003B58", "ast_macbeth/D_MA_6003F58"},
    {"ast_macbeth/D_MA_6003FC8", "ast_macbeth/D_MA_60043C8"},
    {"ast_macbeth/D_MA_6004640", "ast_macbeth/D_MA_6004A40"},
    {"ast_macbeth/D_MA_60050F8", "ast_macbeth/D_MA_60051F8"},
    {"ast_macbeth/D_MA_6005238", "ast_macbeth/D_MA_6005638"},
    {"ast_macbeth/D_MA_60069A8", "ast_macbeth/D_MA_6006AA8"},
    {"ast_macbeth/D_MA_6006BE8", "ast_macbeth/D_MA_6006FE8"},
    {"ast_macbeth/D_MA_6009AE0", "ast_macbeth/D_MA_6009BE0"},
    {"ast_macbeth/D_MA_6009D18", "ast_macbeth/D_MA_6009E18"},
    {"ast_macbeth/D_MA_6009FD8", "ast_macbeth/D_MA_600A0D8"},
    {"ast_macbeth/D_MA_600A2B8", "ast_macbeth/D_MA_600A3B8"},
    {"ast_macbeth/D_MA_600A598", "ast_macbeth/D_MA_600A698"},
    {"ast_macbeth/D_MA_600A898", "ast_macbeth/D_MA_600A998"},
    {"ast_macbeth/D_MA_600AB38", "ast_macbeth/D_MA_600AC38"},
    {"ast_macbeth/D_MA_600AE18", "ast_macbeth/D_MA_600AE98"},
    {"ast_macbeth/D_MA_600C2E0", "ast_macbeth/D_MA_600C3E0"},
    {"ast_macbeth/D_MA_600D878", "ast_macbeth/D_MA_600D978"},
    {"ast_macbeth/D_MA_600DF60", "ast_macbeth/D_MA_600E360"},
    {"ast_macbeth/D_MA_600E480", "ast_macbeth/D_MA_600E880"},
    {"ast_macbeth/D_MA_600EE38", "ast_macbeth/D_MA_600EF38"},
    {"ast_macbeth/D_MA_600EF98", "ast_macbeth/D_MA_600F018"},
    {"ast_macbeth/D_MA_600F028", "ast_macbeth/D_MA_600F128"},
    {"ast_macbeth/D_MA_6013F58", "ast_macbeth/D_MA_6014058"},
    {"ast_macbeth/D_MA_60186B8", "ast_macbeth/D_MA_6018AB8"},
    {"ast_macbeth/D_MA_6019028", "ast_macbeth/D_MA_6019128"},
    {"ast_macbeth/D_MA_601A5E8", "ast_macbeth/D_MA_601A6E8"},
    {"ast_macbeth/D_MA_601BB78", "ast_macbeth/D_MA_601BC78"},
    {"ast_macbeth/D_MA_601BD08", "ast_macbeth/D_MA_601BE08"},
    {"ast_macbeth/D_MA_6022B68", "ast_macbeth/D_MA_6022F68"},
    {"ast_macbeth/D_MA_60230C8", "ast_macbeth/D_MA_60231C8"},
    {"ast_macbeth/D_MA_6023228", "ast_macbeth/D_MA_6023328"},
    {"ast_macbeth/D_MA_6023388", "ast_macbeth/D_MA_6023788"},
    {"ast_macbeth/D_MA_6024230", "ast_macbeth/D_MA_6024630"},
    {"ast_macbeth/D_MA_6026C00", "ast_macbeth/D_MA_6027000"},
    {"ast_map/aMapArwingIconTex", "ast_map/aMapArwingIconTLUT"},
    {"ast_map/aMapXTex", "ast_map/aMapXTLUT"},
    {"ast_option/aNdTex", "ast_option/aNdTLUT"},
    {"ast_option/aRdTex", "ast_option/aRdTLUT"},
    {"ast_option/aSpeakerCenterTex", "ast_option/aSpeakerCenterTLUT"},
    {"ast_option/aSpeakerTex", "ast_option/aSpeakerTLUT"},
    {"ast_option/aStTex", "ast_option/aStTLUT"},
    {"ast_option/aThTex", "ast_option/aThTLUT"},
    {"ast_sector_z/aSzInvaderIIITex1", "ast_sector_z/aSzInvaderIIITex1TLUT"},
    {"ast_sector_z/aSzInvaderIIITex2", "ast_sector_z/aSzInvaderIIITex2TLUT"},
    {"ast_text/aDownWrenchTexture", "ast_text/aDownWrenchTLUT"},
    {"ast_title/aIntroStarfoxLogoTex", "ast_title/aIntroStarfoxLogoTLUT"},
    {"ast_ve1_boss/D_VE1_9002F30", "ast_ve1_boss/D_VE1_9003330"},
    {"ast_ve1_boss/D_VE1_9003490", "ast_ve1_boss/D_VE1_9003890"},
    {"ast_ve1_boss/D_VE1_90039F0", "ast_ve1_boss/D_VE1_9003DF0"},
    {"ast_ve1_boss/D_VE1_90123C0", "ast_ve1_boss/D_VE1_90125C0"},
    {"ast_versus/D_versus_3000000", "ast_versus/D_versus_3000080"},
    {"ast_versus/D_versus_30000A0", "ast_versus/D_versus_30000A0"},
    {"ast_versus/D_versus_3000140", "ast_versus/D_versus_30001C0"},
    {"ast_versus/D_versus_30001E0", "ast_versus/D_versus_3000380"},
    {"ast_versus/D_versus_30003A0", "ast_versus/D_versus_30004E0"},
    {"ast_versus/D_versus_3000510", "ast_versus/D_versus_30006A0"},
    {"ast_versus/D_versus_30006D0", "ast_versus/D_versus_3000810"},
    {"ast_versus/D_versus_3000840", "ast_versus/D_versus_30008E0"},
    {"ast_versus/D_versus_3000900", "ast_versus/D_versus_30009F0"},
    {"ast_versus/D_versus_3000A10", "ast_versus/D_versus_3000B00"},
    {"ast_versus/D_versus_3001420", "ast_versus/D_versus_3003E20"},
    {"ast_versus/D_versus_3004010", "ast_versus/D_versus_3004D58"},
    {"ast_versus/D_versus_3004F60", "ast_versus/D_versus_3005E38"},
    {"ast_versus/D_versus_3006040", "ast_versus/D_versus_3006A68"},
    {"ast_versus/D_versus_3006C60", "ast_versus/D_versus_3007500"},
    {"ast_versus/D_versus_30076C0", "ast_versus/D_versus_3008598"},
    {"ast_versus/D_versus_30087A0", "ast_versus/D_versus_3008DE0"},
    {"ast_versus/D_versus_3008EC0", "ast_versus/D_versus_30098C0"},
    {"ast_versus/D_versus_3009990", "ast_versus/D_versus_300A390"},
    {"ast_versus/D_versus_300A470", "ast_versus/D_versus_300B218"},
    {"ast_versus/D_versus_300B3F0", "ast_versus/D_versus_300C458"},
    {"ast_versus/D_versus_300C660", "ast_versus/D_versus_300D150"},
    {"ast_versus/D_versus_3013F50", "ast_versus/D_versus_3014350"},
    {"ast_versus/D_versus_3014510", "ast_versus/D_versus_3014550"},
    {"ast_versus/D_versus_3014590", "ast_versus/D_versus_3014690"},
    {"ast_versus/D_versus_302BF88", "ast_versus/D_versus_302C088"},
    {"ast_versus/D_versus_302C188", "ast_versus/D_versus_302C288"},
    {"ast_versus/D_versus_302C408", "ast_versus/D_versus_302C508"},
    {"ast_versus/D_versus_302C658", "ast_versus/D_versus_302C758"},
    {"ast_versus/D_versus_302C8E8", "ast_versus/D_versus_302C9E8"},
    {"ast_versus/D_versus_302CBF8", "ast_versus/D_versus_302CCF8"},
    {"ast_versus/D_versus_302CEF8", "ast_versus/D_versus_302CFF8"},
    {"ast_versus/aVsBoostGaugeCoolTex", "ast_versus/aVsBoostGaugeCoolTLUT"},
    {"ast_versus/aVsBoostGaugeOverheatTex", "ast_versus/aVsBoostGaugeOverheatTLUT"},
    {"ast_versus/aVsShieldGaugeTex", "ast_versus/aVsShieldGaugeTLUT"},
    {"ast_vs_menu/D_VS_MENU_7004050", "ast_vs_menu/D_VS_MENU_7004150"},
    {"ast_vs_menu/D_VS_MENU_70041F0", "ast_vs_menu/D_VS_MENU_70042F0"},
    {"ast_vs_menu/D_VS_MENU_7004360", "ast_vs_menu/D_VS_MENU_7004460"},
    {"ast_vs_menu/D_VS_MENU_70044D0", "ast_vs_menu/D_VS_MENU_7004990"},
    {"ast_vs_menu/D_VS_MENU_70051D0", "ast_vs_menu/D_VS_MENU_70055D0"},
    {"ast_vs_menu/D_VS_MENU_70124E8", "ast_vs_menu/D_VS_MENU_7012568"},
    {"ast_vs_menu/aVsCorneriaTex", "ast_vs_menu/aVsCorneriaTLUT"},
    {"ast_vs_menu/aVsFalcoNameTex", "ast_vs_menu/aVsFalcoNameTLUT"},
    {"ast_vs_menu/aVsFoxNameTex", "ast_vs_menu/aVsFoxNameTLUT"},
    {"ast_vs_menu/aVsHandicapFrameTex", "ast_vs_menu/aVsHandicapFrameTLUT"},
    {"ast_vs_menu/aVsKatinaTex", "ast_vs_menu/aVsKatinaTLUT"},
    {"ast_vs_menu/aVsPeppyNameTex", "ast_vs_menu/aVsPeppyNameTLUT"},
    {"ast_vs_menu/aVsSectorZTex", "ast_vs_menu/aVsSectorZTLUT"},
    {"ast_vs_menu/aVsSlippyNameTex", "ast_vs_menu/aVsSlippyNameTLUT"},
};

} // namespace

std::optional<std::string> sf64TlutArchivePathFor(const std::string& archivePath) {
    static const std::unordered_map<std::string, std::string> kMap = [] {
        std::unordered_map<std::string, std::string> map;
        map.reserve(std::size(kSf64TlutPairs));
        for (const auto& [ci, tlut] : kSf64TlutPairs) {
            map.emplace(ci, tlut);
        }
        return map;
    }();
    auto it = kMap.find(archivePath);
    if (it == kMap.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace bitdeck
