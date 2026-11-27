#include "Aetherclime/WeatherCorrelationProbe.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {
namespace {
[[nodiscard]] bool Usable(const WeatherSkyReading& r, double seconds) noexcept {
    return std::isfinite(seconds) && seconds >= 0.0 && r.exterior &&
        r.skyAvailable && r.worldspaceFormId != 0 && r.currentWeatherFormId != 0;
}

[[nodiscard]] bool WeatherIdsDiffer(const WeatherSkyReading& a,
                                    const WeatherSkyReading& b) noexcept {
    return a.currentWeatherFormId != b.currentWeatherFormId ||
           a.lastWeatherFormId != b.lastWeatherFormId ||
           a.overrideWeatherFormId != b.overrideWeatherFormId;
}
} // namespace

WeatherCorrelationProbe::WeatherCorrelationProbe(std::uint32_t maxReports) noexcept
    : maxReports_(std::clamp(maxReports, 1u, 256u)) {}

void WeatherCorrelationProbe::Reset() noexcept {
    previous_.reset();
    lastCrossingTime_ = -1.0;
    nextFollowUp_ = 0;
    reports_ = 0;
    exhausted_ = false;
}

WeatherCorrelationDecision WeatherCorrelationProbe::Observe(
    const WeatherSkyReading& reading, double secondsSinceSessionStart,
    std::uint32_t sampledBoundaryTransitions, bool geographyDiscontinuity) noexcept {
    WeatherCorrelationDecision result;
    result.blendValueValid = std::isfinite(reading.currentWeatherPct) &&
        reading.currentWeatherPct >= 0.0f && reading.currentWeatherPct <= 1.0f;
    result.totalReports = reports_;
    if (exhausted_) {
        result.exhausted = true;
        return result;
    }

    const bool valid = Usable(reading, secondsSinceSessionStart);
    if (!valid) {
        const bool hadBaseline = previous_.has_value();
        previous_.reset();
        lastCrossingTime_ = -1.0;
        nextFollowUp_ = 0;
        if (hadBaseline) {
            result.status = WeatherCorrelationStatus::ResetUnavailable;
            result.report = true;
        }
    } else if (geographyDiscontinuity) {
        // Lost continuity (e.g. a coc/fast travel or incomplete geometry).
        // Clear attribution windows, but preserve report budget for this load.
        previous_ = reading;
        lastCrossingTime_ = -1.0;
        nextFollowUp_ = 0;
        result.status = WeatherCorrelationStatus::ResetGeography;
        result.report = true;
    } else if (!previous_) {
        previous_ = reading;
        result.status = WeatherCorrelationStatus::Baseline;
        result.report = true;
    } else if (reading.worldspaceFormId != previous_->worldspaceFormId) {
        previous_ = reading;
        lastCrossingTime_ = -1.0;
        nextFollowUp_ = 0;
        result.status = WeatherCorrelationStatus::ResetWorldspace;
        result.report = true;
    } else {
        result.previousCurrentWeatherFormId = previous_->currentWeatherFormId;
        result.previousLastWeatherFormId = previous_->lastWeatherFormId;
        result.weatherIdsChanged = WeatherIdsDiffer(*previous_, reading);
        result.skyRegionPointerChanged =
            previous_->skyRegionPointerFormId != reading.skyRegionPointerFormId;
        result.boundaryTransitions = sampledBoundaryTransitions;
        if (sampledBoundaryTransitions > 0) {
            lastCrossingTime_ = secondsSinceSessionStart;
            nextFollowUp_ = 0;
            result.status = result.weatherIdsChanged
                ? WeatherCorrelationStatus::BoundaryAndWeatherIdsChanged
                : WeatherCorrelationStatus::BoundarySampled;
            result.report = true;
        } else if (result.weatherIdsChanged) {
            result.status = WeatherCorrelationStatus::WeatherIdsChanged;
            result.report = true;
        } else if (result.skyRegionPointerChanged) {
            result.status = WeatherCorrelationStatus::SkyRegionPointerChanged;
            result.report = true;
        } else if (lastCrossingTime_ >= 0.0 &&
                   secondsSinceSessionStart >= lastCrossingTime_ &&
                   nextFollowUp_ < 2) {
            result.secondsSinceLastCrossing = secondsSinceSessionStart - lastCrossingTime_;
            if (nextFollowUp_ == 0 && result.secondsSinceLastCrossing >= 10.0) {
                result.status = WeatherCorrelationStatus::FollowUp10s;
                result.report = true;
                ++nextFollowUp_;
            } else if (nextFollowUp_ == 1 && result.secondsSinceLastCrossing >= 30.0) {
                result.status = WeatherCorrelationStatus::FollowUp30s;
                result.report = true;
                ++nextFollowUp_;
            }
        }
        if (lastCrossingTime_ >= 0.0) {
            result.secondsSinceLastCrossing = secondsSinceSessionStart - lastCrossingTime_;
        }
        previous_ = reading;
    }

    if (result.report) {
        ++reports_;
        result.totalReports = reports_;
        if (reports_ >= maxReports_) {
            exhausted_ = true;
            result.exhausted = true;
        }
    }
    return result;
}
} // namespace aetherclime
