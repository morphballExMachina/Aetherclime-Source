#include "Aetherclime/SkyTransitionLifecycleProbe.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {
namespace {
[[nodiscard]] bool Usable(const WeatherSkyReading& r, double seconds) noexcept {
    return std::isfinite(seconds) && seconds >= 0.0 && r.exterior &&
        r.skyAvailable && r.worldspaceFormId != 0 && r.currentWeatherFormId != 0;
}
[[nodiscard]] bool BlendValid(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}
[[nodiscard]] int Quarter(float value) noexcept {
    if (!BlendValid(value)) return -1;
    if (value >= 1.0f) return 4;
    if (value >= 0.75f) return 3;
    if (value >= 0.5f) return 2;
    if (value >= 0.25f) return 1;
    return 0;
}
[[nodiscard]] SkyWeatherPairState PairState(const WeatherSkyReading& r) noexcept {
    if (r.currentWeatherFormId == 0) return SkyWeatherPairState::Unavailable;
    return r.lastWeatherFormId == 0 ? SkyWeatherPairState::CurrentOnly :
                                       SkyWeatherPairState::CurrentAndLast;
}
} // namespace

SkyTransitionLifecycleProbe::SkyTransitionLifecycleProbe(std::uint32_t maxReports) noexcept
    : maxReports_(std::clamp(maxReports, 1u, 128u)) {}

void SkyTransitionLifecycleProbe::Reset() noexcept {
    previous_.reset();
    previousSampleTime_ = -1.0;
    weatherPairStartTime_ = -1.0;
    reports_ = 0;
    exhausted_ = false;
}

SkyLifecycleDecision SkyTransitionLifecycleProbe::Observe(
    const WeatherSkyReading& reading, double elapsedSeconds,
    bool geographyDiscontinuity) noexcept {
    SkyLifecycleDecision out;
    out.blendValid = BlendValid(reading.currentWeatherPct);
    out.blendQuarter = Quarter(reading.currentWeatherPct);
    out.pairState = Usable(reading, elapsedSeconds) ? PairState(reading) : SkyWeatherPairState::Unavailable;
    out.totalReports = reports_;
    if (exhausted_) {
        out.exhausted = true;
        return out;
    }
    if (!Usable(reading, elapsedSeconds)) {
        if (previous_) {
            out.report = true;
            out.status = SkyLifecycleStatus::ResetUnavailable;
        }
        previous_.reset();
        previousSampleTime_ = -1.0;
        weatherPairStartTime_ = -1.0;
    } else if (geographyDiscontinuity) {
        previous_ = reading;
        previousSampleTime_ = elapsedSeconds;
        weatherPairStartTime_ = -1.0;
        out.report = true;
        out.status = SkyLifecycleStatus::ResetGeography;
    } else if (!previous_) {
        previous_ = reading;
        previousSampleTime_ = elapsedSeconds;
        weatherPairStartTime_ = elapsedSeconds;
        out.report = true;
        out.status = SkyLifecycleStatus::Baseline;
    } else if (previous_->worldspaceFormId != reading.worldspaceFormId ||
               elapsedSeconds < previousSampleTime_) {
        previous_ = reading;
        previousSampleTime_ = elapsedSeconds;
        weatherPairStartTime_ = -1.0;
        out.report = true;
        out.status = SkyLifecycleStatus::ResetWorldspace;
    } else {
        const auto& old = *previous_;
        out.previousCurrent = old.currentWeatherFormId;
        out.previousLast = old.lastWeatherFormId;
        out.previousOverride = old.overrideWeatherFormId;
        out.previousSkyRegion = old.skyRegionPointerFormId;
        out.previousBlendQuarter = Quarter(old.currentWeatherPct);
        out.secondsSincePreviousSample = elapsedSeconds - previousSampleTime_;
        out.pairChanged = old.currentWeatherFormId != reading.currentWeatherFormId ||
                          old.lastWeatherFormId != reading.lastWeatherFormId;
        out.lastPointerCleared = old.lastWeatherFormId != 0 && reading.lastWeatherFormId == 0;
        out.overrideChanged = old.overrideWeatherFormId != reading.overrideWeatherFormId;
        out.skyRegionChanged = old.skyRegionPointerFormId != reading.skyRegionPointerFormId;
        // A coarse progress milestone is meaningful only for the same
        // current/last weather pair with both values in [0,1].
        out.blendQuarterChanged = !out.pairChanged && out.blendValid &&
            out.previousBlendQuarter >= 0 && out.blendQuarter != out.previousBlendQuarter;
        if (out.pairChanged) {
            // Preserve the observed duration of the previous weather pair
            // when the last-weather pointer clears. This is NOT a claim that
            // the engine's transition completed at this instant.
            if (out.lastPointerCleared && weatherPairStartTime_ >= 0.0) {
                out.secondsSincePairChange = elapsedSeconds - weatherPairStartTime_;
            }
            weatherPairStartTime_ = elapsedSeconds;
            out.status = out.lastPointerCleared ? SkyLifecycleStatus::LastPointerCleared :
                                                  SkyLifecycleStatus::WeatherPairChanged;
            out.report = true;
        } else if (out.overrideChanged) {
            out.status = SkyLifecycleStatus::OverridePointerChanged;
            out.report = true;
        } else if (out.skyRegionChanged) {
            out.status = SkyLifecycleStatus::SkyRegionPointerChanged;
            out.report = true;
        } else if (out.blendQuarterChanged) {
            out.status = SkyLifecycleStatus::BlendQuarterChanged;
            out.report = true;
        }
        if (weatherPairStartTime_ >= 0.0 && out.secondsSincePairChange < 0.0) {
            out.secondsSincePairChange = elapsedSeconds - weatherPairStartTime_;
        }
        previous_ = reading;
        previousSampleTime_ = elapsedSeconds;
    }
    if (out.report) {
        ++reports_;
        out.totalReports = reports_;
        if (reports_ >= maxReports_) {
            exhausted_ = true;
            out.exhausted = true;
        }
    }
    return out;
}

} // namespace aetherclime
