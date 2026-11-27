#include "Aetherclime/BiomeIdentity.h"
#include <algorithm>
#include <cmath>
namespace aetherclime {
const char* BiomeEvidenceName(BiomeEvidence e) noexcept {
    switch(e) {
    case BiomeEvidence::Unique: return "unique";
    case BiomeEvidence::Overlap: return "overlap";
    case BiomeEvidence::OverlapEdge: return "overlap-edge";
    case BiomeEvidence::Nearby: return "nearby";
    default: return "none";
    }
}
bool ValidBiomeIdentity(const BiomeIdentity& b) noexcept {
    const auto key=[](const std::string& s) {
        if(s.empty() || s.size()>=48) return false;
        for(char c:s) if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')) return false;
        return true;
    };
    if(!b.available || !key(b.biomeA) || !std::isfinite(b.weightA) || !std::isfinite(b.weightB) ||
       b.weightA<=0 || b.weightA>1 || b.weightB<0 || b.weightB>1 || std::abs(b.weightA+b.weightB-1)>1e-9) return false;
    if(b.evidence==BiomeEvidence::Overlap)
        return key(b.biomeB) && b.biomeA!=b.biomeB && b.weightB>0;
    if(b.evidence!=BiomeEvidence::Unique && b.evidence!=BiomeEvidence::OverlapEdge && b.evidence!=BiomeEvidence::Nearby) return false;
    return b.biomeB.empty() && b.weightA==1 && b.weightB==0;
}
std::optional<double> BiomeShare(const BiomeIdentity& b,std::string_view key) noexcept {
    if(!ValidBiomeIdentity(b) || b.evidence==BiomeEvidence::Nearby) return std::nullopt;
    if(key==b.biomeA) return b.weightA;
    if(key==b.biomeB) return b.weightB;
    return 0.0;
}
BiomeIdentity ResolveBiomeIdentity(const Config& config,const NativeAtlasResult& atlas) {
    BiomeIdentity out;
    const auto& cfg=config.biomeClimate;
    using B=NativeBiomeStatus;
    out.status=NativeBiomeStatusName(atlas.biomeStatus);
    if(atlas.biomeStatus==B::Unique) {
        out.biomeA=atlas.biome; out.weightA=1; out.evidence=BiomeEvidence::Unique;
    } else if(atlas.biomeStatus==B::Boundary) {
        // A single interior label plus at most one other boundary-only label
        // has an unambiguous one-sided limit. Shared edges with no interior,
        // multiple interior labels, or complex junctions still abstain.
        std::vector<BiomeSpatialEvidence> touching;
        for(const auto& e:atlas.biomeEvidence) if(e.inside||e.boundary) touching.push_back(e);
        const auto inside=std::find_if(touching.begin(),touching.end(),[](const auto& e){return e.inside&&e.edgeDistanceRaw>0;});
        if(touching.size()>2||inside==touching.end()||std::count_if(touching.begin(),touching.end(),[](const auto& e){return e.inside;})!=1) return out;
        out.biomeA=inside->biome;out.weightA=1;out.status="overlap-edge-interior-preview";out.evidence=BiomeEvidence::OverlapEdge;
    } else if(atlas.biomeStatus==B::Conflict) {
        // Only two-biome interior overlaps get a provisional weighted preview.
        std::vector<BiomeSpatialEvidence> inside;
        for(const auto& e:atlas.biomeEvidence) {
            if(e.boundary) {out.status="overlap-boundary-abstained";return out;}
            if(e.inside) inside.push_back(e);
        }
        if(inside.size()!=2 || inside[0].edgeDistanceRaw<=0 || inside[1].edgeDistanceRaw<=0) {out.status="complex-overlap-abstained";return out;}
        // Deterministic, insensitive to same-label duplicates; not a proof of a real ecotone.
        std::sort(inside.begin(),inside.end(),[](const auto& a,const auto& b){return a.biome<b.biome;});
        out.biomeA=inside[0].biome;out.biomeB=inside[1].biome;
        out.weightA=inside[0].edgeDistanceRaw/(inside[0].edgeDistanceRaw+inside[1].edgeDistanceRaw);
        out.weightB=1-out.weightA;out.status="two-biome-overlap-preview";out.evidence=BiomeEvidence::Overlap;
    } else if(atlas.biomeStatus==B::NoCoverage) {
        auto nearby=atlas.biomeEvidence;
        if(cfg.maxGapRawUnits<=0||nearby.empty()) return out;
        std::sort(nearby.begin(),nearby.end(),[](const auto& a,const auto& b){return a.edgeDistanceRaw!=b.edgeDistanceRaw?a.edgeDistanceRaw<b.edgeDistanceRaw:a.biome<b.biome;});
        const auto& first=nearby[0];
        out.gapDistanceRaw=first.edgeDistanceRaw;
        if(nearby.size()>1) out.gapRunnerUpRaw=nearby[1].edgeDistanceRaw;
        if(first.inside||first.boundary||first.edgeDistanceRaw<0||first.edgeDistanceRaw>cfg.maxGapRawUnits) {out.status="gap-too-far";return out;}
        // Require a competing known label for gap discrimination, never extrapolate a lone polygon.
        if(nearby.size()<2||out.gapRunnerUpRaw-out.gapDistanceRaw<cfg.gapSeparationRawUnits) {out.status="gap-ambiguous";return out;}
        out.biomeA=first.biome;out.weightA=1;out.status="nearby-biome-provisional";out.evidence=BiomeEvidence::Nearby;
    } else return out;
    out.available=true;
    if(!ValidBiomeIdentity(out)) {out.available=false;out.evidence=BiomeEvidence::None;out.status="invalid-biome-evidence";}
    return out;
}
} // namespace aetherclime
