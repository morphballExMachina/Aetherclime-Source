#include "Aetherclime/EnvironmentContextAudit.h"
#include "Aetherclime/ConditionQuery.h"
#include <cmath>
#include <cstring>
namespace aetherclime {
const char* EnvironmentContextName(const EnvironmentSnapshot& s) noexcept {
    if(!s.available)return "unavailable";
    if(!s.interior)return s.trustedRegion?"trusted-exterior":s.provisionalRegion?"provisional-exterior":"untrusted-exterior";
    if(!s.trustedRegion)return s.provisionalRegion?"provisional-interior":"untrusted-interior";
    return s.localAmbientIsModeledIndoor?"modeled-interior":"interior-without-ambient";
}
EnvironmentContextAudit AuditEnvironmentContext(const EnvironmentSnapshot& s) noexcept {
    EnvironmentContextAudit out;
    const auto check=[&](unsigned index,bool pass){if(!pass)out.failedMask|=(1u<<index);};
    AetherclimeEnvironmentSnapshotV1 v1{},zero1{};
    AetherclimeEnvironmentSnapshotV2 v2{},zero2{};
    const bool got1=ToPublicSnapshotV1(s,v1),got2=ToPublicSnapshotV2(s,v2);
    check(0,got1==s.available && got2==s.available &&
        (s.available ? v1.structSize==sizeof(v1)&&v1.schemaVersion==1&&v2.structSize==sizeof(v2)&&v2.schemaVersion==2&&v1.cellFormId!=0 :
         std::memcmp(&v1,&zero1,sizeof(v1))==0&&std::memcmp(&v2,&zero2,sizeof(v2))==0));
    check(1,std::memcmp(&v1,&v2.legacy,sizeof(v1))==0);
    check(2,!s.available || (!(s.trustedRegion&&s.provisionalRegion) &&
        static_cast<bool>(v1.flags&AETHERCLIME_ENV_TRUSTED_REGION)==s.trustedRegion &&
        static_cast<bool>(v1.flags&AETHERCLIME_ENV_OUTDOOR_AIR_UNVERIFIED)!=s.trustedRegion));
    const bool weatherOrigin=s.interior ? s.weatherProvenance==OutdoorWeatherProvenance::CachedVerifiedExterior :
        s.weatherProvenance==OutdoorWeatherProvenance::DirectExteriorSky;
    check(3,!s.available || !s.verifiedOutdoorWeatherFormId || (s.trustedRegion&&weatherOrigin));
    bool ambientOK=true;
    if(s.available) {
        if(s.localAmbientC) {
            ambientOK=std::isfinite(*s.localAmbientC)&&s.trustedRegion &&
                (s.interior ? s.localAmbientIsModeledIndoor&&s.building.knownHeatedInnRule :
                 !s.localAmbientIsModeledIndoor&&*s.localAmbientC==s.outdoorAirC);
        } else ambientOK=!s.localAmbientIsModeledIndoor && (s.interior||!s.trustedRegion);
    }
    check(4,ambientOK);
    check(5,!s.available || !s.interior || (!s.biome.available&&!s.previewAirC &&
        !(v2.flags&(AETHERCLIME_ENV2_BIOME_AVAILABLE|AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE))));
    const bool biome=!s.interior&&ValidBiomeIdentity(s.biome);
    check(6,!s.available || !s.previewAirC || (biome&&std::isfinite(*s.previewAirC)));
    check(7,!s.available || (s.date.year>=0&&s.date.month>=0&&s.date.month<12&&s.date.day>=1&&s.date.day<=40&&
        std::isfinite(s.date.hour)&&s.date.hour>=0&&s.date.hour<24));
    const auto known=[&](std::string_view rule,bool available){
        const auto r=EvaluateCondition(rule,s);
        return r.error==ConditionError::None && r.status==(available?ConditionStatus::True:ConditionStatus::Unknown);
    };
    check(8,known("outdoorAirC<0 | outdoorAirC>=0",s.available&&s.trustedRegion&&std::isfinite(s.outdoorAirC)));
    check(9,known("localAmbientC<0 | localAmbientC>=0",s.available&&s.localAmbientC&&std::isfinite(*s.localAmbientC)));
    check(10,known("biomeWeight(coastal)>=0",s.available&&biome&&s.biome.evidence!=BiomeEvidence::Nearby));
    check(11,known("weather=clear | weather!=clear",s.available&&s.trustedRegion&&weatherOrigin&&s.verifiedOutdoorWeatherFormId&&
        static_cast<unsigned>(s.weather)<static_cast<unsigned>(Weather::Unknown)));
    check(12,known("previewAirC<0 | previewAirC>=0",s.available&&biome&&s.previewAirC&&std::isfinite(*s.previewAirC)));
    return out;
}
} // namespace aetherclime
