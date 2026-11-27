#include "Aetherclime/TemperatureHandoffAudit.h"

#include <cmath>
#include <utility>

namespace aetherclime {
namespace {
bool ValidDate(const Date& date) noexcept {
    return date.month >= 0 && date.month < 12 && date.day >= 1 && date.day <= 31 &&
           std::isfinite(date.hour) && date.hour >= 0.0 && date.hour < 24.0;
}
bool SameDay(const Date& a, const Date& b) noexcept {
    return a.year == b.year && a.month == b.month && a.day == b.day;
}
bool ValidSample(const TemperatureHandoffSample& s) noexcept {
    return s.exterior && s.position && s.position->worldspaceFormId != 0 &&
           std::isfinite(s.position->xRaw) && std::isfinite(s.position->yRaw) &&
           ValidDate(s.date) && s.weather != Weather::Unknown &&
           !s.regionKey.empty() && std::isfinite(s.outdoorAirC);
}
}

std::string_view TemperatureHandoffStatusName(TemperatureHandoffStatus status) noexcept {
    switch (status) {
    case TemperatureHandoffStatus::NoExteriorEvidence: return "no-valid-exterior-evidence";
    case TemperatureHandoffStatus::Baseline: return "first-exterior-baseline";
    case TemperatureHandoffStatus::DifferentWorldspace: return "different-worldspace";
    case TemperatureHandoffStatus::DifferentCalendarDay: return "different-calendar-day";
    case TemperatureHandoffStatus::InvalidTime: return "reversed-game-time";
    case TemperatureHandoffStatus::TooOld: return "sample-too-old";
    case TemperatureHandoffStatus::TooFar: return "distance-too-large";
    case TemperatureHandoffStatus::WeatherChanged: return "weather-changed";
    case TemperatureHandoffStatus::Unchanged: return "no-source-or-profile-change";
    case TemperatureHandoffStatus::SourceChangedSameProfile: return "source-changed-same-profile";
    case TemperatureHandoffStatus::ProfileChanged: return "profile-changed";
    case TemperatureHandoffStatus::SignificantProfileStep: return "significant-profile-step";
    }
    return "unknown";
}

TemperatureHandoffAssessment TemperatureHandoffAudit::Observe(const TemperatureHandoffSample& current) {
    TemperatureHandoffAssessment result;
    if (!ValidSample(current)) {
        // An indoor load, unsettled cell, or nonfinite result invalidates the
        // comparison. Never carry a previous observation through an interior.
        Reset();
        return result;
    }
    if (!previous_) {
        previous_ = current;
        result.status = TemperatureHandoffStatus::Baseline;
        return result;
    }
    // Compare against the immediately preceding eligible observation, never a
    // fixed reference that could turn skipped cells into an apparent jump.
    const auto prior = std::move(*previous_);
    previous_ = current;
    result.previousPosition = prior.position;
    result.previousSource = prior.source;
    result.previousOutdoorAirC = prior.outdoorAirC;
    result.previousReadingAvailable = true;
    result.previousRegion = prior.regionKey;

    if (prior.position->worldspaceFormId != current.position->worldspaceFormId) {
        result.status = TemperatureHandoffStatus::DifferentWorldspace;
        return result;
    }
    if (!SameDay(prior.date, current.date)) {
        result.status = TemperatureHandoffStatus::DifferentCalendarDay;
        return result;
    }
    result.ageGameHours = current.date.hour - prior.date.hour;
    if (result.ageGameHours < 0.0) {
        result.status = TemperatureHandoffStatus::InvalidTime;
        return result;
    }
    if (result.ageGameHours > kMaxSampleAgeGameHours) {
        result.status = TemperatureHandoffStatus::TooOld;
        return result;
    }
    result.distanceRawUnits = std::hypot(current.position->xRaw - prior.position->xRaw,
                                         current.position->yRaw - prior.position->yRaw);
    if (!std::isfinite(result.distanceRawUnits) ||
        result.distanceRawUnits > kMaxSampleDistanceRawUnits) {
        result.status = TemperatureHandoffStatus::TooFar;
        return result;
    }
    if (current.weather != prior.weather) {
        result.status = TemperatureHandoffStatus::WeatherChanged;
        return result;
    }
    result.comparable = true;
    result.sourceChanged = current.source != prior.source;
    result.profileChanged = current.regionKey != prior.regionKey;
    result.airDeltaC = current.outdoorAirC - prior.outdoorAirC;
    result.significant = std::abs(result.airDeltaC) >= kSignificantStepC;
    if (result.profileChanged) {
        result.status = result.significant ? TemperatureHandoffStatus::SignificantProfileStep
                                           : TemperatureHandoffStatus::ProfileChanged;
    } else if (result.sourceChanged) {
        result.status = TemperatureHandoffStatus::SourceChangedSameProfile;
    } else {
        result.status = TemperatureHandoffStatus::Unchanged;
    }
    return result;
}
} // namespace aetherclime
