#include "Aetherclime/NativeRegionAtlas.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <unordered_set>
using namespace aetherclime;
using B=NativeBiomeStatus;
using R=NativeBiomeRole;
RegionPolygon Box(double x,double y,double w=10){return {{x,y},{x+w,y},{x+w,y+w},{x,y+w}};}
ClimateWorldPoint P(double x,double y,std::uint32_t world=60){return {world,x,y,{}};}
NativeRegionRecord Record(unsigned id,unsigned world, std::string biome,R role,RegionPolygon p,bool incomplete=false){
    return {id,world,"",incomplete,{std::move(p)},"Example, \"record\"",std::move(biome),role};
}
int main(int argc,char** argv){
    std::unordered_set<unsigned> ids; unsigned broad=0,variants=0,local=0;
    for(auto& e:kVanillaBiomeRegistry){
        assert(ids.insert(e.localFormId).second && !e.editorId.empty());
        if(e.role==R::Broad) ++broad; else if(e.role==R::NoPrecipVariant) ++variants; else ++local;
        assert(IsBroadBiome(e.role)==!e.biome.empty());
    }
    assert(broad==9&&variants==6&&local==10);
    NativeRegionAtlas a;
    assert(a.Query(P(5,5)).biomeStatus==B::Unavailable);
    assert(a.Query(std::nullopt).biomeStatus==B::NoPosition);
    a.Build({Record(1,60,"snow",R::Broad,Box(0,0)),Record(2,60,"snow",R::NoPrecipVariant,Box(2,2)),Record(3,60,"",R::LocalOverride,Box(2,2)),Record(4,0,"coastal",R::Broad,Box(0,0)),Record(5,61,"coastal",R::Broad,Box(0,0))},true);
    auto r=a.Query(P(5,5)); assert(r.biomeStatus==B::Unique&&r.biome=="snow"&&r.biomeContaining==2&&r.localOverrideContaining==1);
    assert(r.unmappedContaining==3 && r.incompleteUnmapped==0 && a.Stats().quarantined==1);
    assert(a.Query(P(5,5,61)).biome=="coastal");
    assert(a.Query(P(5,5,62)).biomeStatus==B::NoCoverage);
    assert(a.Query(P(0,5)).biomeStatus==B::Boundary);
    std::ostringstream csv; assert(a.ExportGeometry(csv));
    auto text=csv.str(); assert(text.find("aetherclime-native-geometry-v1,type,")==0);
    assert(text.find("\"Example, \"\"record\"\"\"")!=std::string::npos);
    assert(text.find(",vertex,1,,,,,,,0,0,0,0\n")!=std::string::npos);
    assert(text.find(",end,")!=std::string::npos);
    if(argc==2){std::ofstream f(argv[1],std::ios::binary); assert(a.ExportGeometry(f));}
    a.Build({Record(1,60,"snow",R::Broad,Box(0,0)),Record(2,60,"coastal",R::Broad,Box(2,2))},true);
    assert(a.Query(P(5,5)).biomeStatus==B::Conflict && a.Query(P(5,5)).biome.empty());
    a.Build({Record(1,60,"snow",R::Broad,Box(0,0)),Record(2,60,"coastal",R::Broad,{},true)},true);
    assert(a.Query(P(5,5)).biomeStatus==B::Incomplete);
    a.Build({Record(1,60,"snow",R::Broad,{{0,0},{1,1},{2,2}})},true);
    assert(a.Query(P(5,5)).biomeStatus==B::Incomplete);
    a.Build({},false); std::ostringstream rejected; assert(!a.ExportGeometry(rejected)&&rejected.str().empty());
    assert(!DefaultConfig().nativeRegionGeometryExportEnabled);
}
