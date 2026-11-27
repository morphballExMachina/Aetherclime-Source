#include "Aetherclime/EnvironmentContextAudit.h"
#include "Aetherclime/ConditionQuery.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
using namespace aetherclime;
namespace {
EnvironmentSnapshotInput Input(bool interior,bool trusted){
    EnvironmentSnapshotInput in;in.hasCell=true;in.interior=interior;in.trustedRegion=trusted;
    in.regionKey="winterhold";in.date={7,17,9.5,201};in.outdoorAirC=-3;in.outdoorFeelsLikeC=-5;
    in.local=ResolveLocalIdentity({true,interior,42,interior?"The Frozen Hearth":"",{},"Winterhold","winterhold"});
    in.weather=Weather::Clear;return in;
}
BiomeIdentity Identity(unsigned mode){
    BiomeIdentity b;if(mode==0)return b;
    b.available=true;b.biomeA="coastal";b.weightA=1;b.weightB=0;
    b.evidence=mode==1?BiomeEvidence::Unique:BiomeEvidence::Nearby;
    b.status=mode==1?"unique-biome":"nearby-biome-provisional";return b;
}
void Good(const EnvironmentSnapshot& s){
    const auto audit=AuditEnvironmentContext(s);
    if(!audit.Passed())std::cerr<<EnvironmentContextName(s)<<" failedMask="<<audit.failedMask<<'\n';
    assert(audit.Passed());
}
void Fails(const EnvironmentSnapshot& s,unsigned index){assert(AuditEnvironmentContext(s).failedMask&(1u<<index));}
}
int main(){
    std::size_t cases=0;
    for(bool hasCell:{false,true})for(bool interior:{false,true})for(bool trusted:{false,true})
      for(bool provisional:{false,true})for(bool inn:{false,true})for(bool weatherId:{false,true})
      for(unsigned provenance=0;provenance<4;++provenance)for(unsigned ambient=0;ambient<4;++ambient) {
        auto in=Input(interior,trusted);in.hasCell=hasCell;in.provisionalRegion=provisional;
        in.building.knownHeatedInnRule=inn;in.weatherFormId=weatherId?99:0;
        in.weatherProvenance=static_cast<OutdoorWeatherProvenance>(provenance);
        if(ambient)in.modeledIndoorAmbientC=ambient==1?13.0:ambient==2?std::numeric_limits<double>::quiet_NaN():std::numeric_limits<double>::infinity();
        const auto initial=AssembleEnvironmentSnapshot(in);
        const bool expectedModel=hasCell&&interior&&trusted&&inn&&ambient==1;
        const bool expectedAmbient=hasCell&&((!interior&&trusted)||expectedModel);
        assert(initial.available==hasCell);
        assert(initial.localAmbientC.has_value()==expectedAmbient);
        assert(initial.localAmbientIsModeledIndoor==expectedModel);
        assert(initial.provisionalRegion==(hasCell&&!trusted&&provisional));
        const bool expectedWeather=hasCell&&trusted&&weatherId&&provenance==(interior?2u:1u);
        assert((initial.verifiedOutdoorWeatherFormId!=0)==expectedWeather);
        if(expectedAmbient)assert(*initial.localAmbientC==(expectedModel?13.0:-3.0));
        for(unsigned biome=0;biome<3;++biome)for(bool validSeason:{false,true})for(unsigned preview=0;preview<3;++preview){
            auto s=initial;SeasonState season;season.official=Season::Summer;season.weights={0,0,validSeason?1.:2.,0};
            const auto value=preview==0?std::optional<double>{}:std::optional<double>{preview==1?9.:std::numeric_limits<double>::infinity()};
            AttachEnvironmentDetails(s,season,Identity(biome),value);
            assert(s.biome.available==(hasCell&&!interior&&biome!=0));
            assert(s.previewAirC.has_value()==(hasCell&&!interior&&biome!=0&&preview==1));
            assert(s.season.has_value()==(hasCell&&validSeason));
            Good(s);++cases;
        }
    }
    assert(cases==18432);
    // Exact regression: stale inn metadata on an untrusted exterior must never
    // produce an indoor ambient value, nor may an untrusted interior publish it.
    auto in=Input(false,false);in.building.knownHeatedInnRule=true;in.modeledIndoorAmbientC=13;
    assert(!AssembleEnvironmentSnapshot(in).localAmbientC);
    in.interior=true;assert(!AssembleEnvironmentSnapshot(in).localAmbientC);
    // Simulate exterior -> modeled inn -> ordinary interior -> exterior -> no cell.
    in=Input(false,true);in.weatherProvenance=OutdoorWeatherProvenance::DirectExteriorSky;in.weatherFormId=99;
    auto exterior=AssembleEnvironmentSnapshot(in);AttachEnvironmentDetails(exterior,SeasonState{},Identity(1),9);Good(exterior);
    auto innInput=Input(true,true);innInput.building.knownHeatedInnRule=true;innInput.modeledIndoorAmbientC=13;
    innInput.weatherProvenance=OutdoorWeatherProvenance::CachedVerifiedExterior;innInput.weatherFormId=99;
    auto inside=AssembleEnvironmentSnapshot(innInput);AttachEnvironmentDetails(inside,SeasonState{},exterior.biome,exterior.previewAirC);Good(inside);
    assert(std::string_view(EnvironmentContextName(inside))=="modeled-interior" && !inside.previewAirC);
    auto ordinaryInput=innInput;ordinaryInput.building.knownHeatedInnRule=false;
    auto ordinary=AssembleEnvironmentSnapshot(ordinaryInput);Good(ordinary);assert(!ordinary.localAmbientC);
    assert(std::string_view(EnvironmentContextName(ordinary))=="interior-without-ambient");
    auto returned=AssembleEnvironmentSnapshot(in);Good(returned);assert(returned.localAmbientC==exterior.localAmbientC);
    in.hasCell=false;auto absent=AssembleEnvironmentSnapshot(in);Good(absent);assert(!absent.available);
    assert(exterior.previewAirC==9 && inside.localAmbientC==13); // old owned snapshots remain separate
    // Loading inside clears exterior observation cache in the adapter. No supplied
    // verified exterior observation means weather predicates stay unknown.
    innInput.weatherFormId=0;innInput.weatherProvenance=OutdoorWeatherProvenance::InteriorSkyUnverified;
    auto loadedInside=AssembleEnvironmentSnapshot(innInput);Good(loadedInside);
    assert(EvaluateCondition("weather=clear",loadedInside).status==ConditionStatus::Unknown);
    // Fault injection ensures checks detect broken contracts rather than just pass.
    auto bad=exterior;bad.local.cellFormId=0;Fails(bad,0);
    bad=exterior;bad.provisionalRegion=true;Fails(bad,2);
    bad=exterior;bad.weatherProvenance=OutdoorWeatherProvenance::CachedVerifiedExterior;Fails(bad,3);
    bad=exterior;bad.localAmbientIsModeledIndoor=true;Fails(bad,4);
    bad=inside;bad.biome=exterior.biome;bad.previewAirC=9;Fails(bad,5);
    bad=exterior;bad.biome={};Fails(bad,6);
    bad=exterior;bad.date.hour=24;Fails(bad,7);
    bad=inside;bad.building.knownHeatedInnRule=false;Fails(bad,4);
    // Empty captures remain explicitly unavailable: constants may still be true.
    assert(std::string_view(EnvironmentContextName(absent))=="unavailable");
    assert(EvaluateCondition("true",absent).status==ConditionStatus::True);
    assert(EvaluateCondition("outdoorAirC<0",absent).status==ConditionStatus::Unknown);
    std::cout<<"Environment context matrix: "<<cases<<" synthetic cases passed; transitions and fault injections passed\n";
}
