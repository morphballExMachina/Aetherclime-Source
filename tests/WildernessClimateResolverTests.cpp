#include "Aetherclime/ClimateRegionResolver.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace aetherclime;
namespace {
void Check(bool value, std::string_view msg) {
    if (!value) throw std::runtime_error(std::string(msg));
}

ClimateRegionContext Wilderness(double x, double y, std::uint32_t worldspace = 0x3C) {
    ClimateRegionContext ctx;
    ctx.hasCell = true;
    ctx.exteriorPosition = ClimateWorldPoint{worldspace, x, y};
    return ctx;
}

void SurveyedNeighborhoods() {
    const Config config = DefaultConfig();
    Check(config.geographicAnchors.size() == 3, "expected only three sampled neighborhoods");
    for (const auto& a : config.geographicAnchors) {
        const auto at = ResolveClimateRegion(config, Wilderness(a.xRaw, a.yRaw));
        Check(at.matched(), "sample XY should match");
        Check(!at.locationMatched(), "sample XY must not become location-name evidence");
        Check(at.source == ClimateRegionSource::GeographicAnchorProximity, "wrong source");
        Check(at.regionKey == a.regionKey, "incorrect sampled profile");
        Check(at.anchorLabel == a.label, "anchor label was not copied");
        Check(at.anchorRadiusRawUnits == a.radiusRawUnits, "radius was not recorded");
        Check(at.anchorDistanceRawUnits == 0.0, "anchor zero distance was not recorded");
        Check(at.anchorWorldspaceFormId == a.worldspaceFormId, "worldspace was not preserved");
        Sample climateSample{.date = {0, 15, 12.0, 201}, .region = at.regionKey};
        Check(Climate(config).Calculate(climateSample).region == config.regions.at(a.regionKey).name,
              "resolved wilderness profile not fed to climate core");
    }
    const auto inWinterhold = ResolveClimateRegion(config, Wilderness(110000.0, 107000.0));
    Check(inWinterhold.regionKey == "winterhold", "College-side local vicinity should remain Winterhold profile");
    const auto nearFalkreath = ResolveClimateRegion(config, Wilderness(-32000.0, -86000.0));
    Check(nearFalkreath.regionKey == "falkreath", "nearby Falkreath wilds not resolved");
}

void ConservativeRejection() {
    const Config config = DefaultConfig();
    const auto& winterhold = config.geographicAnchors.front();
    Check(ResolveClimateRegion(config, Wilderness(800000.0, 800000.0)).source ==
          ClimateRegionSource::FallbackNoEvidence, "remote point must fall back");
    Check(ResolveClimateRegion(config, Wilderness(winterhold.xRaw, winterhold.yRaw, 999)).regionKey ==
          "temperate", "another worldspace must not inherit Tamriel climate");
    Check(ResolveClimateRegion(config, Wilderness(winterhold.xRaw + winterhold.radiusRawUnits + 0.1,
          winterhold.yRaw)).regionKey == "temperate", "outside the sample radius must fall back");
    Check(ResolveClimateRegion(config, Wilderness(
        std::numeric_limits<double>::quiet_NaN(), winterhold.yRaw)).regionKey == "temperate",
        "invalid coordinates must fall back");
    ClimateRegionContext withoutCell = Wilderness(winterhold.xRaw, winterhold.yRaw);
    withoutCell.hasCell = false;
    Check(ResolveClimateRegion(config, withoutCell).regionKey == "temperate",
          "no cell must not route to sample XY");
    auto interior = Wilderness(winterhold.xRaw, winterhold.yRaw);
    interior.interior = true;
    Check(ResolveClimateRegion(config, interior).regionKey == "temperate",
          "interior local coordinates must be excluded");
    Config disabled = config;
    disabled.geographicFallbackEnabled = false;
    Check(ResolveClimateRegion(disabled, Wilderness(winterhold.xRaw, winterhold.yRaw)).regionKey ==
          "temperate", "disabled geography must have no effect");
    disabled.geographicFallbackEnabled = true;
    disabled.geographicAnchors.clear();
    Check(ResolveClimateRegion(disabled, Wilderness(winterhold.xRaw, winterhold.yRaw)).regionKey ==
          "temperate", "explicitly empty anchor array must be respected");
}

void NamePrecedenceAndAmbiguity() {
    Config config = DefaultConfig();
    const ClimateLocationEvidence name{"Riften", "", 12};
    auto ctx = Wilderness(109542.4, 104511.7);
    ctx.locationChain = std::span(&name, 1);
    const auto named = ResolveClimateRegion(config, ctx);
    Check(named.source == ClimateRegionSource::LocationDisplayName, "name must outrank geography");
    Check(named.regionKey == "rift", "named climate incorrectly overridden by nearest sample");

    const ClimateLocationEvidence unknown{"Abandoned Pine Camp", "", 34};
    ctx.locationChain = std::span(&unknown, 1);
    const auto unknownLoc = ResolveClimateRegion(config, ctx);
    Check(unknownLoc.regionKey == "winterhold", "unrecognized location should allow geographic fallback");
    Check(unknownLoc.matchedLocationFormId == 0, "should not misattribute anchor to named location");

    const ClimateLocationEvidence ambiguous{"Falkreath / Winterhold border", "", 35};
    ctx.locationChain = std::span(&ambiguous, 1);
    const auto conflictedName = ResolveClimateRegion(config, ctx);
    Check(conflictedName.source == ClimateRegionSource::FallbackAmbiguous,
          "ambiguous named evidence must not be overridden by proximity");

    config.geographicAnchors.push_back({"Conflicting overlapping reference", "falkreath", 0x3C,
                                       109542.4, 104511.7, 5000.0});
    ctx.locationChain = {};
    const auto overlapped = ResolveClimateRegion(config, ctx);
    Check(overlapped.source == ClimateRegionSource::FallbackGeographyAmbiguous,
          "overlapping differently keyed samples must not choose arbitrary zone");
    Check(overlapped.regionKey == "temperate", "overlap needs configured fallback");
}

void InvalidAndUnconfigured() {
    Config config = DefaultConfig();
    const auto sample = Wilderness(109542.4, 104511.7);
    config.regions.erase("winterhold");
    Check(ResolveClimateRegion(config, sample).source ==
          ClimateRegionSource::FallbackGeographyUnconfigured,
          "anchor referencing a missing profile must use explicit fallback");
    Check(ResolveClimateRegion(config, sample).regionKey == "temperate",
          "unconfigured anchor must not select unknown key");

    Config tooMany = DefaultConfig();
    while (tooMany.geographicAnchors.size() < 65)
        tooMany.geographicAnchors.push_back(tooMany.geographicAnchors.front());
    bool rejected = false;
    try { Climate climate(tooMany); (void)climate; } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected, "unbounded anchor list not rejected");

    Config badRadius = DefaultConfig();
    badRadius.geographicAnchors.front().radiusRawUnits = std::numeric_limits<double>::infinity();
    rejected = false;
    try { Climate climate(badRadius); (void)climate; } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected, "invalid radius not rejected");
}
} // namespace

int main() {
    SurveyedNeighborhoods();
    ConservativeRejection();
    NamePrecedenceAndAmbiguity();
    InvalidAndUnconfigured();
    std::cout << "Alpha-17 wilderness resolver: all 4 test groups passed\n";
}
