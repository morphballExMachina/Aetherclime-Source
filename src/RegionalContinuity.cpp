#include "Aetherclime/RegionalContinuity.h"

#include <cmath>

namespace aetherclime {
namespace {
bool ValidPoint(const ClimateRegionContext& ctx) noexcept {
    return ctx.hasCell && !ctx.interior && ctx.exteriorPosition &&
           ctx.exteriorPosition->worldspaceFormId != 0 &&
           std::isfinite(ctx.exteriorPosition->xRaw) &&
           std::isfinite(ctx.exteriorPosition->yRaw);
}
bool ValidDate(const Date& d) noexcept {
    return d.month >= 0 && d.month < 12 && d.day >= 1 && d.day <= 31 &&
           std::isfinite(d.hour) && d.hour >= 0.0 && d.hour < 24.0;
}
bool SameDay(const Date& a, const Date& b) noexcept {
    return a.year == b.year && a.month == b.month && a.day == b.day;
}
bool ExplicitlyUnsafe(const ClimateRegionResolution& r) noexcept {
    return r.source == ClimateRegionSource::FallbackAmbiguous ||
           r.source == ClimateRegionSource::FallbackGeographyAmbiguous ||
           r.source == ClimateRegionSource::FallbackUnconfigured ||
           r.source == ClimateRegionSource::FallbackGeographyUnconfigured;
}
}

std::string_view ContinuityStatusName(ContinuityStatus status) noexcept {
    switch (status) {
    case ContinuityStatus::DirectEvidence: return "direct-region-evidence";
    case ContinuityStatus::Disabled: return "disabled";
    case ContinuityStatus::NoVerifiedOrigin: return "no-verified-exterior-origin";
    case ContinuityStatus::NoExteriorPosition: return "no-valid-exterior-position";
    case ContinuityStatus::ExplicitAmbiguity: return "ambiguous-or-unconfigured-evidence";
    case ContinuityStatus::DifferentWorldspace: return "different-worldspace";
    case ContinuityStatus::DifferentCalendarDay: return "different-calendar-day";
    case ContinuityStatus::Expired: return "origin-too-old";
    case ContinuityStatus::TooFar: return "beyond-origin-distance-limit";
    case ContinuityStatus::Provisional: return "provisional-nearby-region";
    case ContinuityStatus::SurveySampleProvisional: return "provisional-survey-neighborhood";
    case ContinuityStatus::AtlasZoneProvisional: return "provisional-atlas-zone";
    }
    return "unknown";
}

bool RegionalContinuity::ObserveVerified(const ClimateRegionResolution& direct,
    const ClimateRegionContext& context, const Date& date) {
    if (!direct.matched() || direct.regionKey.empty() || !ValidPoint(context) || !ValidDate(date))
        return false;
    anchor_ = Anchor{direct.regionKey, *context.exteriorPosition, date};
    return true;
}

ContinuityAssessment RegionalContinuity::Assess(const ClimateRegionResolution& direct,
    const ClimateRegionContext& context, const Date& date, bool enabled) const {
    ContinuityAssessment result{.resolution = direct};
    if (direct.matched()) {
        result.status = ContinuityStatus::DirectEvidence;
        return result;
    }
    // A static low-confidence survey has its own opt-in and does NOT require
    // a recent verified origin. Never chain a survey into the continuity anchor.
    if (direct.source == ClimateRegionSource::ProvisionalAtlasZone) {
        result.status = ContinuityStatus::AtlasZoneProvisional;
        return result;
    }
    if (direct.source == ClimateRegionSource::ProvisionalSurveySample) {
        result.status = ContinuityStatus::SurveySampleProvisional;
        result.distanceRawUnits = direct.anchorDistanceRawUnits;
        return result;
    }
    if (!enabled) { result.status = ContinuityStatus::Disabled; return result; }
    if (ExplicitlyUnsafe(direct)) {
        result.status = ContinuityStatus::ExplicitAmbiguity;
        return result;
    }
    if (!ValidPoint(context) || !ValidDate(date)) {
        result.status = ContinuityStatus::NoExteriorPosition;
        return result;
    }
    if (!anchor_) { result.status = ContinuityStatus::NoVerifiedOrigin; return result; }
    const auto& position = *context.exteriorPosition;
    if (position.worldspaceFormId != anchor_->point.worldspaceFormId) {
        result.status = ContinuityStatus::DifferentWorldspace;
        return result;
    }
    if (!SameDay(anchor_->date, date)) {
        result.status = ContinuityStatus::DifferentCalendarDay;
        return result;
    }
    result.ageGameHours = date.hour - anchor_->date.hour;
    if (result.ageGameHours < 0.0 || result.ageGameHours > kMaxAgeGameHours) {
        result.status = ContinuityStatus::Expired;
        return result;
    }
    result.distanceRawUnits = std::hypot(position.xRaw - anchor_->point.xRaw,
                                          position.yRaw - anchor_->point.yRaw);
    if (!std::isfinite(result.distanceRawUnits) ||
        result.distanceRawUnits > kMaxDistanceRawUnits) {
        result.status = ContinuityStatus::TooFar;
        return result;
    }
    result.resolution.regionKey = anchor_->region;
    result.resolution.source = ClimateRegionSource::ProvisionalRecentExterior;
    result.status = ContinuityStatus::Provisional;
    return result;
}
} // namespace aetherclime
