#include "Aetherclime/NativeRegionAtlas.h"
#include <unordered_set>
#include <utility>
#include <iomanip>
#include <locale>
#include <ostream>
#include <sstream>
namespace aetherclime {
const char* NativeAtlasStatusName(NativeAtlasStatus s) noexcept {
    switch(s) {
    case NativeAtlasStatus::Unavailable:return "unavailable";
    case NativeAtlasStatus::NoExteriorPosition:return "no-exterior-position";
    case NativeAtlasStatus::NoCoverage:return "no-coverage";
    case NativeAtlasStatus::UnmappedCoverage:return "unmapped-coverage";
    case NativeAtlasStatus::IncompleteMappedGeometry:return "incomplete-mapped-geometry";
    case NativeAtlasStatus::Boundary:return "mapped-boundary";
    case NativeAtlasStatus::ConflictingProfiles:return "conflicting-profiles";
    case NativeAtlasStatus::UniqueProfile:return "unique-mapped-profile";
    }
    return "unavailable";
}
const char* NativeBiomeStatusName(NativeBiomeStatus s) noexcept {
    switch(s) {
    case NativeBiomeStatus::Unavailable:return "unavailable";
    case NativeBiomeStatus::NoPosition:return "no-position";
    case NativeBiomeStatus::NoCoverage:return "no-biome-coverage";
    case NativeBiomeStatus::Incomplete:return "incomplete-biome-geometry";
    case NativeBiomeStatus::Boundary:return "biome-boundary";
    case NativeBiomeStatus::Conflict:return "conflicting-biomes";
    case NativeBiomeStatus::Unique:return "unique-biome";
    }
    return "unavailable";
}
void NativeRegionAtlas::Build(std::vector<NativeRegionRecord> inputs, bool complete) {
    records_.clear(); stats_={};
    if (!complete || inputs.empty() || inputs.size()>maxRecords) return;
    std::unordered_set<std::uint32_t> seen;
    for(auto& input:inputs) {
        if(!input.formId || !seen.insert(input.formId).second) { records_.clear(); stats_={}; return; }
        Record r{input.formId,input.worldspace,std::move(input.profile),input.incomplete,{},std::move(input.editorId),std::move(input.biome),input.biomeRole};
        if(!r.worldspace) ++stats_.quarantined;
        if(IsBroadBiome(r.biomeRole)) ++stats_.biomeRecords;
        if(!r.worldspace || input.polygons.empty()) r.incomplete=true;
        for(auto& p:input.polygons) {
            if(++stats_.polygons>maxPolygons || p.size()>maxPolygonVertices || p.size()>maxVertices-stats_.vertices) { records_.clear(); stats_={}; return; }
            stats_.vertices+=p.size();
            if(p.size()<3 || !std::all_of(p.begin(),p.end(),[](const auto& v){return IsFinitePoint(v)&&std::abs(v.x)<=1e7&&std::abs(v.y)<=1e7;})) { r.incomplete=true; continue; }
            Polygon q{std::move(p)};
            q.minX=q.maxX=q.points.front().x; q.minY=q.maxY=q.points.front().y;
            double twiceArea=0;
            for(std::size_t i=0,j=q.points.size()-1;i<q.points.size();j=i++) {
                const auto& a=q.points[j]; const auto& b=q.points[i];
                q.minX=std::min(q.minX,b.x); q.maxX=std::max(q.maxX,b.x);
                q.minY=std::min(q.minY,b.y); q.maxY=std::max(q.maxY,b.y);
                twiceArea+=a.x*b.y-b.x*a.y;
            }
            if(std::abs(twiceArea)<0.01) { r.incomplete=true; continue; }
            r.polygons.push_back(std::move(q));
        }
        ++stats_.records; if(!r.profile.empty()) ++stats_.mapped; if(r.incomplete) ++stats_.incomplete;
        records_.push_back(std::move(r));
    }
    stats_.complete=true;
}
NativeAtlasResult NativeRegionAtlas::Query(const std::optional<ClimateWorldPoint>& pos, bool measureBiomeEdges) const {
    NativeAtlasResult out;
    if(!pos || !pos->worldspaceFormId || !IsFinitePoint({pos->xRaw,pos->yRaw})) {out.status=NativeAtlasStatus::NoExteriorPosition;out.biomeStatus=NativeBiomeStatus::NoPosition;return out;}
    if(!stats_.complete) return out;
    RegionPoint2 point{pos->xRaw,pos->yRaw};
    bool mappedIncomplete=false, mappedBoundary=false, conflict=false;
    std::string profile, biome;
    bool biomeIncomplete=false, biomeBoundary=false, biomeConflict=false;
    for(const auto& r:records_) {
        // Missing identity is quarantined, never a wildcard worldspace.
        if(!r.worldspace || r.worldspace!=pos->worldspaceFormId) continue;
        if(IsBroadBiome(r.biomeRole) && (r.incomplete || r.biome.empty())) biomeIncomplete=true;
        if(r.incomplete) { if(!r.profile.empty()) mappedIncomplete=true; else ++out.incompleteUnmapped; }
        bool inside=false,boundary=false;
        const bool measure=measureBiomeEdges && IsBroadBiome(r.biomeRole) && !r.biome.empty();
        double deepest=-1, nearest=-1;

        for(const auto& p:r.polygons) {
            if(!measure && (point.x<p.minX-.01||point.x>p.maxX+.01||point.y<p.minY-.01||point.y>p.maxY+.01)) continue;
            const auto relation=TestPolygon(p.points,point);
            inside|=relation==PolygonRelation::Inside; boundary|=relation==PolygonRelation::Boundary;
            if(measure) {
                double edge=-1;
                for(std::size_t i=0,j=p.points.size()-1;i<p.points.size();j=i++) {
                    const auto& a=p.points[j]; const auto& b=p.points[i];
                    const double dx=b.x-a.x,dy=b.y-a.y,len2=dx*dx+dy*dy;
                    const double t=len2>0?std::clamp(((point.x-a.x)*dx+(point.y-a.y)*dy)/len2,0.0,1.0):0.0;
                    const double d=std::hypot(point.x-(a.x+t*dx),point.y-(a.y+t*dy));
                    if(edge<0||d<edge) edge=d;
                }
                if(relation==PolygonRelation::Inside) deepest=std::max(deepest,edge);
                else if(nearest<0||edge<nearest) nearest=edge;
            }
        }
        if(measure && (deepest>=0||nearest>=0)) {
            auto it=std::find_if(out.biomeEvidence.begin(),out.biomeEvidence.end(),[&](const auto& e){return e.biome==r.biome;});
            const double distance=inside?deepest:nearest;
            if(it==out.biomeEvidence.end()) out.biomeEvidence.push_back({r.biome,distance,inside,boundary});
            else {
                if(inside) it->edgeDistanceRaw=it->inside?std::max(it->edgeDistanceRaw,distance):distance;
                else if(!it->inside) it->edgeDistanceRaw=std::min(it->edgeDistanceRaw,distance);
                it->inside|=inside; it->boundary|=boundary;
            }
        }
        if(IsBroadBiome(r.biomeRole)) {
            if(boundary) biomeBoundary=true;
            if(inside) { ++out.biomeContaining; if(biome.empty()) biome=r.biome; else if(biome!=r.biome) biomeConflict=true; }
        } else if(r.biomeRole==NativeBiomeRole::LocalOverride && (inside||boundary)) ++out.localOverrideContaining;
        if(r.profile.empty()) {
            if(boundary) ++out.boundaryUnmapped;
            if(inside||boundary) {
                if(out.unmappedContaining<out.unmappedIds.size()) out.unmappedIds[out.unmappedContaining]=r.id;
                ++out.unmappedContaining;
            }
            continue;
        }
        if(boundary) mappedBoundary=true;
        if(inside) {++out.mappedContaining; if(profile.empty()) profile=r.profile; else if(profile!=r.profile) conflict=true;}
    }
    if(mappedIncomplete) out.status=NativeAtlasStatus::IncompleteMappedGeometry;
    else if(conflict) out.status=NativeAtlasStatus::ConflictingProfiles;
    else if(mappedBoundary) out.status=NativeAtlasStatus::Boundary;
    else if(!profile.empty()) {out.status=NativeAtlasStatus::UniqueProfile;out.profile=std::move(profile);}
    else out.status=out.unmappedContaining?NativeAtlasStatus::UnmappedCoverage:NativeAtlasStatus::NoCoverage;
    if(biomeIncomplete) out.biomeStatus=NativeBiomeStatus::Incomplete;
    else if(biomeConflict) out.biomeStatus=NativeBiomeStatus::Conflict;
    else if(biomeBoundary) out.biomeStatus=NativeBiomeStatus::Boundary;
    else if(!biome.empty()) {out.biomeStatus=NativeBiomeStatus::Unique;out.biome=std::move(biome);}
    else out.biomeStatus=NativeBiomeStatus::NoCoverage;
    std::sort(out.biomeEvidence.begin(),out.biomeEvidence.end(),[](const auto& a,const auto& b){return a.biome<b.biome;});
    return out;
}
namespace {
std::string CsvQuote(const std::string& s) {
    std::string result="\"";
    for(char c:s) { if(c=='\"') result+='\"'; result+=c; }
    return result+'\"';
}
}
bool NativeRegionAtlas::ExportGeometry(std::ostream& out) const {
    if(!stats_.complete) return false;
    std::ostringstream csv;
    csv.imbue(std::locale::classic());
    csv<<std::setprecision(17);
    csv<<"aetherclime-native-geometry-v1,type,formId,worldspace,editorId,profile,biome,role,incomplete,polygon,vertex,x,y\n";
    for(const auto& r:records_) {
        csv<<",record,"<<r.id<<','<<r.worldspace<<','<<CsvQuote(r.editorId)<<','<<CsvQuote(r.profile)<<','<<CsvQuote(r.biome)<<','<<NativeBiomeRoleName(r.biomeRole)<<','<<(r.incomplete?1:0)<<",,,,\n";
        for(std::size_t i=0;i<r.polygons.size();++i) {
            const auto& polygon=r.polygons[i];
            for(std::size_t j=0;j<polygon.points.size();++j) {
                const auto& v=polygon.points[j];
                csv<<",vertex,"<<r.id<<",,,,,,,"<<i<<','<<j<<','<<v.x<<','<<v.y<<'\n';
            }
        }
    }
    csv<<",end,,,,,,,,,,,\n";
    out<<csv.str(); out.flush(); return bool(out);
}
NativeShadowDecision CompareNativeShadow(const ClimateRegionResolution& direct,const ClimateRegionResolution& effective,const NativeAtlasResult& atlas) {
    using S=ClimateRegionSource;
    if(direct.matched()||effective.matched()) return {effective.regionKey,"direct-evidence-preserved",false};
    if(direct.source!=S::FallbackNoEvidence && direct.source!=S::FallbackUnmatched && !direct.provisional())
        return {effective.regionKey,"explicit-ambiguity-or-other-gate-preserved",false};
    if(atlas.status!=NativeAtlasStatus::UniqueProfile) return {effective.regionKey,"no-unique-native-profile",false};
    return {atlas.profile,"native-profile-shadow",!effective.provisional()&&effective.regionKey!=atlas.profile};
}
void NativeShadowCoverage::Observe(const NativeAtlasResult& r,const NativeShadowDecision& d,const ClimateRegionResolution& effective) {
    ++samples;
    switch(r.status) {
    case NativeAtlasStatus::Unavailable:++unavailable;break;
    case NativeAtlasStatus::NoExteriorPosition:++noPosition;break;
    case NativeAtlasStatus::NoCoverage:++noCoverage;break;
    case NativeAtlasStatus::UnmappedCoverage:++unmapped;break;
    case NativeAtlasStatus::IncompleteMappedGeometry:++incomplete;break;
    case NativeAtlasStatus::Boundary:++boundary;break;
    case NativeAtlasStatus::ConflictingProfiles:++conflicts;break;
    case NativeAtlasStatus::UniqueProfile:++unique; if(r.profile==effective.regionKey) ++sameProfile;else ++differentProfile;break;
    }
    if(d.wouldReplaceFallback) ++fallbackWouldReplace;
    if(effective.matched()) ++directPreserved;
}
}
