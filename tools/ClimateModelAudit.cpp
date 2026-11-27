// Offline synthetic audit: never reads game data or claims real atlas coverage.
#include "Aetherclime/BiomeClimatePreview.h"
#include "Aetherclime/BiomeIdentity.h"
#include "Aetherclime/ConditionQuery.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
using namespace aetherclime;
namespace {
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool Near(double a,double b,double epsilon=1e-8){return std::abs(a-b)<epsilon;}
void ReadingChecks(const Config& cfg,const Sample& s,const Reading& r) {
    double sum=0;for(double w:r.season.weights){Check(std::isfinite(w)&&w>=0&&w<=1,"season weight range");sum+=w;}
    Check(Near(sum,1)&&r.season.progress>=0&&r.season.progress<=1,"season normalization/progress");
    Check(std::isfinite(r.airC)&&std::isfinite(r.feelsLikeC)&&std::isfinite(r.waterC),"non-finite reading");
    Check(Near(r.airC,r.seasonalBaselineC+r.diurnalOffsetC+r.weatherOffsetC+r.altitudeOffsetC+r.anomalyOffsetC),"component sum");
    Check(std::abs(r.anomalyOffsetC)<=cfg.anomalyAmplitudeC+1e-9,"anomaly bounds");
    const auto& profile=cfg.regions.at(s.region);
    Check(r.waterC>=(profile.saltWater?-1.8:0)&&r.waterC<=27,"water bounds");
}
void PublicationChecks(const Config& cfg,const Sample& s,const Reading& reading,const NativeAtlasResult& atlas,const BiomeClimatePreview& preview){
    EnvironmentSnapshot state;state.available=true;state.trustedRegion=true;
    state.local.cellFormId=42;state.date=s.date;state.regionKey=s.region;
    state.outdoorAirC=reading.airC;state.outdoorFeelsLikeC=reading.feelsLikeC;
    AetherclimeEnvironmentSnapshotV1 before{},after{};
    Check(ToPublicSnapshotV1(state,before),"V1 serialization");
    const auto identity=ResolveBiomeIdentity(cfg,atlas);
    AttachEnvironmentDetails(state,reading.season,identity,preview.previewAirC);
    AetherclimeEnvironmentSnapshotV2 v2{};
    Check(ToPublicSnapshotV2(state,v2)&&ToPublicSnapshotV1(state,after),"V2 serialization");
    Check(std::memcmp(&before,&after,sizeof(before))==0&&std::memcmp(&before,&v2.legacy,sizeof(before))==0,"preview changed published regional fields");
    Check(v2.legacy.outdoorAirC==reading.airC&&v2.previewAirC==preview.previewAirC,"publication model separation");
    Check(EvaluateCondition("outdoorAirC<0",state).status==(reading.airC<0?ConditionStatus::True:ConditionStatus::False),"regional rule model");
    state.trustedRegion=false;
    Check(EvaluateCondition("outdoorAirC<0",state).status==ConditionStatus::Unknown,"preview conferred regional trust");
    state.interior=true;AttachEnvironmentDetails(state,reading.season,identity,preview.previewAirC);
    Check(!state.previewAirC&& !state.biome.available,"interior inherited exterior biome");
}
struct Scenario {const char* label;const char* region;const char* biome;double y;};
constexpr Scenario scenarios[]{
    {"pine-falkreath","falkreath","pine-forest",100000},
    {"fall-rift","rift","fall-forest",100000},
    {"tundra-whiterun","whiterun","tundra",100000},
    {"reach","reach","reach",100000},
    {"marsh-hjaalmarch","hjaalmarch","marsh",100000},
    {"volcanic-eastmarch","eastmarch","volcanic-tundra",100000},
    {"snow-winterhold","winterhold","snow",100000},
    {"alpine-winterhold","winterhold","alpine",100000},
    {"coastal-south-haafingar","haafingar","coastal",52000},
    {"coastal-mid-pale","pale","coastal",113500},
    {"coastal-mid-winterhold","winterhold","coastal",113500},
    {"coastal-north-winterhold","winterhold","coastal",175000}
};
constexpr Weather weathers[]{Weather::Unknown,Weather::Clear,Weather::Cloudy,Weather::Rain,Weather::Storm,Weather::Snow,Weather::Blizzard};
constexpr double hours[]{0,6,12,18};
struct Stats {
    std::size_t n{};double legacyMin{1e9},legacyMax{-1e9},previewMin{1e9},previewMax{-1e9},deltaMin{1e9},deltaMax{-1e9},deltaSum{};
    void Add(double a,double b){++n;legacyMin=std::min(legacyMin,a);legacyMax=std::max(legacyMax,a);previewMin=std::min(previewMin,b);previewMax=std::max(previewMax,b);deltaMin=std::min(deltaMin,b-a);deltaMax=std::max(deltaMax,b-a);deltaSum+=b-a;}
};
void CalendarChecks() {
    for(int days:{28,31,40})for(int transition:{0,9,13}) {
        auto c=DefaultConfig();c.daysInMonth.fill(days);c.transitionDays=transition;
        Climate model(c);
        for(int month=0;month<12;++month) {
            Sample a;a.date={month,days,24-1.0/3600,201};
            Sample b=a;b.date={(month+1)%12,1,0,month==11?202:201};
            const auto left=model.Calculate(a),right=model.Calculate(b);
            if(transition>0 || c.months[month]==c.months[(month+1)%12])Check(std::abs(left.airC-right.airC)<.01,"calendar seam discontinuity");
        }
    }
    auto c=DefaultConfig();c.months.fill(Season::Summer);Climate summer(c);
    for(int m=0;m<12;++m){const auto s=summer.GetSeason({m,1,0,201});Check(s.weights[2]==1,"all-year season");}
    c.transitionDays=0;c.daysInMonth.fill(1);Climate shortCalendar(c);Check(shortCalendar.GetSeason({11,1,23.9,201}).weights[2]==1,"short custom calendar");
}
int Run(bool csv){
    auto cfg=DefaultConfig();cfg.biomeClimate.enabled=true;cfg.biomeClimate.spatialGradientEnabled=true;
    Climate climate(cfg);auto disabled=cfg;disabled.biomeClimate.enabled=false;disabled.biomeClimate.spatialGradientEnabled=false;Climate regionalOnly(disabled);
    std::size_t regionalSamples=0,comparisonSamples=0,publicationChecks=0;
    // Every configured regional profile, every day, four hours, all weather classes,
    // reference and 1000m relative elevation. Wind/depth exercise separate outputs.
    for(const auto& [key,profile]:cfg.regions)for(int month=0;month<12;++month)for(int day=1;day<=cfg.daysInMonth[month];++day)
      for(double hour:hours)for(auto weather:weathers)for(double elevation:{0.,1000.}) {
        Sample s;s.region=key;s.date={month,day,hour,201};s.weather=weather;s.elevationAboveReferenceM=elevation;s.windKmh=35;s.waterDepthM=50;
        const auto r=climate.Calculate(s);ReadingChecks(cfg,s,r);++regionalSamples;
        Check(r.airC==regionalOnly.Calculate(s).airC,"preview toggle changed regional air");
      }
    if(csv)std::cout<<"scenario,region,biome,syntheticYRaw,month,samples,regionalMinC,regionalMaxC,previewMinC,previewMaxC,deltaMinC,deltaMaxC,meanDeltaC\n";
    for(const auto& scenario:scenarios) {
        NativeAtlasResult atlas;atlas.biomeStatus=NativeBiomeStatus::Unique;atlas.biome=scenario.biome;
        ClimateWorldPoint position{0x3C,0,scenario.y,{}};
        for(int month=0;month<12;++month) {
            Stats stats;
            for(int day=1;day<=cfg.daysInMonth[month];++day)for(double hour:hours) {
                Sample base;base.region=scenario.region;base.date={month,day,hour,201};
                const auto baseline=CompareBiomeClimate(cfg,base,climate.Calculate(base),atlas,position);
                Check(baseline.available,"synthetic comparison missing");
                for(auto weather:weathers)for(double elevation:{0.,1000.}) {
                    auto s=base;s.weather=weather;s.elevationAboveReferenceM=elevation;
                    const auto r=climate.Calculate(s);const auto p=CompareBiomeClimate(cfg,s,r,atlas,position);
                    Check(p.available&&std::isfinite(p.previewAirC),"preview non-finite");
                    Check(Near(p.deltaC,baseline.deltaC),"preview lost shared weather/elevation corrections");
                    Check(Near(p.previewAirC-r.airC,p.deltaC),"comparison delta");
                    Check(Near(p.previewAirC-p.baselineC-p.diurnalC,r.weatherOffsetC+r.altitudeOffsetC+r.anomalyOffsetC),"hybrid residual retention");
                    stats.Add(r.airC,p.previewAirC);++comparisonSamples;
                    if(day==1&&hour==0&&weather==Weather::Unknown&&elevation==0){PublicationChecks(cfg,s,r,atlas,p);++publicationChecks;}
                }
            }
            if(csv)std::cout<<scenario.label<<','<<scenario.region<<','<<scenario.biome<<','<<scenario.y<<','<<month<<','<<stats.n<<','<<stats.legacyMin<<','<<stats.legacyMax<<','<<stats.previewMin<<','<<stats.previewMax<<','<<stats.deltaMin<<','<<stats.deltaMax<<','<<stats.deltaSum/static_cast<double>(stats.n)<<'\n';
        }
    }
    CalendarChecks();
    std::cerr<<"Climate model audit: passed | regionalSamples="<<regionalSamples<<" | comparisonSamples="<<comparisonSamples<<" | publicationChecks="<<publicationChecks<<" | synthetic=true | defaultsOnly=true | actualAtlasCoverageTested=false\n";
    return 0;
}
}
int main(int argc,char** argv){
    if(argc>2 || (argc==2&&std::string_view(argv[1])!="--csv")){std::cerr<<"Usage: AetherclimeClimateModelAudit [--csv]\n";return 2;}
    try {std::cout.imbue(std::locale::classic());std::cout<<std::fixed<<std::setprecision(4);return Run(argc==2);}
    catch(const std::exception& e){std::cerr<<"Climate model audit FAILED: "<<e.what()<<'\n';return 1;}
}
