#include "Aetherclime/AtlasProposalReview.h"
#include "Aetherclime/RegionAtlasDataAudit.h"
#include "Aetherclime/RegionalContinuity.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <utility>
using namespace aetherclime;
namespace {
using S=ClimateRegionSource; using R=GeographicFallbackReason; using P=AtlasProposalStatus;
Config EmptyGeography() { auto c=DefaultConfig(); c.geographicAnchors.clear(); c.provisionalSurveySamples.clear(); c.provisionalAtlasZones.clear(); return c; }
GeographicGapCandidate Exit(double x,double y,double x2,double y2,std::uint64_t index=1,const char* region="eastmarch") {
    GeographicGapCandidate c; c.kind=GeographicGapKind::FallbackExit; c.priority=GeographicGapPriority::High;
    c.worldspaceFormId=60; c.sampleIndex=index; c.previousXRaw=x; c.previousYRaw=y; c.currentXRaw=x2; c.currentYRaw=y2;
    c.distanceRawUnits=std::hypot(x2-x,y2-y); c.airDeltaC=-15.99; c.profileChanged=true; c.significant=true;
    c.previousRegion="temperate"; c.currentRegion=region; c.previousSource=S::FallbackNoEvidence; c.currentSource=S::LocationDisplayName;
    c.previousFallback={R::ContinuityOriginTooFar,S::FallbackNoEvidence,ContinuityStatus::TooFar,13000,0.1};
    c.currentFallback={R::NotFallback,S::LocationDisplayName,ContinuityStatus::DirectEvidence}; return c;
}
GeographicGapCandidate Reverse(GeographicGapCandidate c) {
    c.kind=GeographicGapKind::FallbackEntry; std::swap(c.previousXRaw,c.currentXRaw); std::swap(c.previousYRaw,c.currentYRaw);
    std::swap(c.previousSource,c.currentSource); std::swap(c.previousRegion,c.currentRegion); std::swap(c.previousFallback,c.currentFallback); c.airDeltaC=-c.airDeltaC; return c;
}
void ProposalAndSafety() {
    auto cfg=EmptyGeography(); const auto c=Exit(114696.6,-37350.7,115591.1,-36853.4,180); const auto d=MakeAtlasProposal(cfg,c);
    assert(d.status==P::Draft && d.fallbackWasPrevious); assert(d.zone.minXRaw==114440 && d.zone.maxXRaw==115848);
    assert(d.zone.minYRaw==-37607 && d.zone.maxYRaw==-36597); assert(d.zone.regionKey=="eastmarch" && d.zone.worldspaceFormId==60);
    assert(MakeAtlasProposal(cfg,Reverse(c)).status==P::Draft);
    auto wrong=c; wrong.currentSource=S::ProvisionalRecentExterior; assert(MakeAtlasProposal(cfg,wrong).status==P::WeakSupport);
    wrong=c; wrong.previousFallback.reason=R::ContinuityDisabled; assert(MakeAtlasProposal(cfg,wrong).status==P::UnsafeFallback);
    wrong=c; wrong.previousSource=S::FallbackAmbiguous; wrong.previousFallback.directSource=wrong.previousSource; assert(MakeAtlasProposal(cfg,wrong).status==P::UnsafeFallback);
    wrong=c; wrong.previousFallback.continuityStatus=ContinuityStatus::DifferentWorldspace; assert(MakeAtlasProposal(cfg,wrong).status==P::UnsafeFallback);
    wrong=c; wrong.significant=false; assert(MakeAtlasProposal(cfg,wrong).status==P::IneligibleHandoff);
    assert(MakeAtlasProposal(cfg,Exit(0,0,5000,0)).status==P::BracketTooWide);
    wrong=c; wrong.currentXRaw=std::numeric_limits<double>::infinity(); assert(MakeAtlasProposal(cfg,wrong).status==P::InvalidGeometry);
    wrong=c; wrong.distanceRawUnits=100; assert(MakeAtlasProposal(cfg,wrong).status==P::InvalidGeometry);
    wrong=c; wrong.currentRegion="unknown"; assert(MakeAtlasProposal(cfg,wrong).status==P::InvalidGeometry);
    cfg.geographicFallbackEnabled=false; assert(MakeAtlasProposal(cfg,c).status==P::UnsafeFallback);
}
void ConflictsAndCapacity() {
    auto cfg=EmptyGeography(); const auto c=Exit(0,0,1000,0); const auto d=MakeAtlasProposal(cfg,c);
    cfg.provisionalAtlasZones={{"touch","rift",60,1256,1400,-100,100}}; assert(MakeAtlasProposal(cfg,c).status==P::ConflictingAtlas);
    cfg.provisionalAtlasZones[0].worldspaceFormId=61; assert(MakeAtlasProposal(cfg,c).status==P::Draft);
    cfg.provisionalAtlasZones.clear(); cfg.provisionalSurveySamples={{"survey","rift",60,1400,0,144}}; assert(MakeAtlasProposal(cfg,c).status==P::ConflictingSurvey);
    cfg.provisionalSurveySamples.clear(); cfg.geographicAnchors={{"anchor","rift",60,1400,0,144}}; assert(MakeAtlasProposal(cfg,c).status==P::ConflictingAnchor);
    cfg.geographicAnchors[0].regionKey="eastmarch"; assert(MakeAtlasProposal(cfg,c).status==P::Draft);
    cfg.geographicAnchors[0].xRaw=0; assert(MakeAtlasProposal(cfg,c).status==P::ExistingCoverage);
    cfg.geographicAnchors.clear(); cfg.provisionalAtlasZones.push_back(d.zone); assert(MakeAtlasProposal(cfg,c).status==P::ExistingCoverage);
    cfg.provisionalAtlasZones.clear(); for(int i=0;i<32;++i) cfg.provisionalAtlasZones.push_back({"other"+std::to_string(i),"rift",61,0,1,0,1}); assert(MakeAtlasProposal(cfg,c).status==P::Capacity);
}
void RegionalCountsAndBoundedDrafts() {
    auto cfg=EmptyGeography(); AtlasProposalReview report; auto c=Exit(0,0,1000,0); report.Observe(cfg,c); auto reverse=Reverse(c); reverse.sampleIndex=2; report.Observe(cfg,reverse);
    auto weak=Reverse(Exit(4200,0,5000,0,3)); weak.previousSource=S::ProvisionalRecentExterior; report.Observe(cfg,weak);
    assert(report.Summary().fallbackHandoffs==3 && report.Summary().draftCount==1 && report.Summary().mergedDrafts==1);
    assert(report.Summary().weakSupport==1 && report.RankedDrafts()[0].occurrences==2);
    auto east=report.RankedRegions()[0]; assert(east.entries==2 && east.exits==1 && east.significant==3 && east.originTooFar==3);
    report.Observe(cfg,Exit(0,0,1000,0,4,"rift")); assert(report.Summary().draftCount==2);
    for(std::size_t i=0;i<2;++i) assert(report.RankedDrafts()[i].status==P::ConflictingAtlas);
    for(int i=0;i<12;++i) report.Observe(cfg,Exit(10000+i*10000,0,10500+i*10000,0,5+i));
    assert(report.Summary().draftCount==AtlasProposalReview::kMaxDrafts && report.Summary().droppedDrafts==6);
    report.Reset(); assert(report.Summary().draftCount==0 && report.Summary().regionCount==0);
}
void PilotReplay() {
    auto cfg=DefaultConfig(); cfg.geographicAtlasEnabled=true; cfg.geographicSurveyEnabled=true;
    AtlasProposalReview usage;
    // Historical exit pairs (rounded at log precision); gate values are synthetic, not historical facts.
    auto e1=Exit(114696.6,-37350.7,115591.1,-36853.4,180); auto e2=Exit(104595.5,-28637.5,102372.5,-26658.2,192);
    auto e3=Exit(86650.5,-4088.7,86009.2,-3854.6,208); auto f=Exit(-20488.2,-66772.6,-24595.0,-67393.4,16,"falkreath");
    f.currentSource=S::GeographicAnchorProximity; f.currentFallback.directSource=f.currentSource; f.airDeltaC=-5.72;
    for(const auto& c:{e1,e2,e3,f}) {
        ClimateRegionContext ctx; ctx.hasCell=true; ctx.exteriorPosition=ClimateWorldPoint{60,c.previousXRaw,c.previousYRaw};
        auto before=ResolveClimateRegion(cfg,ctx); assert(before.regionKey=="temperate" && !before.matched());
        auto d=MakeAtlasProposal(cfg,c); assert(d.status==P::Draft); cfg.provisionalAtlasZones.push_back(d.zone);
        auto after=ResolveClimateRegion(cfg,ctx); assert(after.regionKey==c.currentRegion && after.source==S::ProvisionalAtlasZone && !after.matched());
        usage.RecordAtlasUse(before); usage.RecordAtlasUse(after); usage.RecordAtlasUse(after);
        // At identical weather, date and elevation, selecting the same
        // profile on both sides eliminates the PROFILE component of the step.
        Climate climate(cfg); Sample sample; sample.date={4,11,12,201}; sample.weather=Weather::Clear;
        sample.region=before.regionKey; const double original=climate.Calculate(sample).airC;
        sample.region=after.regionKey; const double adjusted=climate.Calculate(sample).airC;
        sample.region=c.currentRegion; assert(climate.Calculate(sample).airC==adjusted);
        assert(std::abs(original-adjusted)>0.1);
        RegionalContinuity continuity; assert(!continuity.ObserveVerified(after,ctx,{4,11,12,201}));
    }
    assert(usage.Summary().effectiveAtlasObservations==8 && usage.Summary().atlasUseCount==4);
    assert(usage.RankedAtlasUse()[0].observations==2);
    assert(AuditAtlasData(cfg).empty()); Climate validation(cfg);
    ClimateRegionContext ctx; ctx.hasCell=true; ctx.exteriorPosition=ClimateWorldPoint{60,e1.previousXRaw,e1.previousYRaw};
    ClimateLocationEvidence named[]={ {"The Rift","",1} }; ctx.locationChain=named;
    auto namedResult=ResolveClimateRegion(cfg,ctx); assert(namedResult.locationMatched() && namedResult.regionKey=="rift");
    ctx.locationChain={}; cfg.geographicAtlasEnabled=false; assert(ResolveClimateRegion(cfg,ctx).regionKey=="temperate");
}
}
int main(){ ProposalAndSafety(); ConflictsAndCapacity(); RegionalCountsAndBoundedDrafts(); PilotReplay(); std::cout<<"Alpha-36 atlas proposal and pilot replay passed\n"; }
