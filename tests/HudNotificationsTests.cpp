#include "Aetherclime/HudNotifications.h"
#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>
using namespace aetherclime;

namespace {
EnvironmentSnapshot Exterior(double c=4.0,Weather w=Weather::Clear) {
    EnvironmentSnapshotInput in;
    in.hasCell=true;in.trustedRegion=true;in.regionKey="winterhold";in.regionName="Winterhold";
    in.regionSource="location-display-name";in.local.cellFormId=1;in.local.name="Winterhold";
    in.local.certainty=LocalNameCertainty::NamedAreaRecord;in.weather=w;
    in.weatherFormId=42;in.weatherProvenance=OutdoorWeatherProvenance::DirectExteriorSky;
    in.outdoorAirC=c;in.outdoorFeelsLikeC=c;
    auto e=AssembleEnvironmentSnapshot(in);e.season=SeasonState{};return e;
}
EnvironmentSnapshot Interior(bool modeled) {
    EnvironmentSnapshotInput in;
    in.hasCell=true;in.interior=true;in.trustedRegion=true;in.regionKey="winterhold";in.regionName="Winterhold";
    in.regionSource="location-display-name";in.local.cellFormId=2;in.local.name="The Frozen Hearth";
    in.local.certainty=LocalNameCertainty::ActiveInteriorCell;in.building.knownHeatedInnRule=modeled;
    in.weather=Weather::Blizzard;in.weatherFormId=42;in.weatherProvenance=OutdoorWeatherProvenance::InteriorSkyUnverified;
    in.outdoorAirC=-12;in.outdoorFeelsLikeC=-15;if(modeled)in.modeledIndoorAmbientC=19;
    auto e=AssembleEnvironmentSnapshot(in);e.season=SeasonState{};return e;
}
std::optional<HudNotice> TakeHudSample(HudNotifications& h,const HudSettings& s,double t,const EnvironmentSnapshot& e,bool ready=true) {
    return h.BeginSample(s,t,ready)?h.Observe(s,e):std::nullopt;
}
bool Has(const HudNotice& n,std::string_view text) {return n.text.find(text)!=std::string::npos;}
}
int main() {
    HudSettings s;HudNotifications h;
    assert(s.mode==HudMode::Off&&ValidHudSettings(s));s.mode=HudMode::Minimal;
    for(auto mode:{HudMode::Off,HudMode::Minimal,HudMode::Detailed,HudMode::Debug})assert(ParseHudMode(HudModeName(mode))==mode);
    for(auto unit:{HudTemperatureUnit::Celsius,HudTemperatureUnit::Fahrenheit,HudTemperatureUnit::Both})assert(ParseHudTemperatureUnit(HudTemperatureUnitName(unit))==unit);
    assert(!ParseHudMode("verbose")&&!ParseHudTemperatureUnit("kelvin"));
    assert(HudMenuCloseTrigger("Sleep/Wait Menu")=="wait-sleep-close-deferred");
    assert(HudMenuCloseTrigger("Journal Menu")=="journal-close-deferred");
    assert(HudMenuCloseTrigger("InventoryMenu")=="inventory-close-deferred");
    assert(HudMenuCloseTrigger("Main Menu").empty()&&HudMenuCloseTrigger("HUD Menu").empty());
    auto invalid=s;invalid.cooldownSeconds=0;assert(!ValidHudSettings(invalid));
    invalid=s;invalid.loadGraceSeconds=std::numeric_limits<double>::quiet_NaN();assert(!ValidHudSettings(invalid));
    invalid=s;invalid.heartbeatSeconds=89;assert(!ValidHudSettings(invalid));
    invalid=s;invalid.temperatureDeltaC=std::numeric_limits<double>::infinity();assert(!ValidHudSettings(invalid));
    invalid=s;invalid.sampleIntervalSeconds=61;assert(!ValidHudSettings(invalid));
    auto config=DefaultConfig();config.hud=invalid;bool rejected=false;
    try{Climate bad(config);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    auto out=Exterior();
    auto off=s;off.mode=HudMode::Off;assert(!TakeHudSample(h,off,100,out));assert(h.Statistics().eligibleSamples==0);
    assert(!h.Observe(s,out));
    assert(!TakeHudSample(h,s,2,out));assert(!TakeHudSample(h,s,8,out,false));
    assert(h.Statistics().blockedGrace==1&&h.Statistics().blockedUi==1);
    auto initial=TakeHudSample(h,s,8,out);assert(initial&&initial->reason==HudReason::Initial);
    assert(Has(*initial,"Winter")&&Has(*initial,"Outdoor estimate 4 C")&&!Has(*initial,"Clear"));
    assert(!h.Observe(s,out));
    auto snowy=Exterior(0,Weather::Snow);
    assert(!TakeHudSample(h,s,20,snowy));assert(h.Statistics().blockedCooldown==1);
    // A transient suppressed state never queues; only fresh state matters.
    assert(!TakeHudSample(h,s,98,out));assert(!TakeHudSample(h,s,99,snowy));
    auto snow=TakeHudSample(h,s,100,snowy);assert(snow&&snow->reason==HudReason::Weather&&Has(*snow,"Snow"));
    auto warm=Exterior(15,Weather::Snow);
    assert(!TakeHudSample(h,s,190,warm));
    auto indoor=TakeHudSample(h,s,192,Interior(true));assert(indoor&&indoor->reason==HudReason::Context);
    assert(Has(*indoor,"Indoor model 19 C")&&!Has(*indoor,"Blizzard")&&!Has(*indoor,"-12"));
    auto unknown=TakeHudSample(h,s,282,Interior(false));assert(unknown&&Has(*unknown,"Indoor estimate unavailable")&&!Has(*unknown,"-12"));
    h.Reset();assert(h.Statistics().sent==0);assert(!TakeHudSample(h,s,0,out));assert(!TakeHudSample(h,s,8,EnvironmentSnapshot{}));
    assert(h.Statistics().unavailable==1);assert(!TakeHudSample(h,s,9,out));assert(TakeHudSample(h,s,10,out));
    assert(!TakeHudSample(h,s,9,out));assert(h.Statistics().invalidClock==1);
    h.Reset();s.mode=HudMode::Detailed;s.temperatureUnit=HudTemperatureUnit::Both;
    auto d=TakeHudSample(h,s,8,out);assert(d&&Has(*d,"4 C / 39 F")&&Has(*d,"Clear"));
    assert(!TakeHudSample(h,s,98,Exterior(6.5)));
    auto delta=TakeHudSample(h,s,100,Exterior(7));assert(delta&&delta->reason==HudReason::Temperature);
    auto other=Exterior(7);other.local.name="College of Winterhold";
    auto loc=TakeHudSample(h,s,190,other);assert(loc&&loc->reason==HudReason::Location&&Has(*loc,"College of Winterhold"));
    other.local.cellFormId=17;assert(!TakeHudSample(h,s,280,other));
    auto newRegion=other;newRegion.regionKey="pale";newRegion.regionName="The Pale";
    auto reg=TakeHudSample(h,s,282,newRegion);assert(reg&&reg->reason==HudReason::Region);
    auto newSeason=newRegion;newSeason.season=SeasonState{.official=Season::Summer,.weights={0,0,1,0}};
    auto season=TakeHudSample(h,s,372,newSeason);assert(season&&season->reason==HudReason::Season&&Has(*season,"Summer"));
    h.Reset();s.temperatureUnit=HudTemperatureUnit::Fahrenheit;
    auto fallback=out;fallback.trustedRegion=false;fallback.provisionalRegion=false;
    fallback.regionKey="temperate";fallback.regionName="Temperate Skyrim";fallback.outdoorAirC=37;fallback.localAmbientC.reset();
    fallback.local.certainty=LocalNameCertainty::RegionalOnly;fallback.local.name="Temperate Skyrim";
    auto f=TakeHudSample(h,s,8,fallback);assert(f&&Has(*f,"Outdoor estimate unavailable")&&!Has(*f,"Temperate Skyrim")&&!Has(*f,"99 F")&&!Has(*f,"Clear"));
    auto provisional=fallback;provisional.provisionalRegion=true;provisional.regionKey="eastmarch";provisional.regionName="Eastmarch";provisional.outdoorAirC=10;
    h.Reset();auto hidden=TakeHudSample(h,s,8,provisional);assert(hidden&&Has(*hidden,"unavailable")&&!Has(*hidden,"50 F"));
    h.Reset();s.showProvisionalEstimates=true;
    auto visible=TakeHudSample(h,s,8,provisional);assert(visible&&Has(*visible,"Near Eastmarch")&&Has(*visible,"Provisional regional estimate 50 F")&&!Has(*visible,"Clear"));
    auto unverified=out;unverified.verifiedOutdoorWeatherFormId=0;unverified.weatherProvenance=OutdoorWeatherProvenance::Unavailable;
    h.Reset();auto u=TakeHudSample(h,s,8,unverified);assert(u&&Has(*u,"weather unverified")&&!Has(*u,"Clear"));
    auto wrong=Interior(false);wrong.localAmbientC=std::numeric_limits<double>::infinity();wrong.localAmbientIsModeledIndoor=true;
    h.Reset();auto bad=TakeHudSample(h,s,8,wrong);assert(bad&&Has(*bad,"Indoor estimate unavailable"));
    h.Reset();s.heartbeatSeconds=180;s.showOnFirstObservation=false;s.mode=HudMode::Minimal;
    assert(!TakeHudSample(h,s,8,out));assert(h.Statistics().baselineOnly==1&&h.Statistics().sent==0);
    assert(!TakeHudSample(h,s,187,out));auto beat=TakeHudSample(h,s,189,out);assert(beat&&beat->reason==HudReason::Heartbeat);
    auto same=TakeHudSample(h,s,370,out);assert(same&&same->reason==HudReason::Heartbeat&&same->text==beat->text);
    // A hidden change in evidence produces the same formatted message; an enabled
    // heartbeat remains possible rather than being starved by that change.
    h.Reset();s.showOnFirstObservation=true;s.showProvisionalEstimates=false;
    assert(TakeHudSample(h,s,8,fallback));
    assert(!TakeHudSample(h,s,100,provisional));
    auto hiddenBeat=TakeHudSample(h,s,190,provisional);
    assert(!hiddenBeat); // identical unavailable heartbeat is noise
    h.Reset();s.heartbeatSeconds=0;s.showOnFirstObservation=true;s.mode=HudMode::Debug;s.temperatureUnit=HudTemperatureUnit::Celsius;
    auto joke=TakeHudSample(h,s,8,out);assert(joke&&joke->reason==HudReason::DebugInitialization&&Has(*joke,"Marshmallow reserves critically low."));
    assert(!TakeHudSample(h,s,20,out));out.previewAirC=11;
    auto debug=TakeHudSample(h,s,98,out);assert(debug&&debug->reason==HudReason::Initial&&Has(*debug,"Experimental preview 11 C")&&Has(*debug,"Outdoor estimate 4 C"));
    assert(!TakeHudSample(h,s,188,out));h.Reset();assert(TakeHudSample(h,s,8,out)->reason==HudReason::DebugInitialization);
    h.Reset();s.debugInitializationMessage=false;out.local.name=std::string(500,'x')+"\n";out.regionName=std::string(500,'r');out.regionSource=std::string(500,'g');
    auto bounded=TakeHudSample(h,s,8,out);assert(bounded&&bounded->reason==HudReason::Initial&&bounded->text.size()<=243&&bounded->text.find('\n')==std::string::npos);
    // Alpha-49 fallback estimates and capitalization do not grant weather trust.
    h.Reset();s.mode=HudMode::Minimal;s.cooldownSeconds=5;s.loadGraceSeconds=0;
    auto gap=fallback;gap.estimatedOutdoorAirC=8;
    gap.outdoorEstimateBasis=OutdoorEstimateBasis::BroadTamriel;
    auto gapNotice=TakeHudSample(h,s,0,gap);
    assert(gapNotice&&Has(*gapNotice,"Outdoor geographic estimate 8 C")&&!Has(*gapNotice,"Temperate Skyrim"));
    auto generic=Interior(false);
    generic.estimatedIndoorAmbientC=12;
    generic.indoorEstimateBasis=IndoorEstimateBasis::Generic;
    auto genericNotice=TakeHudSample(h,s,5,generic);
    assert(genericNotice&&Has(*genericNotice,"Indoor model (generic) 12 C"));
    h.Reset();s.mode=HudMode::Minimal;s.loadGraceSeconds=8;s.cooldownSeconds=90;
    auto lowercase=Exterior();lowercase.regionName="winterhold";
    auto cap=TakeHudSample(h,s,8,lowercase);
    assert(cap&&Has(*cap,"Winterhold")&&!Has(*cap,"| winterhold |"));
    // Long sessions must not silently stop displaying due to a diagnostics budget.
    h.Reset();s.mode=HudMode::Minimal;s.cooldownSeconds=5;s.loadGraceSeconds=0;s.heartbeatSeconds=0;
    for(int i=0;i<500;++i){auto n=TakeHudSample(h,s,i*5.0,i%2?Interior(true):Exterior());assert(n);}
    assert(h.Statistics().sent==500);
}
