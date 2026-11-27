#include "Aetherclime/ClimateRegionResolver.h"
#include "Aetherclime/RegionalContinuity.h"
#include "Aetherclime/EnvironmentalSnapshot.h"
#include "Aetherclime/LocationIdentity.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace aetherclime;
namespace {
ClimateRegionContext Wilderness(double x, double y, std::uint32_t worldspace = 0x3C) {
    ClimateRegionContext ctx;
    ctx.hasCell = true;
    ctx.exteriorPosition = ClimateWorldPoint{worldspace, x, y};
    return ctx;
}
Date Noon() { return Date{.month = 9, .day = 12, .hour = 12.0, .year = 201}; }
}
int main() {
    Config config = DefaultConfig();
    assert(!config.geographicSurveyEnabled);
    assert(config.provisionalSurveySamples.size() == 1);
    const auto& survey = config.provisionalSurveySamples[0];
    assert(survey.regionKey == "winterhold");
    assert(std::abs(survey.xRaw - 72751.8) < 1.0);
    const auto site = Wilderness(survey.xRaw, survey.yRaw);

    // Explicit opt-in. The region is NOT trusted even exactly on the observation.
    auto off = ResolveClimateRegion(config, site);
    assert(off.regionKey == "temperate" && !off.provisional());
    config.geographicSurveyEnabled = true;
    const auto on = ResolveClimateRegion(config, site);
    assert(on.regionKey == "winterhold");
    assert(on.source == ClimateRegionSource::ProvisionalSurveySample);
    assert(on.provisional() && !on.matched());
    assert(on.anchorDistanceRawUnits == 0.0);
    assert(on.anchorRadiusRawUnits == survey.radiusRawUnits);
    assert(on.anchorLabel == survey.label);
    assert(ClimateRegionSourceName(on.source) == "provisional-survey-sample");

    // The survey is stateless and independent of any chronological continuity.
    RegionalContinuity continuity;
    assert(!continuity.ObserveVerified(on, site, Noon()));
    const auto assessment = continuity.Assess(on, site, Noon(), false);
    assert(assessment.status == ContinuityStatus::SurveySampleProvisional);
    assert(assessment.resolution.provisional() && !assessment.resolution.matched());
    assert(assessment.provisional());
    assert(assessment.distanceRawUnits == 0.0);
    assert(continuity.Assess(on, site, Noon(), true).status == ContinuityStatus::SurveySampleProvisional);

    // Spatial, worldspace and interior boundaries are hard limits.
    const auto near = ResolveClimateRegion(config, Wilderness(survey.xRaw + 1200.0, survey.yRaw));
    assert(near.provisional());
    assert(!ResolveClimateRegion(config, Wilderness(survey.xRaw + survey.radiusRawUnits + 0.01,
        survey.yRaw)).provisional());
    assert(!ResolveClimateRegion(config, Wilderness(survey.xRaw, survey.yRaw, 0x99)).provisional());
    auto indoor = site; indoor.interior = true;
    assert(!ResolveClimateRegion(config, indoor).provisional());
    auto noCell = site; noCell.hasCell = false;
    assert(!ResolveClimateRegion(config, noCell).provisional());
    assert(!ResolveClimateRegion(config, Wilderness(std::numeric_limits<double>::quiet_NaN(), 0)).provisional());
    config.geographicFallbackEnabled = false;
    assert(!ResolveClimateRegion(config, site).provisional());
    config.geographicFallbackEnabled = true;

    // Verified location evidence wins over a provisional survey.
    const ClimateLocationEvidence rift{"Riften", "", 0x1234};
    auto named = site;
    named.locationChain = std::span(&rift, 1);
    auto resolved = ResolveClimateRegion(config, named);
    assert(resolved.matched() && resolved.regionKey == "rift");
    const ClimateLocationEvidence ambiguous{"Riften and Winterhold", "", 0x1235};
    named.locationChain = std::span(&ambiguous, 1);
    resolved = ResolveClimateRegion(config, named);
    assert(resolved.source == ClimateRegionSource::FallbackAmbiguous);
    assert(!resolved.provisional());

    // The established, higher-confidence geographic sample remains authoritative.
    const auto verified = ResolveClimateRegion(config, Wilderness(109542.4,104511.7));
    assert(verified.source == ClimateRegionSource::GeographicAnchorProximity);
    assert(verified.matched() && !verified.provisional());

    // Explicit conflicting provisional samples must yield ambiguity, not a guessed profile.
    auto conflict = config;
    conflict.provisionalSurveySamples.push_back({"contradiction", "rift", 0x3C,
        survey.xRaw, survey.yRaw, 1000.0});
    const auto disputed = ResolveClimateRegion(conflict, site);
    assert(disputed.source == ClimateRegionSource::FallbackGeographyAmbiguous);
    assert(!disputed.provisional() && !disputed.matched());

    // Invalid region/profile/radius and excessive points are rejected at construction.
    auto bad = config;
    bad.provisionalSurveySamples[0].radiusRawUnits = 13000.0;
    bool thrown = false;
    try { Climate climate(bad); (void)climate; }
    catch (const std::invalid_argument&) { thrown = true; }
    assert(thrown);
    bad = config;
    bad.provisionalSurveySamples[0].regionKey = "nonexistent";
    thrown = false;
    try { Climate climate(bad); (void)climate; }
    catch (const std::invalid_argument&) { thrown = true; }
    assert(thrown);

    // A future consumer MUST see provisional but not verified weather/local ambient.
    LocalIdentity loc = ResolveLocalIdentity({true, false, 0x8E8A, {}, {}, "Near winterhold", "winterhold"});
    assert(loc.name == "Near Winterhold (local area unknown)");
    assert(loc.certainty == LocalNameCertainty::RegionalOnly);
    EnvironmentSnapshotInput input;
    input.hasCell = true;
    input.regionKey = on.regionKey;
    input.regionName = "Winterhold";
    input.regionSource = std::string(ClimateRegionSourceName(on.source));
    input.trustedRegion = false;
    input.provisionalRegion = on.provisional();
    input.local = loc;
    input.weatherProvenance = OutdoorWeatherProvenance::DirectExteriorSky;
    input.weatherFormId = 0x1234;
    input.outdoorAirC = 0.18;
    input.outdoorFeelsLikeC = 0.18;
    auto snapshot = AssembleEnvironmentSnapshot(input);
    assert(snapshot.available && snapshot.provisionalRegion && !snapshot.trustedRegion);
    assert(!snapshot.localAmbientC && !snapshot.verifiedOutdoorWeatherFormId);
    AetherclimeEnvironmentSnapshotV1 output{};
    assert(ToPublicSnapshotV1(snapshot, output));
    assert((output.flags & AETHERCLIME_ENV_PROVISIONAL_REGION) != 0);
    assert((output.flags & AETHERCLIME_ENV_OUTDOOR_AIR_UNVERIFIED) != 0);
    assert((output.flags & AETHERCLIME_ENV_TRUSTED_REGION) == 0);
    assert((output.flags & AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER) == 0);
    assert((output.flags & AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE) == 0);
    assert(output.verifiedOutdoorWeatherFormId == 0);

    // Disable survey and direct evidence; continuity still functions independently.
    config.geographicSurveyEnabled = false;
    continuity.Reset();
    assert(continuity.ObserveVerified(verified, Wilderness(109542.4,104511.7), Noon()));
    assert(continuity.Assess(ResolveClimateRegion(config, Wilderness(84000, 104511.7)),
        Wilderness(84000, 104511.7), Noon(), true).status == ContinuityStatus::TooFar);
    std::cout << "Alpha-27 provisional geographic survey tests passed\n";
}
