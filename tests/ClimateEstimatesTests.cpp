#include "Aetherclime/ClimateEstimates.h"
#include "Aetherclime/EnvironmentalSnapshot.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace aetherclime;
int main() {
    auto c=DefaultConfig();c.biomeClimate.enabled=true;
    Sample s;s.region="temperate";s.date.month=7;s.date.day=17;s.date.hour=14;
    const auto legacy=Climate(c).Calculate(s);
    const ClimateWorldPoint point{0x3C,3000,90000,0};
    auto known=EstimateOutdoorClimate(c,s,legacy,true,false,5.0,false,point);
    assert(known.airC && *known.airC==legacy.airC && known.regionTrusted);
    auto biome=EstimateOutdoorClimate(c,s,legacy,false,false,9.5,false,point);
    assert(biome.airC && *biome.airC==9.5 && biome.basis==OutdoorEstimateBasis::NativeBiome && !biome.regionTrusted);
    auto nearby=EstimateOutdoorClimate(c,s,legacy,false,true,9.8,true,point);
    assert(nearby.airC && nearby.basis==OutdoorEstimateBasis::NearbyBiome && !nearby.weatherVerified);
    auto provisional=EstimateOutdoorClimate(c,s,legacy,false,true,std::nullopt,false,point);
    assert(provisional.airC && *provisional.airC==legacy.airC && provisional.basis==OutdoorEstimateBasis::ProvisionalRegional);
    const auto broad=EstimateOutdoorClimate(c,s,legacy,false,false,std::nullopt,false,point);
    assert(broad.airC && broad.basis==OutdoorEstimateBasis::BroadTamriel && *broad.airC<legacy.airC);
    auto south=EstimateOutdoorClimate(c,s,legacy,false,false,std::nullopt,false,ClimateWorldPoint{0x3C,3000,-100000,0});
    auto north=EstimateOutdoorClimate(c,s,legacy,false,false,std::nullopt,false,ClimateWorldPoint{0x3C,3000,170000,0});
    assert(south.airC && north.airC && *south.airC>*north.airC);
    auto other=EstimateOutdoorClimate(c,s,legacy,false,false,9.0,false,ClimateWorldPoint{0x99,3000,90000,0});
    assert(other.airC && other.basis==OutdoorEstimateBasis::GenericWorldspace && *other.airC==legacy.airC);
    auto initial=EstimateOutdoorClimate(c,s,legacy,false,false,std::nullopt,false,std::nullopt);
    assert(initial.airC && initial.basis==OutdoorEstimateBasis::GenericWorldspace);
    auto invalid=EstimateOutdoorClimate(c,s,legacy,false,false,std::numeric_limits<double>::quiet_NaN(),false,point);
    assert(invalid.airC && invalid.basis==OutdoorEstimateBasis::BroadTamriel);
    // V1/V2 are unchanged even when additional V3 estimates exist.
    EnvironmentSnapshotInput input;
    input.hasCell=true;input.local.cellFormId=0x123;input.outdoorAirC=legacy.airC;
    input.outdoorFeelsLikeC=legacy.feelsLikeC;
    input.estimatedOutdoorAirC=9.5;input.outdoorEstimateBasis=OutdoorEstimateBasis::NativeBiome;
    auto e=AssembleEnvironmentSnapshot(input);
    AetherclimeEnvironmentSnapshotV1 v1{};
    AetherclimeEnvironmentSnapshotV2 v2{};
    AetherclimeEnvironmentSnapshotV3 v3{};
    assert(ToPublicSnapshotV1(e,v1)&&ToPublicSnapshotV2(e,v2)&&ToPublicSnapshotV3(e,v3));
    assert((v1.flags&AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE)==0);
    assert(v2.legacy.outdoorAirC==legacy.airC);
    assert(v3.legacy.legacy.outdoorAirC==legacy.airC);
    assert((v3.flags&AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE)!=0);
    assert(v3.outdoorEstimateC==9.5 && v3.outdoorEstimateBasis==3 && v3.schemaVersion==3);
    input.interior=true;input.estimatedIndoorAmbientC=15.0;input.indoorEstimateBasis=IndoorEstimateBasis::Generic;
    e=AssembleEnvironmentSnapshot(input);
    assert(ToPublicSnapshotV3(e,v3));
    assert((v3.flags&AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE)!=0);
    assert(v3.indoorEstimateBasis==3 && v3.indoorEstimateC==15 &&
           !(v3.legacy.legacy.flags&AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE));
    // Alpha-49.1: controlled reproduction of the College of Winterhold ->
    // Whistling Mine road. The exact in-game gap XY were NOT in the HUD log;
    // this is a bounded synthetic point outside the strict 24576-unit
    // field-sample radius, NOT a claim about the road's surveyed coordinates.
    // The secondary estimate must follow the nearby cold climate field rather
    // than jumping to the seasonally warm generic Tamriel fallback.
    auto snowy=s;
    snowy.weather=Weather::Snow;
    const auto genericSnow=Climate(c).Calculate(snowy);
    const auto& winterAnchor=c.geographicAnchors[0];
    const ClimateWorldPoint winterRoad{0x3C,winterAnchor.xRaw+30000.0,winterAnchor.yRaw,0};
    auto winterNearby=EstimateOutdoorClimate(c,snowy,genericSnow,false,false,
                                              std::nullopt,false,winterRoad);
    auto winterProfile=snowy;
    winterProfile.region="winterhold";
    const double winterExpected=Climate(c).Calculate(winterProfile).airC;
    assert(winterNearby.airC && winterNearby.basis==OutdoorEstimateBasis::NearbyClimateReference);
    assert(winterNearby.referenceLabel==winterAnchor.label &&
           winterNearby.referenceDistanceRawUnits==30000.0 &&
           winterNearby.referenceInfluence==1.0 &&
           winterNearby.contributingReferences==1);
    assert(std::abs(*winterNearby.airC-winterExpected)<0.001);
    assert(std::abs(*winterNearby.airC - (genericSnow.airC-6.0))>5.0);
    // Fade continuously, without declaring this reference a trusted hold.
    const ClimateWorldPoint winterShoulder{0x3C,winterAnchor.xRaw+45000.0,winterAnchor.yRaw,0};
    const ClimateWorldPoint winterOuter{0x3C,winterAnchor.xRaw+57344.0,winterAnchor.yRaw,0};
    auto shoulder=EstimateOutdoorClimate(c,snowy,genericSnow,false,false,std::nullopt,false,winterShoulder);
    auto outer=EstimateOutdoorClimate(c,snowy,genericSnow,false,false,std::nullopt,false,winterOuter);
    assert(shoulder.airC && outer.airC && shoulder.basis==OutdoorEstimateBasis::NearbyClimateReference);
    assert(shoulder.referenceInfluence>0 && shoulder.referenceInfluence<1);
    assert(outer.basis==OutdoorEstimateBasis::BroadTamriel);
    assert(*shoulder.airC < *outer.airC && *shoulder.airC > *winterNearby.airC);
    // Existing higher-tier evidence ALWAYS wins, and the reference shoulder
    // is disabled with geographic fallback or outside vanilla Tamriel.
    auto direct=EstimateOutdoorClimate(c,snowy,genericSnow,true,false,std::nullopt,false,winterRoad);
    auto native=EstimateOutdoorClimate(c,snowy,genericSnow,false,false,1.5,false,winterRoad);
    auto provisionalRoad=EstimateOutdoorClimate(c,snowy,genericSnow,false,true,std::nullopt,false,winterRoad);
    auto noAnchorConfig=c;noAnchorConfig.geographicFallbackEnabled=false;
    auto disabledReference=EstimateOutdoorClimate(noAnchorConfig,snowy,genericSnow,false,false,std::nullopt,false,winterRoad);
    auto differentWorld=EstimateOutdoorClimate(c,snowy,genericSnow,false,false,std::nullopt,false,
                                                ClimateWorldPoint{0x99,winterRoad.xRaw,winterRoad.yRaw,0});
    assert(direct.basis==OutdoorEstimateBasis::TrustedRegional);
    assert(native.basis==OutdoorEstimateBasis::NativeBiome);
    assert(provisionalRoad.basis==OutdoorEstimateBasis::ProvisionalRegional);
    assert(disabledReference.basis==OutdoorEstimateBasis::BroadTamriel);
    assert(differentWorld.basis==OutdoorEstimateBasis::GenericWorldspace);
    // V3 enum is additive; V1/V2 snapshot temps remain the legacy model.
    input.estimatedOutdoorAirC=*winterNearby.airC;
    input.outdoorEstimateBasis=OutdoorEstimateBasis::NearbyClimateReference;
    auto referenceSnapshot=AssembleEnvironmentSnapshot(input);
    assert(ToPublicSnapshotV3(referenceSnapshot,v3));
    assert(v3.outdoorEstimateBasis==7 && v3.legacy.legacy.outdoorAirC==legacy.airC);

    e.estimatedIndoorAmbientC=std::numeric_limits<double>::infinity();
    assert(ToPublicSnapshotV3(e,v3) && !(v3.flags&AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE));
    return 0;
}
