#include "Aetherclime/ExteriorWeatherContext.h"

#include <cmath>

namespace aetherclime {
namespace {
bool ValidClock(const Date& value) noexcept {
    return value.month >= 0 && value.month < 12 && value.day >= 1 && value.day <= 31 &&
           std::isfinite(value.hour) && value.hour >= 0.0 && value.hour < 24.0;
}
} // namespace

std::string_view ExteriorWeatherStatusName(ExteriorWeatherStatus status) noexcept {
    switch (status) {
    case ExteriorWeatherStatus::NoExteriorObservation: return "no-exterior-observation";
    case ExteriorWeatherStatus::ExteriorObserved: return "exterior-observation-captured";
    case ExteriorWeatherStatus::ExteriorWeatherUnavailable: return "exterior-sky-weather-unavailable";
    case ExteriorWeatherStatus::RegionUntrusted: return "untrusted-climate-region";
    case ExteriorWeatherStatus::InteriorRegionMismatch: return "interior-region-mismatch";
    case ExteriorWeatherStatus::InteriorTimeUnknown: return "interior-time-not-comparable";
    case ExteriorWeatherStatus::InteriorObservationExpired: return "interior-exterior-observation-expired";
    case ExteriorWeatherStatus::InteriorObservationCurrent: return "interior-using-exterior-observation";
    case ExteriorWeatherStatus::CellUnknown: return "cell-unavailable";
    }
    return "unknown";
}

void ExteriorWeatherContext::Clear() noexcept { last_.reset(); }

bool ExteriorWeatherContext::CaptureExterior(bool exteriorCell, bool regionTrusted,
                                            std::string_view regionKey, Weather weather,
                                            std::uint32_t weatherFormId,
                                            std::uint32_t worldspaceFormId, Date at) {
    if (!exteriorCell || !regionTrusted || regionKey.empty() || weatherFormId == 0 ||
        worldspaceFormId == 0 || weather == Weather::Unknown || !ValidClock(at)) return false;
    last_ = ExteriorWeatherObservation{std::string(regionKey), weather,
                                       weatherFormId, worldspaceFormId, at};
    return true;
}

ExteriorWeatherAssessment ExteriorWeatherContext::Assess(const ExteriorWeatherRequest& request) const {
    ExteriorWeatherAssessment result;
    result.observation = last_;
    if (!request.hasCell) {
        result.status = ExteriorWeatherStatus::CellUnknown;
        return result;
    }
    if (!request.regionTrusted || request.regionKey.empty()) {
        result.status = ExteriorWeatherStatus::RegionUntrusted;
        return result;
    }
    if (!request.interior) {
        result.status = last_ ? ExteriorWeatherStatus::ExteriorObserved : ExteriorWeatherStatus::ExteriorWeatherUnavailable;
        return result;
    }
    if (!last_) return result;
    if (last_->regionKey != request.regionKey) {
        result.status = ExteriorWeatherStatus::InteriorRegionMismatch;
        return result;
    }
    // Conservative initial policy: require the same day. An intervening date
    // boundary cannot be timed reliably without a verified game-time source.
    // Reject rather than using a possibly stale exterior reading.
    if (!ValidClock(request.now) || last_->observedAt.year != request.now.year ||
        last_->observedAt.month != request.now.month || last_->observedAt.day != request.now.day ||
        request.now.hour < last_->observedAt.hour) {
        result.status = ExteriorWeatherStatus::InteriorTimeUnknown;
        return result;
    }
    result.ageGameHours = request.now.hour - last_->observedAt.hour;
    if (result.ageGameHours > kMaxAgeGameHours) {
        result.status = ExteriorWeatherStatus::InteriorObservationExpired;
        return result;
    }
    result.status = ExteriorWeatherStatus::InteriorObservationCurrent;
    result.usable = true;
    return result;
}
} // namespace aetherclime
