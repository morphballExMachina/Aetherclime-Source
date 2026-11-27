#include "Aetherclime/BiomeIdentity.h"
#include "Aetherclime/BiomeClimatePreview.h"
#include <cassert>
#include <cmath>
#include <limits>
using namespace aetherclime;
namespace {
NativeRegionRecord Record(unsigned id,std::string biome,double x,double width) {
    return {id,60,"",false,{{{x,0},{x+width,0},{x+width,200000},{x,200000}}},"",biome,NativeBiomeRole::Broad};
}
bool Eq(double a,double b) {return std::abs(a-b)<1e-9;}
}
int main() {
    auto cfg=DefaultConfig(); // preview disabled, identity still queryable
    NativeRegionAtlas atlas;
    atlas.Build({Record(1,"coastal",0,10000),Record(2,"snow",8000,10000)},true);
    auto get=[&](double x,unsigned world=60) {return ResolveBiomeIdentity(cfg,atlas.Query(ClimateWorldPoint{world,x,100000,{}},true));};
    auto a=get(5000);
    assert(a.available && a.evidence==BiomeEvidence::Unique && a.biomeA=="coastal" && a.weightA==1);
    assert(BiomeShare(a,"coastal")==1 && BiomeShare(a,"snow")==0);
    Sample s;s.region="winterhold";s.date={6,15,15,201};auto legacy=Climate(cfg).Calculate(s);
    ClimateWorldPoint pos{60,5000,100000,{}};
    assert(!CompareBiomeClimate(cfg,s,legacy,atlas.Query(pos,true),pos).available);
    cfg.biomeClimate.profiles.clear();
    assert(get(5000).available); // geometry identity does not require a temperature profile
    cfg.biomeClimate.enabled=true;
    assert(!CompareBiomeClimate(cfg,s,legacy,atlas.Query(pos,true),pos).available);
    a=get(8500);
    assert(a.available && a.evidence==BiomeEvidence::Overlap);
    assert(a.biomeA=="coastal" && a.biomeB=="snow" && Eq(a.weightA,.75) && Eq(a.weightB,.25));
    assert(BiomeShare(a,"snow")==.25);
    a=get(8000);assert(a.available && a.evidence==BiomeEvidence::OverlapEdge && a.biomeA=="coastal");
    a=get(10000);assert(a.available && a.evidence==BiomeEvidence::OverlapEdge && a.biomeA=="snow");
    assert(!get(5000,61).available);
    assert(!ResolveBiomeIdentity(cfg,atlas.Query(std::nullopt,true)).available);
    // Outside but clearly nearest to one label: keep provenance; predicates abstain.
    a=get(-1000);assert(a.available && a.evidence==BiomeEvidence::Nearby && a.gapDistanceRaw==1000);
    assert(!BiomeShare(a,"coastal").has_value());
    cfg.biomeClimate.maxGapRawUnits=0;assert(!get(-1000).available);
    cfg.biomeClimate.maxGapRawUnits=4096;
    assert(!get(-5000).available);
    atlas.Build({Record(1,"coastal",0,10000),Record(2,"snow",10000,10000)},true);
    assert(!get(10000).available); // shared edge without a positive-depth interior
    atlas.Build({Record(1,"coastal",0,10000),Record(2,"snow",0,10000),Record(3,"alpine",0,10000)},true);
    assert(!get(5000).available);
    atlas.Build({Record(1,"coastal",0,10000)},false);assert(!get(5000).available);
    // Defensive serialization/query validation of malformed owned evidence.
    BiomeIdentity b;b.available=true;b.biomeA="coastal";b.weightA=1;b.evidence=BiomeEvidence::Unique;
    assert(ValidBiomeIdentity(b));
    b.weightA=std::numeric_limits<double>::quiet_NaN();assert(!ValidBiomeIdentity(b));
    b.weightA=1;b.biomeB="snow";assert(!ValidBiomeIdentity(b));
    b.evidence=BiomeEvidence::Overlap;b.weightA=.7;b.weightB=.2;assert(!ValidBiomeIdentity(b));
    b.weightB=.3;assert(ValidBiomeIdentity(b));
    b.biomeB="coastal";assert(!ValidBiomeIdentity(b));
    b.biomeB="snow";b.biomeA=std::string(48,'x');assert(!ValidBiomeIdentity(b));
    b.biomeA="bad key";assert(!ValidBiomeIdentity(b));
    assert(std::string_view(BiomeEvidenceName(BiomeEvidence::Nearby))=="nearby");
}
