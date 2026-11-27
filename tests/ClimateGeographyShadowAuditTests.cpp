#include "Aetherclime/ClimateGeographyShadowAudit.h"

#include <array>
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

ClimateRegionContext At(double x, double y, std::uint32_t ws = 0x3C) {
    ClimateRegionContext ctx;
    ctx.hasCell = true;
    ctx.exteriorPosition = ClimateWorldPoint{ws, x, y};
    return ctx;
}

void KnownLocationShadowMatchesAllAnchors() {
    const Config config = DefaultConfig();
    const auto expectedSize = config.geographicAnchors.size();
    Check(expectedSize == 3, "starter samples are expected to remain three");
    for (const auto& a : config.geographicAnchors) {
        const ClimateLocationEvidence location{a.regionKey, "", 0x1234};
        auto context = At(a.xRaw, a.yRaw, a.worldspaceFormId);
        context.locationChain = std::span(&location, 1);
        const auto audit = AuditClimateGeography(config, context);
        Check(audit.effective.locationMatched(), "actual named location must still win");
        Check(audit.effective.regionKey == a.regionKey, "actual climate region altered");
        Check(audit.status == ClimateShadowStatus::AnchorSelected, "anchor not selected in shadow");
        Check(audit.shadow.source == ClimateRegionSource::GeographicAnchorProximity, "shadow must use geography");
        Check(audit.shadow.regionKey == a.regionKey, "wrong sampled climate profile");
        Check(audit.shadow.anchorLabel == a.label, "wrong copied anchor label");
        Check(audit.shadow.anchorDistanceRawUnits == 0.0, "anchor center has nonzero distance");
        Check(audit.profileComparisonAvailable && audit.sameProfile, "location+geography should agree at sample");
        Check(audit.worldspaceFormId == a.worldspaceFormId && audit.positionAvailable, "position lost");
        Check(config.geographicAnchors.size() == expectedSize, "audit must not change configuration");
    }
}

void ShadowDoesNotOverrideTheEffectiveProfile() {
    const Config config = DefaultConfig();
    const ClimateLocationEvidence namedRift{"The Rift", "", 0x5678};
    auto ctx = At(109542.4, 104511.7);
    ctx.locationChain = std::span(&namedRift, 1);
    const auto audit = AuditClimateGeography(config, ctx);
    Check(audit.effective.regionKey == "rift" && audit.effective.locationMatched(),
          "named profile must remain authoritative");
    Check(audit.shadow.regionKey == "winterhold", "counterfactual geography lost");
    Check(audit.profileComparisonAvailable && !audit.sameProfile,
          "name/geography disagreement should be explicit");
    const auto after = ResolveClimateRegion(config, ctx);
    Check(after.regionKey == "rift" && after.source == ClimateRegionSource::LocationDisplayName,
          "audit mutated effective resolution inputs");

    const ClimateLocationEvidence unknown{"Unmarked Pine Hollow", "", 0x55};
    ctx.locationChain = std::span(&unknown, 1);
    const auto unknownAudit = AuditClimateGeography(config, ctx);
    Check(unknownAudit.effective.source == ClimateRegionSource::GeographicAnchorProximity,
          "unknown names should allow real geographic fallback");
    Check(unknownAudit.shadow.regionKey == unknownAudit.effective.regionKey && unknownAudit.sameProfile,
          "shadow comparison must also work with real wilderness routing");
}

void EligibilityAndAbsentReferences() {
    const Config config = DefaultConfig();
    auto ctx = At(109542.4, 104511.7);
    ctx.hasCell = false;
    Check(AuditClimateGeography(config, ctx).status == ClimateShadowStatus::ExcludedNoCell,
          "missing cell should be excluded");
    ctx.hasCell = true;
    ctx.interior = true;
    Check(AuditClimateGeography(config, ctx).status == ClimateShadowStatus::ExcludedInterior,
          "interior must never use local coordinates");
    ctx.interior = false;
    ctx.exteriorPosition.reset();
    Check(AuditClimateGeography(config, ctx).status == ClimateShadowStatus::ExcludedNoWorldPosition,
          "missing position must be excluded");
    ctx = At(std::numeric_limits<double>::quiet_NaN(), 104511.7);
    Check(AuditClimateGeography(config, ctx).status == ClimateShadowStatus::ExcludedInvalidWorldPosition,
          "nonfinite position must be excluded");
    ctx = At(109542.4, 104511.7, 0x77);
    Check(AuditClimateGeography(config, ctx).status == ClimateShadowStatus::NoApplicableAnchor,
          "another worldspace should not inherit Tamriel anchor");
    Check(!AuditClimateGeography(config, ctx).profileComparisonAvailable,
          "a missing shadow anchor should not be reported as disagreement");
    ctx = At(900000.0, 900000.0);
    Check(AuditClimateGeography(config, ctx).status == ClimateShadowStatus::NoApplicableAnchor,
          "remote region should preserve fallback");
}

void DisabledAmbiguousAndUnconfigured() {
    Config config = DefaultConfig();
    auto ctx = At(109542.4, 104511.7);
    config.geographicFallbackEnabled = false;
    const auto disabled = AuditClimateGeography(config, ctx);
    Check(disabled.status == ClimateShadowStatus::GeographyDisabled && !disabled.shadowEvaluated,
          "disabled geography should not be audited as selected");
    config.geographicFallbackEnabled = true;
    config.geographicAnchors.push_back({"Conflict", "falkreath", 0x3C, 109542.4, 104511.7, 6000.0});
    const auto ambiguous = AuditClimateGeography(config, ctx);
    Check(ambiguous.status == ClimateShadowStatus::AmbiguousAnchorOverlap &&
          !ambiguous.profileComparisonAvailable, "overlap must remain ambiguous");
    config.geographicAnchors.pop_back();
    config.regions.erase("winterhold");
    const auto unconfigured = AuditClimateGeography(config, ctx);
    Check(unconfigured.status == ClimateShadowStatus::AnchorProfileUnconfigured,
          "missing anchor profile must not look like a valid result");
    Check(unconfigured.effective.regionKey == config.fallbackRegion,
          "unconfigured anchor must use fallback");
}
} // namespace

int main() {
    KnownLocationShadowMatchesAllAnchors();
    ShadowDoesNotOverrideTheEffectiveProfile();
    EligibilityAndAbsentReferences();
    DisabledAmbiguousAndUnconfigured();
    std::cout << "Alpha-18 climate geographic shadow audit: all 4 groups passed\n";
}
