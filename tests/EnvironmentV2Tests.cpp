#include "Aetherclime/EnvironmentalSnapshot.h"
#include "Aetherclime/ConditionQuery.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
using namespace aetherclime;
using S=ConditionStatus;
int main() {
    EnvironmentSnapshot s;s.available=true;s.trustedRegion=false;s.provisionalRegion=true;
    s.regionKey="winterhold";s.date={6,15,12,201};s.outdoorAirC=-3;s.outdoorFeelsLikeC=-4;
    BiomeIdentity b;b.available=true;b.biomeA="coastal";b.biomeB="snow";
    b.weightA=.75;b.weightB=.25;b.evidence=BiomeEvidence::Overlap;b.status="two-biome-overlap-preview";
    SeasonState season;season.official=Season::Summer;season.weights={0,.2,.8,0};
    AetherclimeEnvironmentSnapshotV1 baseline{};assert(ToPublicSnapshotV1(s,baseline));
    AttachEnvironmentDetails(s,season,b,8.2);
    AetherclimeEnvironmentSnapshotV2 out{};
    assert(ToPublicSnapshotV2(s,out));
    static_assert(std::is_standard_layout_v<decltype(out)> && std::is_trivially_copyable_v<decltype(out)>);
    assert(out.structSize==sizeof(out) && out.schemaVersion==2 && out.reserved==0);
    assert(std::memcmp(&out.legacy,&baseline,sizeof(baseline))==0); // byte-for-byte legacy contract
    assert(out.flags==(AETHERCLIME_ENV2_SEASON_AVAILABLE|AETHERCLIME_ENV2_BIOME_AVAILABLE|AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE));
    assert(out.officialSeason==2 && out.seasonWeights[1]==.2 && out.seasonWeights[2]==.8);
    assert(out.biomeEvidence==2 && out.biomeWeightA==.75 && out.biomeWeightB==.25);
    assert(std::string(out.biomeA)=="coastal" && std::string(out.biomeB)=="snow");
    assert(out.legacy.outdoorAirC==-3 && out.previewAirC==8.2);
    auto check=[&](std::string_view rule,S expected) {
        auto r=EvaluateCondition(rule,s);
        if(r.status!=expected) std::cerr<<rule<<" status="<<static_cast<int>(r.status)<<" error="<<static_cast<int>(r.error)<<'\n';
        assert(r.status==expected);
    };
    check("season=summer & month=6 & biome=coastal & biomeWeight(snow)>=0.25 & time<18:30",S::True);
    check("biome=snow",S::False);check("biome!=snow",S::True);
    check("biomeWeight(custom-biome)=0 & biomeAvailable=true & biomeEvidence=overlap",S::True);
    check("seasonWeight(spring)=0.2 & seasonWeight(summer)>0.5 & seasonWeight(winter)=0",S::True);
    check("previewAirC>0",S::True);check("outdoorAirC<0",S::Unknown); // biome cannot confer geography trust
    check("trustedRegion=false & provisionalRegion=true & biome=coastal",S::True);
    check("false & biomeWeight(snow)",S::Invalid);
    check("biomeWeight(snow)>1",S::False);
    for(auto rule:{"biomeWeight=0", "biomeWeight()=0", "biomeWeight(coastal,snow)=0", "biomeWeight(biomeWeight(snow))>0",
        "biomeWeight(snow)>1.1", "biomeWeight(snow)<-0.1", "seasonWeight(fall)>0", "biome>snow", "biomeEvidence=trusted",
        "seasonWeight(Summer)>0", "biomeWeight(coastal).x=0", "biomeWeight(coastal)>seasonWeight(summer)"}) check(rule,S::Invalid);
    // Threshold ties: both 50% biomes match, no arbitrary dominant-label winner.
    b.weightA=.5;b.weightB=.5;AttachEnvironmentDetails(s,season,b,{});
    check("biome=coastal & biome=snow",S::True);check("previewAirC>0",S::Unknown);
    assert(ToPublicSnapshotV2(s,out));assert(!(out.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE) && out.previewAirC==0);
    // Nearby is exposed only as explicit provisional data; negative predicates do not turn unknown into true.
    b.biomeB.clear();b.weightA=1;b.weightB=0;b.evidence=BiomeEvidence::Nearby;b.status="nearby-biome-provisional";
    AttachEnvironmentDetails(s,season,b,7);
    check("biomeAvailable=true & biomeEvidence=nearby",S::True);
    check("biome=coastal",S::Unknown);check("biome!=snow",S::Unknown);check("biomeWeight(coastal)>0",S::Unknown);
    check("biomeEvidence!=nearby & biome=coastal",S::False);check("previewAirC>0",S::True);
    assert(ToPublicSnapshotV2(s,out));assert(out.flags&AETHERCLIME_ENV2_BIOME_NEARBY);
    // Never attach exterior geometry to indoor XY, even if a caller supplied it.
    s.interior=true;AttachEnvironmentDetails(s,season,b,7);
    check("biomeAvailable=false & biomeEvidence=none",S::True);check("biome=coastal",S::Unknown);
    check("previewAirC>0",S::Unknown);check("seasonWeight(summer)>0",S::True);
    assert(ToPublicSnapshotV2(s,out));assert(out.flags==AETHERCLIME_ENV2_SEASON_AVAILABLE);
    assert(out.biomeA[0]==0 && out.biomeWeightA==0 && out.previewAirC==0);
    // Serializer and evaluator also defend direct manually filled states.
    s.biome=b;s.previewAirC=7;
    assert(ToPublicSnapshotV2(s,out));assert(out.flags==AETHERCLIME_ENV2_SEASON_AVAILABLE);
    check("biome=coastal",S::Unknown);check("previewAirC>0",S::Unknown);
    s.interior=false;season.weights={.5,0,.8,0};AttachEnvironmentDetails(s,season,b,{});
    check("season=summer",S::Unknown);check("seasonWeight(summer)>0",S::Unknown);
    assert(ToPublicSnapshotV2(s,out));assert(!(out.flags&AETHERCLIME_ENV2_SEASON_AVAILABLE));
    season.weights={0,0,1,0};b.available=false;b.status="atlas-disabled";AttachEnvironmentDetails(s,season,b,{});
    check("biomeAvailable=false & biomeEvidence=none",S::True);check("biomeWeight(snow)=0",S::Unknown);
    assert(ToPublicSnapshotV2(s,out));assert(std::string(out.biomeStatus)=="atlas-disabled");
    // Empty output on unavailable snapshot, including nested legacy and optional data.
    s.available=false;
    check("biomeAvailable=false",S::Unknown);
    check("true | biome=coastal",S::True);
    check("true | seasonWeight(typo)>0",S::Invalid);
    assert(!ToPublicSnapshotV2(s,out));
    const AetherclimeEnvironmentSnapshotV2 zero{};
    assert(std::memcmp(&out,&zero,sizeof(out))==0);
    // New fields work through the existing result ABI as well.
    s.available=true;s.interior=false;b.available=true;b.evidence=BiomeEvidence::Unique;
    b.biomeA="coastal";b.biomeB.clear();b.weightA=1;b.weightB=0;
    AttachEnvironmentDetails(s,season,b,8);
    AetherclimeConditionResultV1 r{};
    constexpr char rule[]="biome=coastal & seasonWeight(summer)=1";
    assert(WriteConditionResultV1(rule,sizeof(rule)-1,s,{},&r,sizeof(r))==1 && r.status==1);
    // Extension serialization never changes V1's existing fields/flags.
    AetherclimeEnvironmentSnapshotV1 current{};assert(ToPublicSnapshotV1(s,current));
    assert(std::memcmp(&current,&baseline,sizeof(current))==0);
    std::cout<<"V2: legacy compatibility, biome/season weights, provisional and indoor gates passed\n";
}
