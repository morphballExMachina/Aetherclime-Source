#include "Aetherclime/NativeRegionAtlas.h"
#include <cassert>
#include <limits>
#include <stdexcept>
using namespace aetherclime;
using S=NativeAtlasStatus;
RegionPolygon Box(double x,double y,double w=10){return {{x,y},{x+w,y},{x+w,y+w},{x,y+w}};}
ClimateWorldPoint P(double x,double y,std::uint32_t world=60){return {world,x,y,{}};}
int main(){
    NativeRegionAtlas atlas;
    assert(atlas.Query(P(5,5)).status==S::Unavailable);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0),Box(20,20)}},{2,60,"",false,{Box(0,0,40)}},{3,61,"rift",false,{Box(0,0)}}},true);
    assert(atlas.Stats().complete && atlas.Stats().records==3 && atlas.Stats().vertices==16);
    auto r=atlas.Query(P(5,5)); assert(r.status==S::UniqueProfile&&r.profile=="eastmarch"&&r.unmappedContaining==1&&r.unmappedIds[0]==2);
    assert(atlas.Query(P(25,25)).profile=="eastmarch");
    assert(atlas.Query(P(15,15)).status==S::UnmappedCoverage);
    assert(atlas.Query(P(0,5)).status==S::Boundary);
    assert(atlas.Query(P(100,100)).status==S::NoCoverage);
    assert(atlas.Query(P(5,5,61)).profile=="rift");
    assert(atlas.Query(std::nullopt).status==S::NoExteriorPosition);
    assert(atlas.Query(P(std::numeric_limits<double>::quiet_NaN(),5)).status==S::NoExteriorPosition);
    using C=ClimateRegionSource;
    ClimateRegionResolution direct; direct.source=C::FallbackNoEvidence; direct.regionKey="temperate";
    auto effective=direct;
    auto d=CompareNativeShadow(direct,effective,r); assert(d.profile=="eastmarch"&&d.wouldReplaceFallback);
    NativeShadowCoverage coverage; coverage.Observe(r,d,effective); assert(coverage.samples==1&&coverage.unique==1&&coverage.fallbackWouldReplace==1&&coverage.differentProfile==1);
    effective.source=C::ProvisionalRecentExterior; effective.regionKey="eastmarch";
    d=CompareNativeShadow(direct,effective,r); assert(d.profile=="eastmarch"&&!d.wouldReplaceFallback);
    direct.source=C::ProvisionalAtlasZone; direct.regionKey="temperate"; effective=direct;
    d=CompareNativeShadow(direct,effective,r); assert(d.profile=="eastmarch"&&!d.wouldReplaceFallback);
    direct.source=C::LocationDisplayName; direct.regionKey="rift"; effective=direct;
    d=CompareNativeShadow(direct,effective,r); assert(d.profile=="rift"&&!d.wouldReplaceFallback);
    direct.source=C::GeographicAnchorProximity; effective=direct; assert(CompareNativeShadow(direct,effective,r).profile=="rift");
    direct.source=C::FallbackAmbiguous; effective=direct; assert(CompareNativeShadow(direct,effective,r).profile=="rift");
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}},{2,60,"rift",false,{Box(2,2)}}},true);
    assert(atlas.Query(P(5,5)).status==S::ConflictingProfiles);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}},{2,60,"eastmarch",false,{Box(2,2)}}},true);
    assert(atlas.Query(P(5,5)).mappedContaining==2&&atlas.Query(P(5,5)).status==S::UniqueProfile);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}},{2,60,"rift",true,{}}},true);
    assert(atlas.Query(P(5,5)).status==S::IncompleteMappedGeometry);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}},{2,61,"rift",true,{}}},true);
    assert(atlas.Query(P(5,5)).status==S::UniqueProfile);
    atlas.Build({{1,60,"eastmarch",false,{{{0,0},{1,1},{2,2}}}}},true);
    assert(atlas.Query(P(5,5)).status==S::IncompleteMappedGeometry);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}}},false);
    assert(atlas.Query(P(5,5)).status==S::Unavailable);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}},{1,60,"rift",false,{Box(0,0)}}},true);
    assert(atlas.Query(P(5,5)).status==S::Unavailable);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}},{2,0,"rift",true,{}}},true);
    assert(atlas.Query(P(5,5)).status==S::UniqueProfile && atlas.Stats().quarantined==1);
    atlas.Build({{1,60,"eastmarch",false,{Box(0,0)}},{2,60,"",false,{Box(5,5)}}},true);
    r=atlas.Query(P(5,5)); assert(r.status==S::UniqueProfile&&r.boundaryUnmapped==1);
    std::vector<NativeRegionRecord> tooMany;
    for(std::uint32_t i=1;i<=NativeRegionAtlas::maxRecords+1;++i) tooMany.push_back({i,60,"",false,{Box(0,0)}});
    atlas.Build(std::move(tooMany),true); assert(!atlas.Stats().complete);
    RegionPolygon tooBig(NativeRegionAtlas::maxPolygonVertices+1,{0,0});
    atlas.Build({{1,60,"eastmarch",false,{std::move(tooBig)}}},true); assert(!atlas.Stats().complete);
    auto config=DefaultConfig(); assert(!config.nativeRegionShadowEnabled && config.nativeRegionProfiles.size()==3);
    config.nativeRegionProfiles.push_back({"Example.esp",1,"missing"}); bool rejected=false;
    try {Climate c(config);} catch(const std::invalid_argument&){rejected=true;} assert(rejected);
}
