#include "Aetherclime/ClimateGeographyShadowAudit.h"

#include <cmath>

namespace aetherclime {

std::string_view ClimateShadowStatusName(ClimateShadowStatus status) noexcept {
    switch (status) {
    case ClimateShadowStatus::AnchorSelected: return "anchor-selected";
    case ClimateShadowStatus::NoApplicableAnchor: return "no-applicable-anchor";
    case ClimateShadowStatus::AmbiguousAnchorOverlap: return "ambiguous-anchor-overlap";
    case ClimateShadowStatus::AnchorProfileUnconfigured: return "anchor-profile-unconfigured";
    case ClimateShadowStatus::GeographyDisabled: return "geography-disabled";
    case ClimateShadowStatus::ExcludedNoCell: return "excluded-no-cell";
    case ClimateShadowStatus::ExcludedInterior: return "excluded-interior";
    case ClimateShadowStatus::ExcludedNoWorldPosition: return "excluded-no-world-position";
    case ClimateShadowStatus::ExcludedInvalidWorldPosition: return "excluded-invalid-world-position";
    }
    return "unknown";
}

ClimateGeographyShadowAudit AuditClimateGeography(
    const Config& config, const ClimateRegionContext& context) {
    ClimateGeographyShadowAudit out;
    out.effective = ResolveClimateRegion(config, context);
    out.shadow.regionKey = config.fallbackRegion;

    // Respect the same eligibility restrictions as the real resolver. Do not
    // reinterpret an interior's local XYZ as exterior geographic coordinates.
    if (!context.hasCell) {
        out.status = ClimateShadowStatus::ExcludedNoCell;
        return out;
    }
    if (context.interior) {
        out.status = ClimateShadowStatus::ExcludedInterior;
        return out;
    }
    if (!context.exteriorPosition || context.exteriorPosition->worldspaceFormId == 0) {
        out.status = ClimateShadowStatus::ExcludedNoWorldPosition;
        return out;
    }
    const auto& pos = *context.exteriorPosition;
    if (!std::isfinite(pos.xRaw) || !std::isfinite(pos.yRaw)) {
        out.status = ClimateShadowStatus::ExcludedInvalidWorldPosition;
        return out;
    }
    out.worldspaceFormId = pos.worldspaceFormId;
    out.xRaw = pos.xRaw;
    out.yRaw = pos.yRaw;
    out.positionAvailable = true;
    if (!config.geographicFallbackEnabled) {
        out.status = ClimateShadowStatus::GeographyDisabled;
        return out;
    }

    // Strip only names from this temporary context. The actual context and
    // effective profile are untouched. This exercises the exact Alpha-17
    // resolver and actual active Config anchors, not a separate approximation.
    auto shadowContext = context;
    shadowContext.locationChain = {};
    out.shadow = ResolveClimateRegion(config, shadowContext);
    out.shadowEvaluated = true;
    switch (out.shadow.source) {
    case ClimateRegionSource::GeographicAnchorProximity:
        out.status = ClimateShadowStatus::AnchorSelected;
        out.profileComparisonAvailable = true;
        out.sameProfile = out.shadow.regionKey == out.effective.regionKey;
        break;
    case ClimateRegionSource::FallbackGeographyAmbiguous:
        out.status = ClimateShadowStatus::AmbiguousAnchorOverlap;
        break;
    case ClimateRegionSource::FallbackGeographyUnconfigured:
        out.status = ClimateShadowStatus::AnchorProfileUnconfigured;
        break;
    default:
        out.status = ClimateShadowStatus::NoApplicableAnchor;
        break;
    }
    return out;
}

} // namespace aetherclime
