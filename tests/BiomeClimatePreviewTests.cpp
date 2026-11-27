#include "Aetherclime/BiomeClimatePreview.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <iomanip>
using namespace aetherclime;
NativeRegionRecord R(unsigned id,unsigned world,std::string biome,double x,double w,bool incomplete=false){
    return {id,world,"",incomplete,{{{x,0},{x+w,0},{x+w,10000},{x,10000}}},"",biome,NativeBiomeRole::Broad};
}
// Offline replay uses validated Alpha-38 data converted to a bounded numeric stream.
// Output is a standardized scenario, never a reconstructed historical temperature.
int Replay(bool grid=false){
    std::size_t count{}; if(!(std::cin>>count)||count>NativeRegionAtlas::maxRecords) return 2;
    std::vector<NativeRegionRecord> records;
    std::size_t totalVertices=0,totalPolygons=0;
    for(std::size_t i=0;i<count;++i){
        NativeRegionRecord r;unsigned role;std::size_t n;
        if(!(std::cin>>r.formId>>r.worldspace>>r.incomplete>>role>>std::quoted(r.biome)>>n)||role>3||n>NativeRegionAtlas::maxPolygons-totalPolygons) return 2;
        totalPolygons+=n;r.biomeRole=static_cast<NativeBiomeRole>(role);
        for(std::size_t j=0;j<n;++j){
            std::size_t vertices;if(!(std::cin>>vertices)||vertices>NativeRegionAtlas::maxPolygonVertices||vertices>NativeRegionAtlas::maxVertices-totalVertices) return 2;
            totalVertices+=vertices;RegionPolygon poly(vertices);
            for(auto& v:poly) if(!(std::cin>>v.x>>v.y)) return 2;
            r.polygons.push_back(std::move(poly));
        }
        records.push_back(std::move(r));
    }
    NativeRegionAtlas atlas;atlas.Build(std::move(records),true);if(!atlas.Stats().complete) return 2;
    auto cfg=DefaultConfig();cfg.biomeClimate.enabled=true;cfg.anomalyAmplitudeC=0;
    if(grid) {
        auto& bc=cfg.biomeClimate;
        std::size_t n{};
        if(!(std::cin>>bc.spatialGradientEnabled>>bc.gradientSouthYRaw>>bc.gradientNorthYRaw>>bc.maxGapRawUnits>>bc.gapSeparationRawUnits>>n)||n>32) return 2;
        bc.profiles.clear();bc.gradients.clear();
        for(std::size_t i=0;i<n;++i) {
            std::string key;BiomeClimateProfile p;BiomeNorthSouthGradient g;bool hasGradient;
            if(!(std::cin>>std::quoted(key))||bc.profiles.contains(key)||key.find_first_of(",\"\r\n")!=std::string::npos) return 2;
            for(auto& value:p.seasonalMeanC) if(!(std::cin>>value)) return 2;
            if(!(std::cin>>p.diurnalAmplitudeC>>hasGradient)) return 2;
            bc.profiles.emplace(key,p);
            if(hasGradient) {
                for(auto& value:g.southOffsetC) if(!(std::cin>>value)) return 2;
                for(auto& value:g.northOffsetC) if(!(std::cin>>value)) return 2;
                bc.gradients.emplace(key,g);
            }
        }
        try { Climate validated(cfg); } catch(const std::invalid_argument&) {return 2;}
        unsigned nx{},ny{};double minX{},maxX{},minY{},maxY{};std::uint32_t world{};
        if(!(std::cin>>world>>nx>>ny>>minX>>maxX>>minY>>maxY)||!world||!nx||!ny||nx>1000||ny>1000||static_cast<std::size_t>(nx)*ny>250000||
           !std::isfinite(minX)||!std::isfinite(maxX)||!std::isfinite(minY)||!std::isfinite(maxY)||
           minX>=maxX||minY>=maxY||std::abs(minX)>1e7||std::abs(maxX)>1e7||std::abs(minY)>1e7||std::abs(maxY)>1e7) return 2;
        std::cout<<"ix,iy,x,y,status,biomeA,biomeB,weightA,weightB,winterC,springC,summerC,autumnC\n"<<std::fixed<<std::setprecision(5);
        Sample s;s.date.hour=9;
        for(unsigned iy=0;iy<ny;++iy) for(unsigned ix=0;ix<nx;++ix) {
            ClimateWorldPoint pos{world,minX+(maxX-minX)*(ix+.5)/nx,minY+(maxY-minY)*(iy+.5)/ny,{}};
            const auto a=atlas.Query(pos,true);
            std::array<BiomeClimatePreview,4> values;
            for(unsigned k=0;k<4;++k) {
                Reading reference{};reference.season.weights.fill(0);reference.season.weights[k]=1;
                values[k]=CompareBiomeClimate(cfg,s,reference,a,pos);
            }
            const auto& v=values[0];
            std::cout<<ix<<','<<iy<<','<<pos.xRaw<<','<<pos.yRaw<<','<<v.status<<','<<v.biomeA<<','<<v.biomeB<<','<<v.weightA<<','<<v.weightB;
            for(const auto& value:values) {std::cout<<',';if(value.available) std::cout<<value.baselineC;}
            std::cout<<'\n';
        }
        return 0;
    }
    Climate climate(cfg);
    std::size_t samples;if(!(std::cin>>samples)||samples>10000) return 2;
    std::cout<<"label,geographicProfile,status,available,biomeA,biomeB,weightA,weightB,gapDistanceRaw,gapRunnerUpRaw,legacyAirC,previewAirC,deltaC\n"<<std::fixed<<std::setprecision(4);
    for(std::size_t i=0;i<samples;++i){
        std::string label;ClimateWorldPoint pos;Sample s;s.date={6,15,15,201};s.weather=Weather::Clear;
        if(!(std::cin>>std::quoted(label)>>pos.worldspaceFormId>>pos.xRaw>>pos.yRaw>>s.region)||label.find_first_of(",\"\r\n")!=std::string::npos||!cfg.regions.contains(s.region)) return 2;
        auto p=CompareBiomeClimate(cfg,s,climate.Calculate(s),atlas.Query(pos,true));
        std::cout<<label<<','<<s.region<<','<<p.status<<','<<p.available<<','<<p.biomeA<<','<<p.biomeB<<','<<p.weightA<<','<<p.weightB<<','<<p.gapDistanceRaw<<','<<p.gapRunnerUpRaw<<','<<p.legacyAirC<<','<<p.previewAirC<<','<<p.deltaC<<'\n';
    }
    return 0;
}
int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--replay") return Replay();
    if(argc==2&&std::string(argv[1])=="--seasonal-grid") return Replay(true);
    auto cfg=DefaultConfig(); assert(!cfg.biomeClimate.enabled && cfg.biomeClimate.profiles.size()==9);
    cfg.biomeClimate.enabled=true; cfg.anomalyAmplitudeC=0;
    Climate climate(cfg); Sample s; s.region="eastmarch";s.date={6,15,15,201};s.weather=Weather::Rain;s.elevationAboveReferenceM=500;
    auto legacy=climate.Calculate(s);
    NativeRegionAtlas atlas; atlas.Build({R(1,60,"pine-forest",0,10000),R(2,60,"volcanic-tundra",8000,10000),R(3,61,"snow",0,20000),R(4,0,"alpine",0,20000),R(5,60,"pine-forest",1000,1000)},true);
    auto query=[&](double x,double y=5000){return atlas.Query(ClimateWorldPoint{60,x,y,{}},true);};
    auto preview=[&](double x,double y=5000){return CompareBiomeClimate(cfg,s,legacy,query(x,y));};
    auto p=preview(5000);assert(p.available&&p.biomeA=="pine-forest"&&p.geographicProfile=="eastmarch");assert(std::abs(p.baselineC-19)<1e-8);assert(std::abs(p.previewAirC-(19+4+legacy.weatherOffsetC+legacy.altitudeOffsetC))<1e-8);
    auto q=preview(9000);assert(q.available&&q.biomeA=="pine-forest"&&q.biomeB=="volcanic-tundra"&&std::abs(q.weightA-.5)<1e-8);
    assert(preview(8100).weightA>.9);assert(preview(9900).weightB>.9);
    assert(preview(8000).available && preview(8000).biomeA=="pine-forest");
    assert(preview(-1000).available&&std::string(preview(-1000).status)=="nearby-biome-provisional");
    assert(!preview(-5000).available);
    assert(!CompareBiomeClimate(cfg,s,legacy,atlas.Query(std::nullopt,true)).available);
    assert(atlas.Query(ClimateWorldPoint{60,5000,5000,{} }).biomeEvidence.empty());
    assert(climate.Calculate(s).airC==legacy.airC);
    cfg.biomeClimate.enabled=false; assert(!preview(5000).available);cfg.biomeClimate.enabled=true;
    atlas.Build({R(1,60,"pine-forest",0,10000),R(2,60,"snow",12000,10000)},true);
    assert(!preview(11000).available);assert(std::string(preview(11000).status)=="gap-ambiguous");
    atlas.Build({R(1,60,"pine-forest",0,10000),R(2,60,"snow",0,10000,true)},true);assert(!preview(5000).available);
    atlas.Build({R(1,60,"pine-forest",0,10000),R(2,60,"snow",0,10000),R(3,60,"alpine",0,10000)},true);assert(!preview(5000).available);
    atlas.Build({R(1,60,"pine-forest",0,10000)},true);assert(!preview(-100).available);
    cfg.biomeClimate.profiles.erase("pine-forest");assert(!preview(1000).available);
    cfg.biomeClimate.maxGapRawUnits=std::numeric_limits<double>::infinity();bool rejected=false;try{Climate bad(cfg);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
}
