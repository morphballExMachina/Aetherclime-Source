#include "Aetherclime/BiomeClimatePreview.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace aetherclime;
namespace {
NativeRegionRecord R(unsigned id,unsigned world,std::string biome,double x,double w){
    return {id,world,"",false,{{{x,0},{x+w,0},{x+w,200000},{x,200000}}},"",biome,NativeBiomeRole::Broad};
}
bool eq(double a,double b,double tol=1e-8){return std::abs(a-b)<tol;}
}
int main(){
    auto cfg=DefaultConfig(); assert(!cfg.biomeClimate.spatialGradientEnabled);
    cfg.biomeClimate.enabled=true;cfg.anomalyAmplitudeC=0;
    Climate climate(cfg); Sample s;s.region="winterhold";s.date={6,15,15,201};
    auto legacy=climate.Calculate(s);
    NativeRegionAtlas atlas;atlas.Build({R(1,60,"coastal",0,10000),R(2,60,"pine-forest",8000,10000),R(3,61,"coastal",0,10000)},true);
    auto p=[&](double x,double y,unsigned world=60){ClimateWorldPoint pos{world,x,y,{}};return CompareBiomeClimate(cfg,s,legacy,atlas.Query(pos,true),pos);};
    assert(eq(p(5000,52000).baselineC,9));
    cfg.biomeClimate.spatialGradientEnabled=true;
    assert(eq(p(5000,52000).baselineC,16)&&eq(p(5000,175000).baselineC,6));
    assert(eq(p(5000,113500).baselineC,11)&&eq(p(5000,113500).northFraction,.5));
    assert(eq(p(5000,1000).baselineC,16)&&eq(p(5000,190000).baselineC,6));
    assert(eq(p(5000,175000,61).baselineC,9)&&!p(5000,175000,61).gradientApplied);
    assert(eq(p(15000,175000).baselineC,19)&&!p(15000,175000).gradientApplied);
    auto blend=p(9000,175000);assert(blend.available&&eq(blend.baselineC,12.5)&&eq(blend.gradientOffsetC,-1.5));
    // Approach both sides of overlap; an interior at the exact edge of the other contour remains available.
    for(double edge:{8000.,10000.}) {
        auto l=p(edge-1,100000),m=p(edge,100000),r=p(edge+1,100000);
        assert(l.available&&m.available&&r.available&&eq(l.baselineC,r.baselineC,.02)&&eq(m.baselineC,l.baselineC,.02));
    }
    // Season mixing and custom offsets are additive and don't change legacy output.
    auto mixed=legacy;mixed.season.weights={.25,0,.75,0};
    ClimateWorldPoint pos{60,5000,175000,{}};
    auto q=CompareBiomeClimate(cfg,s,mixed,atlas.Query(pos,true),pos);
    assert(eq(q.baselineC,.25*(-14)+.75*6)&&eq(q.gradientOffsetC,-3.25));
    assert(eq(climate.Calculate(s).airC,legacy.airC));
    cfg.biomeClimate.gradients["coastal"].northOffsetC[2]=1;
    assert(eq(p(5000,175000).baselineC,10));
    cfg.biomeClimate.gradientNorthYRaw=cfg.biomeClimate.gradientSouthYRaw;bool bad=false;try{Climate c(cfg);}catch(const std::invalid_argument&){bad=true;}assert(bad);
    cfg.biomeClimate.gradientNorthYRaw=175000;cfg.biomeClimate.gradients["coastal"].northOffsetC[2]=std::numeric_limits<double>::infinity();bad=false;try{Climate c(cfg);}catch(const std::invalid_argument&){bad=true;}assert(bad);
    cfg.biomeClimate.gradients["coastal"].northOffsetC[2]=-3;
    atlas.Build({R(1,60,"coastal",0,10000),R(2,60,"pine-forest",10000,10000)},true);
    assert(!p(10000,100000).available); // exact shared edge without interior: don't invent a boundary climate
}
