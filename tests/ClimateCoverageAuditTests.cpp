#include "Aetherclime/ClimateCoverageAudit.h"
#include "Aetherclime/ClimateGeographyShadowAudit.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace aetherclime;

namespace {
void Check(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}
ClimateRegionContext At(double x, double y, std::uint32_t ws = 0x3C,
                        std::optional<double> z = {}) {
    ClimateRegionContext ctx;
    ctx.hasCell = true;
    ctx.exteriorPosition = ClimateWorldPoint{ws, x, y, z};
    return ctx;
}
void RealCentersAndVerticalEvidence() {
    const auto config = DefaultConfig();
    for (const auto& a : config.geographicAnchors) {
        auto ctx = At(a.xRaw, a.yRaw, a.worldspaceFormId, -8999.25);
        const auto name = ClimateLocationEvidence{a.regionKey, {}, 0x1234};
        ctx.locationChain = std::span(&name, 1);
        const auto before = ResolveClimateRegion(config, ctx);
        const auto coverage = AuditClimateCoverage(config, ctx);
        Check(coverage.status == ClimateCoverageStatus::WithinOneReference, "center not inside circle");
        Check(coverage.nearestAnchorLabel == a.label && coverage.nearestRegionKey == a.regionKey,
              "nearest sample provenance lost");
        Check(coverage.nearestDistanceRawUnits == 0.0 &&
              coverage.nearestSignedMarginRawUnits == a.radiusRawUnits &&
              coverage.nearestRadiusRawUnits == a.radiusRawUnits, "center distance/margin wrong");
        Check(coverage.inRangeAnchors == 1 && coverage.distinctInRangeProfiles == 1,
              "expected one matching sample");
        Check(!coverage.nearSampleEdge && coverage.nearestBoundaryDistanceRawUnits == a.radiusRawUnits,
              "center should not appear near edge");
        Check(coverage.rawZAvailable && coverage.zRaw == -8999.25, "raw Z lost or converted");
        const auto after = ResolveClimateRegion(config, ctx);
        Check(before.regionKey == after.regionKey && before.source == after.source &&
              after.locationMatched(), "audit changed effective location resolution");
        const auto shadow = AuditClimateGeography(config, ctx);
        Check(shadow.status == ClimateShadowStatus::AnchorSelected &&
              shadow.shadow.regionKey == a.regionKey, "coverage and existing shadow disagree");
    }
}
void BoundaryOffsetsAndGap() {
    const auto config = DefaultConfig();
    const auto& a = config.geographicAnchors.front();
    const auto inside = AuditClimateCoverage(config, At(a.xRaw + a.radiusRawUnits - 500.0, a.yRaw));
    Check(inside.status == ClimateCoverageStatus::WithinOneReference && inside.nearSampleEdge,
          "near-inside edge missed");
    Check(std::abs(inside.nearestSignedMarginRawUnits - 500.0) < 1e-7, "inside signed margin wrong");
    const auto exactEdge = AuditClimateCoverage(config, At(a.xRaw + a.radiusRawUnits, a.yRaw));
    Check(exactEdge.status == ClimateCoverageStatus::WithinOneReference && exactEdge.nearSampleEdge &&
          std::abs(exactEdge.nearestSignedMarginRawUnits) < 1e-7, "inclusive boundary should be measured");
    const auto outside = AuditClimateCoverage(config, At(a.xRaw + a.radiusRawUnits + 500.0, a.yRaw));
    Check(outside.status == ClimateCoverageStatus::OutsideSampleNeighborhoods && outside.nearSampleEdge,
          "near-outside edge missed");
    Check(std::abs(outside.nearestSignedMarginRawUnits + 500.0) < 1e-7, "outside signed margin wrong");
    const auto remote = AuditClimateCoverage(config, At(700000.0, 750000.0));
    Check(remote.status == ClimateCoverageStatus::OutsideSampleNeighborhoods && !remote.nearSampleEdge,
          "remote point incorrectly covered");
}
void OverlapAndPriorityOfNamedLocations() {
    Config config = DefaultConfig();
    auto& a = config.geographicAnchors.front();
    config.geographicAnchors.push_back({"Winterhold repeat", "winterhold", 0x3C,
                                        a.xRaw + 200.0, a.yRaw, 4000.0});
    auto ctx = At(a.xRaw, a.yRaw);
    auto coverage = AuditClimateCoverage(config, ctx);
    Check(coverage.status == ClimateCoverageStatus::WithinSameProfileOverlap &&
          coverage.inRangeAnchors == 2 && coverage.distinctInRangeProfiles == 1,
          "same-profile overlap should not be conflicting");
    config.geographicAnchors.push_back({"Rift conflict", "rift", 0x3C,
                                        a.xRaw + 100.0, a.yRaw, 3000.0});
    coverage = AuditClimateCoverage(config, ctx);
    Check(coverage.status == ClimateCoverageStatus::ConflictingProfileOverlap &&
          coverage.inRangeAnchors == 3 && coverage.distinctInRangeProfiles == 2,
          "conflicting profile overlap missed");
    const ClimateLocationEvidence named{"Winterhold", {}, 0x111};
    ctx.locationChain = std::span(&named, 1);
    Check(ResolveClimateRegion(config, ctx).regionKey == "winterhold",
          "geographic conflict must not override a recognized location");
    Check(AuditClimateGeography(config, ctx).status == ClimateShadowStatus::AmbiguousAnchorOverlap,
          "effective resolver must preserve ambiguity in shadow");
}
void ExclusionsAndDisabledGeography() {
    Config config = DefaultConfig();
    auto ctx = At(109542.4, 104511.7, 0x3C, 145.0);
    ctx.hasCell = false;
    Check(AuditClimateCoverage(config, ctx).status == ClimateCoverageStatus::ExcludedNoCell,
          "no-cell should be excluded");
    ctx.hasCell = true;
    ctx.interior = true;
    Check(AuditClimateCoverage(config, ctx).status == ClimateCoverageStatus::ExcludedInterior,
          "interior local Z must be excluded");
    ctx.interior = false;
    ctx.exteriorPosition.reset();
    Check(AuditClimateCoverage(config, ctx).status == ClimateCoverageStatus::ExcludedNoWorldPosition,
          "missing coordinates should be excluded");
    ctx = At(109542.4, 104511.7, 0x77);
    Check(AuditClimateCoverage(config, ctx).status == ClimateCoverageStatus::NoReferenceForWorldspace,
          "unrelated worldspace should have no samples");
    ctx = At(109542.4, 104511.7);
    Check(!AuditClimateCoverage(config, ctx).rawZAvailable,
          "missing Z must not be invented");
    ctx = At(std::numeric_limits<double>::infinity(), 0.0);
    Check(AuditClimateCoverage(config, ctx).status == ClimateCoverageStatus::ExcludedInvalidPosition,
          "invalid XY should be excluded");
    ctx = At(0.0, 0.0, 0x3C, std::numeric_limits<double>::quiet_NaN());
    Check(AuditClimateCoverage(config, ctx).status == ClimateCoverageStatus::ExcludedInvalidPosition,
          "invalid Z should be excluded");
    config.geographicFallbackEnabled = false;
    ctx = At(109542.4, 104511.7);
    Check(AuditClimateCoverage(config, ctx).status == ClimateCoverageStatus::Disabled,
          "disabled geography should not produce coverage");
}
void DifferentRadiiAndCap() {
    Config config = DefaultConfig();
    config.geographicAnchors.clear();
    config.geographicAnchors.push_back({"near tiny", "winterhold", 0x3C, 0.0, 0.0, 10.0});
    config.geographicAnchors.push_back({"far wide", "falkreath", 0x3C, 100.0, 0.0, 200.0});
    const auto coverage = AuditClimateCoverage(config, At(15.0, 0.0));
    Check(coverage.status == ClimateCoverageStatus::WithinOneReference &&
          coverage.nearestAnchorLabel == "near tiny" && coverage.nearestSignedMarginRawUnits == -5.0 &&
          coverage.inRangeAnchors == 1, "nearest CENTER may be outside while another radius covers");
    Check(coverage.nearestBoundaryDistanceRawUnits == 5.0,
          "nearest boundary distance must consider any sample circle");
    // Verify the independent safety cap when passed raw, unvalidated config.
    for (unsigned i = 0; i < 70; ++i)
        config.geographicAnchors.push_back({"extra", "rift", 0x3C, 0.0, 0.0, 100.0});
    const auto capped = AuditClimateCoverage(config, At(15.0, 0.0));
    Check(capped.worldspaceAnchors == 64, "coverage must cap raw input at 64 anchors");
}
} // namespace
int main() {
    RealCentersAndVerticalEvidence();
    BoundaryOffsetsAndGap();
    OverlapAndPriorityOfNamedLocations();
    ExclusionsAndDisabledGeography();
    DifferentRadiiAndCap();
    std::cout << "Alpha-19 geographic coverage and raw elevation evidence: 5 groups passed\n";
}
