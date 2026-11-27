#include "Aetherclime/AtlasProposalReview.h"
#include "Aetherclime/GeographicEvidence.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace aetherclime {
namespace {
using S = ClimateRegionSource;
using R = GeographicFallbackReason;
using P = AtlasProposalStatus;
bool Trusted(S source) noexcept {
    return source == S::LocationDisplayName || source == S::LocationEditorId ||
           source == S::GeographicAnchorProximity;
}
bool Generic(S source) noexcept { return source == S::FallbackNoEvidence || source == S::FallbackUnmatched; }
bool SafeGate(const GeographicFallbackFinding& f, S source) noexcept {
    if (!Generic(source) || f.directSource != source) return false;
    switch (f.reason) {
    case R::ContinuityOriginTooFar: return f.continuityStatus == ContinuityStatus::TooFar;
    case R::ContinuityOriginExpired: return f.continuityStatus == ContinuityStatus::Expired;
    case R::NoVerifiedContinuityOrigin: return f.continuityStatus == ContinuityStatus::NoVerifiedOrigin;
    // Disabled features, ambiguity, unconfigured profiles, day/world resets,
    // and unidentified gates are not evidence of a missing patch.
    default: return false;
    }
}
bool Overlap(const ProvisionalAtlasZone& a, const ProvisionalAtlasZone& b) noexcept {
    return a.worldspaceFormId == b.worldspaceFormId && a.minXRaw <= b.maxXRaw &&
        b.minXRaw <= a.maxXRaw && a.minYRaw <= b.maxYRaw && b.minYRaw <= a.maxYRaw;
}
bool Inside(const ProvisionalAtlasZone& a, double x, double y) noexcept {
    return x >= a.minXRaw && x <= a.maxXRaw && y >= a.minYRaw && y <= a.maxYRaw;
}
bool CircleOverlap(const ProvisionalAtlasZone& a, const GeographicClimateAnchor& b) noexcept {
    if (a.worldspaceFormId != b.worldspaceFormId) return false;
    return std::hypot(b.xRaw-std::clamp(b.xRaw,a.minXRaw,a.maxXRaw),
                      b.yRaw-std::clamp(b.yRaw,a.minYRaw,a.maxYRaw)) <= b.radiusRawUnits;
}
bool ValidCoordinate(double v) noexcept { return std::isfinite(v) && std::abs(v) <= 10000000.0; }
bool Better(const AtlasProposal& a, const AtlasProposal& b) noexcept {
    if (std::abs(a.observation.airDeltaC) != std::abs(b.observation.airDeltaC))
        return std::abs(a.observation.airDeltaC) > std::abs(b.observation.airDeltaC);
    if (a.observation.distanceRawUnits != b.observation.distanceRawUnits)
        return a.observation.distanceRawUnits < b.observation.distanceRawUnits;
    return a.observation.sampleIndex < b.observation.sampleIndex;
}
}
std::string_view AtlasProposalStatusName(AtlasProposalStatus s) noexcept {
    switch(s) {
    case P::Draft: return "draft-manual-review";
    case P::IneligibleHandoff: return "ineligible-handoff";
    case P::WeakSupport: return "no-adjacent-direct-support";
    case P::UnsafeFallback: return "unsafe-or-unknown-fallback-gate";
    case P::BracketTooWide: return "bracket-too-wide";
    case P::InvalidGeometry: return "invalid-geometry-or-profile";
    case P::Capacity: return "atlas-capacity";
    case P::ExistingCoverage: return "existing-coverage-at-fallback";
    case P::ConflictingAtlas: return "conflicting-atlas";
    case P::ConflictingSurvey: return "conflicting-survey";
    case P::ConflictingAnchor: return "conflicting-anchor";
    }
    return "unknown";
}
AtlasProposalStatus ValidateAtlasProposal(const Config& cfg, const ProvisionalAtlasZone& z,
                                         double x, double y) noexcept {
    if (!z.worldspaceFormId || z.label.empty() || z.label.size()>96 ||
        z.regionKey.empty() || !cfg.regions.contains(z.regionKey) ||
        !ValidCoordinate(z.minXRaw) || !ValidCoordinate(z.maxXRaw) ||
        !ValidCoordinate(z.minYRaw) || !ValidCoordinate(z.maxYRaw) ||
        !ValidCoordinate(x) || !ValidCoordinate(y) || !Inside(z,x,y) ||
        z.minXRaw>=z.maxXRaw || z.minYRaw>=z.maxYRaw ||
        z.maxXRaw-z.minXRaw>12288 || z.maxYRaw-z.minYRaw>12288) return P::InvalidGeometry;
    bool covered = false;
    for (const auto& other: cfg.provisionalAtlasZones) {
        if (z.worldspaceFormId != other.worldspaceFormId) continue;
        if (z.label==other.label) {
            if (z.regionKey==other.regionKey && Inside(other,x,y)) covered=true;
            else return P::ConflictingAtlas;
        }
        if (Overlap(z,other)) {
            if (z.regionKey!=other.regionKey) return P::ConflictingAtlas;
            if (Inside(other,x,y)) covered=true;
        }
    }
    for (const auto& other: cfg.provisionalSurveySamples) {
        if (!CircleOverlap(z,other)) continue;
        if (z.regionKey!=other.regionKey) return P::ConflictingSurvey;
        if (std::hypot(x-other.xRaw,y-other.yRaw)<=other.radiusRawUnits) covered=true;
    }
    for (const auto& other: cfg.geographicAnchors) {
        if (!CircleOverlap(z,other)) continue;
        if (z.regionKey!=other.regionKey) return P::ConflictingAnchor;
        if (std::hypot(x-other.xRaw,y-other.yRaw)<=other.radiusRawUnits) covered=true;
    }
    if (covered) return P::ExistingCoverage;
    if (cfg.provisionalAtlasZones.size()>=32) return P::Capacity;
    return P::Draft;
}
AtlasProposal MakeAtlasProposal(const Config& cfg, const GeographicGapCandidate& c) {
    AtlasProposal p;
    p.observation=c;
    const bool entry=c.kind==GeographicGapKind::FallbackEntry;
    if (!entry && c.kind!=GeographicGapKind::FallbackExit) return p;
    if (!c.profileChanged || !c.significant || !std::isfinite(c.airDeltaC) ||
        std::abs(c.airDeltaC)<TemperatureHandoffAudit::kSignificantStepC) return p;
    const auto support=entry?c.previousSource:c.currentSource;
    const auto fallback=entry?c.currentSource:c.previousSource;
    const auto& gate=entry?c.currentFallback:c.previousFallback;
    const auto& supportFinding=entry?c.previousFallback:c.currentFallback;
    if (!Trusted(support) || supportFinding.reason!=R::NotFallback || supportFinding.directSource!=support) {
        p.status=P::WeakSupport; return p;
    }
    if (!cfg.geographicFallbackEnabled || !SafeGate(gate,fallback)) { p.status=P::UnsafeFallback; return p; }
    const auto& region=entry?c.previousRegion:c.currentRegion;
    const auto& generic=entry?c.currentRegion:c.previousRegion;
    if (region==cfg.fallbackRegion || generic!=cfg.fallbackRegion || region==generic) return p;
    const double distance=std::hypot(c.currentXRaw-c.previousXRaw,c.currentYRaw-c.previousYRaw);
    if (!ValidCoordinate(c.currentXRaw) || !ValidCoordinate(c.currentYRaw) ||
        !ValidCoordinate(c.previousXRaw) || !ValidCoordinate(c.previousYRaw) ||
        !std::isfinite(c.distanceRawUnits) || !std::isfinite(distance) ||
        std::abs(distance-c.distanceRawUnits)>1.0 || distance<=0.0) { p.status=P::InvalidGeometry; return p; }
    if (distance>AtlasProposalReview::kMaxBracketRawUnits) { p.status=P::BracketTooWide; return p; }
    p.fallbackWasPrevious=!entry;
    p.fallbackXRaw=entry?c.currentXRaw:c.previousXRaw;
    p.fallbackYRaw=entry?c.currentYRaw:c.previousYRaw;
    // A small research rectangle covering the actual sampled endpoints, not
    // a straight-line path reconstruction or extrapolation of the hold.
    const double pad=AtlasProposalReview::kPaddingRawUnits;
    p.zone={"Alpha36 draft "+std::to_string(c.sampleIndex),region,c.worldspaceFormId,
        std::floor(std::min(c.previousXRaw,c.currentXRaw)-pad),
        std::ceil(std::max(c.previousXRaw,c.currentXRaw)+pad),
        std::floor(std::min(c.previousYRaw,c.currentYRaw)-pad),
        std::ceil(std::max(c.previousYRaw,c.currentYRaw)+pad)};
    p.status=ValidateAtlasProposal(cfg,p.zone,p.fallbackXRaw,p.fallbackYRaw);
    return p;
}
void AtlasProposalReview::Observe(const Config& cfg, const GeographicGapCandidate& c) {
    const bool entry=c.kind==GeographicGapKind::FallbackEntry;
    if (!entry && c.kind!=GeographicGapKind::FallbackExit) return;
    ++summary_.fallbackHandoffs;
    const auto& region=entry?c.previousRegion:c.currentRegion;
    AtlasRegionalIssue* issue=nullptr;
    for (std::size_t i=0;i<summary_.regionCount;++i) {
        if (regions_[i].worldspaceFormId==c.worldspaceFormId && regions_[i].region==region) { issue=&regions_[i]; break; }
    }
    if (!issue && summary_.regionCount<kMaxRegions) {
        issue=&regions_[summary_.regionCount++]; issue->worldspaceFormId=c.worldspaceFormId; issue->region=region;
    }
    if (issue) {
        if(entry) ++issue->entries; else ++issue->exits;
        if(c.profileChanged && c.significant) ++issue->significant;
        const auto& gate=entry?c.currentFallback:c.previousFallback;
        if(gate.reason==R::ContinuityOriginTooFar && gate.continuityStatus==ContinuityStatus::TooFar) ++issue->originTooFar;
        if(std::isfinite(c.airDeltaC) && c.profileChanged) issue->peakAbsDeltaC=std::max(issue->peakAbsDeltaC,std::abs(c.airDeltaC));
    } else ++summary_.droppedRegionalEvents;
    auto draft=MakeAtlasProposal(cfg,c);
    switch(draft.status) {
    case P::Draft: break;
    case P::WeakSupport: ++summary_.weakSupport; return;
    case P::UnsafeFallback: ++summary_.unsafeFallback; return;
    case P::BracketTooWide: ++summary_.wideBrackets; return;
    case P::ExistingCoverage: ++summary_.existingCoverage; return;
    case P::Capacity: ++summary_.capacityRejected; return;
    case P::ConflictingAtlas: case P::ConflictingSurvey: case P::ConflictingAnchor: ++summary_.conflicts; return;
    default: ++summary_.invalidOrIneligible; return;
    }
    ++summary_.eligibleDrafts;
    if(issue) ++issue->drafts;
    // De-duplicate only when BOTH observed fallback points are inside the
    // other's rectangle, and the profile agrees. Never grow a rectangle.
    for(std::size_t i=0;i<summary_.draftCount;++i) {
        auto& old=drafts_[i];
        if(old.zone.worldspaceFormId!=draft.zone.worldspaceFormId) continue;
        if(old.zone.regionKey!=draft.zone.regionKey && Overlap(old.zone,draft.zone)) {
            // Neither proposal is privileged; retain neither for activation.
            old.status=P::ConflictingAtlas; draft.status=P::ConflictingAtlas;
        }
    }
    for(std::size_t i=0;i<summary_.draftCount;++i) {
        auto& old=drafts_[i];
        if(old.zone.worldspaceFormId!=draft.zone.worldspaceFormId) continue;
        if(old.zone.regionKey==draft.zone.regionKey && Inside(old.zone,draft.fallbackXRaw,draft.fallbackYRaw) &&
           Inside(draft.zone,old.fallbackXRaw,old.fallbackYRaw)) {
            // Retain the fixed footprint (do not lose earlier conflict flags
            // or drift outward as repeated nearby observations arrive).
            if(draft.status==P::ConflictingAtlas) old.status=P::ConflictingAtlas;
            ++old.occurrences; ++summary_.mergedDrafts; return;
        }
    }
    // Bounded first observations rather than evicting conflict context. Later
    // proposals still contribute to region counts; overflow is explicit.
    if(summary_.draftCount<kMaxDrafts) drafts_[summary_.draftCount++]=std::move(draft);
    else ++summary_.droppedDrafts;
}
void AtlasProposalReview::RecordAtlasUse(const ClimateRegionResolution& r) {
    if (r.source != ClimateRegionSource::ProvisionalAtlasZone || r.interior ||
        r.anchorWorldspaceFormId == 0 || r.anchorLabel.empty() || r.regionKey.empty()) return;
    ++summary_.effectiveAtlasObservations;
    for (std::size_t i=0; i<summary_.atlasUseCount; ++i) {
        auto& use = atlasUse_[i];
        if (use.worldspaceFormId == r.anchorWorldspaceFormId && use.region == r.regionKey && use.label == r.anchorLabel) {
            ++use.observations; return;
        }
    }
    if (summary_.atlasUseCount == kMaxAtlasUse) { ++summary_.untrackedAtlasObservations; return; }
    atlasUse_[summary_.atlasUseCount++] = {r.anchorWorldspaceFormId, r.regionKey, r.anchorLabel, 1};
}
std::array<AtlasUseCount,AtlasProposalReview::kMaxAtlasUse> AtlasProposalReview::RankedAtlasUse() const {
    auto result=atlasUse_;
    std::sort(result.begin(),result.begin()+static_cast<std::ptrdiff_t>(summary_.atlasUseCount),[](const auto& a,const auto& b){
        if (a.observations!=b.observations) return a.observations>b.observations;
        if (a.worldspaceFormId!=b.worldspaceFormId) return a.worldspaceFormId<b.worldspaceFormId;
        if (a.region!=b.region) return a.region<b.region;
        return a.label<b.label;
    }); return result;
}
std::array<AtlasProposal,AtlasProposalReview::kMaxDrafts> AtlasProposalReview::RankedDrafts() const {
    auto result=drafts_; std::sort(result.begin(),result.begin()+static_cast<std::ptrdiff_t>(summary_.draftCount),Better); return result;
}
std::array<AtlasRegionalIssue,AtlasProposalReview::kMaxRegions> AtlasProposalReview::RankedRegions() const {
    auto result=regions_;
    std::sort(result.begin(),result.begin()+static_cast<std::ptrdiff_t>(summary_.regionCount),[](const auto& a,const auto& b){
        if(a.significant!=b.significant) return a.significant>b.significant;
        if(a.peakAbsDeltaC!=b.peakAbsDeltaC) return a.peakAbsDeltaC>b.peakAbsDeltaC;
        if(a.worldspaceFormId!=b.worldspaceFormId) return a.worldspaceFormId<b.worldspaceFormId;
        return a.region<b.region;
    }); return result;
}
} // namespace aetherclime
