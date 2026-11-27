#include "Aetherclime/RegionAtlas.h"
#include "Aetherclime/RegionalContinuity.h"
#include "Aetherclime/EnvironmentalSnapshot.h"
#include "Aetherclime/LocationIdentity.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace aetherclime;

namespace {
ClimateRegionContext Outdoor(double x, double y, std::uint32_t world = 0x3c) {
    ClimateRegionContext ctx;
    ctx.hasCell = true;
    ctx.exteriorPosition = ClimateWorldPoint{world, x, y};
    return ctx;
}
Date Noon() { return Date{.month=9, .day=12, .hour=12.0, .year=201}; }
bool InvalidConfig(const Config& config) {
    try { Climate model(config); (void)model; return false; }
    catch (const std::invalid_argument&) { return true; }
}
}
int main() {
    auto config = DefaultConfig();
    assert(!config.geographicAtlasEnabled);
    assert(config.provisionalAtlasZones.size() == 1);
    const auto zone = config.provisionalAtlasZones.front();
    assert(zone.label == "Tower Doomstone pilot tile");
    assert(zone.regionKey == "winterhold");
    const double x = (zone.minXRaw+zone.maxXRaw)/2;
    const double y = (zone.minYRaw+zone.maxYRaw)/2;
    const auto site = Outdoor(x,y);
    assert(EvaluateRegionAtlas(config, site).status == AtlasStatus::Disabled);
    config.geographicAtlasEnabled = true;
    auto audit = EvaluateRegionAtlas(config, site);
    assert(audit.status == AtlasStatus::MatchedProvisional);
    assert(audit.provisional());
    assert(audit.regionKey == "winterhold");
    assert(audit.zoneLabel == zone.label);
    assert(audit.matchingZones == 1);
    assert(audit.minXRaw == zone.minXRaw && audit.maxXRaw == zone.maxXRaw);
    assert(AtlasStatusName(audit.status) == "provisional-atlas-zone");
    auto resolved = ResolveClimateRegion(config, site);
    assert(resolved.source == ClimateRegionSource::ProvisionalAtlasZone);
    assert(resolved.provisional() && !resolved.matched());
    assert(resolved.anchorLabel == zone.label);
    assert(resolved.anchorDistanceRawUnits == -1.0); // rectangles have no radius
    assert(resolved.anchorRadiusRawUnits == -1.0);
    assert(ClimateRegionSourceName(resolved.source) == "provisional-atlas-zone");

    // Exact rectangle edges are included; crossing any edge falls out.
    assert(EvaluateRegionAtlas(config, Outdoor(zone.minXRaw,zone.minYRaw)).provisional());
    assert(EvaluateRegionAtlas(config, Outdoor(zone.maxXRaw,zone.maxYRaw)).provisional());
    assert(EvaluateRegionAtlas(config, Outdoor(zone.minXRaw-0.01,y)).status == AtlasStatus::OutsideCoverage);
    assert(EvaluateRegionAtlas(config, Outdoor(x,zone.maxYRaw+0.01)).status == AtlasStatus::OutsideCoverage);
    assert(EvaluateRegionAtlas(config, Outdoor(x,y,0x99)).status == AtlasStatus::OutsideCoverage);
    assert(!ResolveClimateRegion(config, Outdoor(x,y,0x99)).provisional());
    auto indoor=site; indoor.interior=true;
    assert(EvaluateRegionAtlas(config, indoor).status == AtlasStatus::NoExteriorPoint);
    assert(!ResolveClimateRegion(config, indoor).provisional());
    auto missingCell=site; missingCell.hasCell=false;
    assert(EvaluateRegionAtlas(config, missingCell).status == AtlasStatus::NoExteriorPoint);
    assert(!ResolveClimateRegion(config, missingCell).provisional());
    assert(EvaluateRegionAtlas(config, Outdoor(std::numeric_limits<double>::quiet_NaN(),y)).status == AtlasStatus::NoExteriorPoint);
    config.geographicFallbackEnabled=false;
    assert(EvaluateRegionAtlas(config,site).status == AtlasStatus::Disabled);
    assert(!ResolveClimateRegion(config,site).provisional());
    config.geographicFallbackEnabled=true;

    // Real named evidence wins regardless of atlas. The atlas audit remains a shadow.
    const ClimateLocationEvidence namedWinterhold{"The Tower Doomstone Winterhold", "", 0xD567E};
    auto named=site;
    named.locationChain=std::span(&namedWinterhold,1);
    resolved=ResolveClimateRegion(config,named);
    assert(resolved.locationMatched() && resolved.matched());
    assert(resolved.source == ClimateRegionSource::LocationDisplayName);
    assert(EvaluateRegionAtlas(config,named).provisional());
    const ClimateLocationEvidence conflictingName{"Winterhold and Riften", "", 0x123};
    named.locationChain=std::span(&conflictingName,1);
    assert(ResolveClimateRegion(config,named).source == ClimateRegionSource::FallbackAmbiguous);
    const auto verified=ResolveClimateRegion(config,Outdoor(109542.4,104511.7));
    assert(verified.matched() && !verified.provisional());
    // A configured trusted anchor always beats the experimental atlas.
    auto originalAnchors=config.geographicAnchors;
    config.geographicAnchors.push_back({"Verified overlap test", "rift", 0x3C,x,y, 512.0});
    resolved=ResolveClimateRegion(config,site);
    assert(resolved.source==ClimateRegionSource::GeographicAnchorProximity);
    assert(resolved.regionKey=="rift" && resolved.matched());
    config.geographicAnchors=originalAnchors;

    // Two overlapping same-profile zones are deterministic; narrowest one wins.
    config.provisionalAtlasZones.push_back({"A smaller tile", "winterhold", 0x3C,
                                            x-500,x+500,y-500,y+500});
    resolved=ResolveClimateRegion(config,site);
    assert(resolved.source==ClimateRegionSource::ProvisionalAtlasZone);
    assert(resolved.anchorLabel=="A smaller tile");
    assert(EvaluateRegionAtlas(config,site).matchingZones==2);
    // Conflicting atlas profiles MUST be ambiguous, independent of array order.
    config.provisionalAtlasZones.back().regionKey="rift";
    assert(EvaluateRegionAtlas(config,site).status==AtlasStatus::ConflictingRegions);
    assert(ResolveClimateRegion(config,site).source==ClimateRegionSource::FallbackGeographyAmbiguous);
    std::swap(config.provisionalAtlasZones[0], config.provisionalAtlasZones[1]);
    assert(ResolveClimateRegion(config,site).source==ClimateRegionSource::FallbackGeographyAmbiguous);
    config.provisionalAtlasZones={zone};
    // A contradictory *survey circle* also cannot be overridden by atlas.
    config.geographicSurveyEnabled=true;
    config.provisionalSurveySamples[0].regionKey="rift";
    assert(ResolveClimateRegion(config,site).source==ClimateRegionSource::FallbackGeographyAmbiguous);
    config.provisionalSurveySamples[0].regionKey="winterhold";
    assert(ResolveClimateRegion(config,site).source==ClimateRegionSource::ProvisionalAtlasZone);
    config.geographicAtlasEnabled=false;
    assert(ResolveClimateRegion(config,site).source==ClimateRegionSource::ProvisionalSurveySample);
    config.geographicAtlasEnabled=true;

    // Atlas estimates cannot initialize or refresh time-based verified origins.
    RegionalContinuity continuity;
    resolved=ResolveClimateRegion(config,site);
    assert(!continuity.ObserveVerified(resolved,site,Noon()));
    auto decision=continuity.Assess(resolved,site,Noon(),true);
    assert(decision.status==ContinuityStatus::AtlasZoneProvisional && decision.provisional());
    assert(continuity.Assess(resolved,site,Noon(),false).status==ContinuityStatus::AtlasZoneProvisional);
    assert(continuity.ObserveVerified(verified, Outdoor(109542.4,104511.7), Noon()));
    decision=continuity.Assess(resolved,site,Noon(),true);
    assert(decision.status==ContinuityStatus::AtlasZoneProvisional);

    // The V1 ABI unchanged. Atlas values are never marked trusted or published
    // as reliable local ambient / verified Sky weather, even at tile center.
    EnvironmentSnapshotInput input;
    input.hasCell=true;
    input.regionKey=resolved.regionKey;
    input.regionName="Winterhold";
    input.regionSource=std::string(ClimateRegionSourceName(resolved.source));
    input.trustedRegion=resolved.matched();
    input.provisionalRegion=resolved.provisional();
    input.local=ResolveLocalIdentity({true,false,0x8EAA,{}, {},"Near winterhold","winterhold"});
    input.weatherProvenance=OutdoorWeatherProvenance::DirectExteriorSky;
    input.weatherFormId=0x123;
    input.outdoorAirC=0.15;
    input.outdoorFeelsLikeC=0.15;
    const auto snapshot=AssembleEnvironmentSnapshot(input);
    assert(snapshot.provisionalRegion && !snapshot.trustedRegion);
    assert(!snapshot.localAmbientC && !snapshot.verifiedOutdoorWeatherFormId);
    AetherclimeEnvironmentSnapshotV1 api{};
    assert(ToPublicSnapshotV1(snapshot,api));
    assert((api.flags&AETHERCLIME_ENV_PROVISIONAL_REGION)!=0);
    assert((api.flags&AETHERCLIME_ENV_OUTDOOR_AIR_UNVERIFIED)!=0);
    assert((api.flags&AETHERCLIME_ENV_TRUSTED_REGION)==0);
    assert((api.flags&AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE)==0);
    assert((api.flags&AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER)==0);
    assert(api.verifiedOutdoorWeatherFormId==0);

    // Hard bounds, required profile, finite coordinates, safe maximum count.
    auto bad=config;
    bad.provisionalAtlasZones.front().maxXRaw=bad.provisionalAtlasZones.front().minXRaw;
    assert(InvalidConfig(bad));
    bad=config; bad.provisionalAtlasZones.front().maxYRaw += 9000;
    assert(InvalidConfig(bad));
    bad=config; bad.provisionalAtlasZones.front().regionKey="nonexistent";
    assert(InvalidConfig(bad));
    bad=config; bad.provisionalAtlasZones.front().worldspaceFormId=0;
    assert(InvalidConfig(bad));
    bad=config; bad.provisionalAtlasZones.front().minXRaw=std::numeric_limits<double>::infinity();
    assert(InvalidConfig(bad));
    bad=config; bad.provisionalAtlasZones.resize(33,zone);
    assert(InvalidConfig(bad));
    // Empty atlas list is explicitly allowed and produces outside-coverage.
    config.provisionalAtlasZones.clear();
    assert(EvaluateRegionAtlas(config,site).status==AtlasStatus::OutsideCoverage);
    std::cout << "Alpha-28 bounded geographic atlas tests passed\n";
}
