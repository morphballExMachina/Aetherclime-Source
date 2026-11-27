#include "Aetherclime/RegionPointerTimingProbe.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {
namespace {
[[nodiscard]] bool Usable(const RegionPointerTimingInput& input) noexcept {
    return std::isfinite(input.sessionSeconds) && input.sessionSeconds >= 0.0 &&
        input.sky.exterior && input.sky.skyAvailable &&
        input.sky.worldspaceFormId != 0 && input.sky.currentWeatherFormId != 0 &&
        input.cellFormId != 0;
}

[[nodiscard]] double TimeSince(double t, double eventT) noexcept {
    return eventT >= 0.0 && t >= eventT ? t - eventT : -1.0;
}
} // namespace

RegionPointerTimingProbe::RegionPointerTimingProbe(std::uint32_t maxReports) noexcept
    : maxReports_(std::clamp(maxReports, 1u, 128u)) {}

void RegionPointerTimingProbe::Reset() noexcept {
    previous_.reset();
    lastCellTime_ = -1.0;
    lastBoundaryTime_ = -1.0;
    lastPointerTime_ = -1.0;
    lastWeatherPairTime_ = -1.0;
    reports_ = 0;
    exhausted_ = false;
}

RegionPointerTimingDecision RegionPointerTimingProbe::Observe(const RegionPointerTimingInput& input) noexcept {
    RegionPointerTimingDecision out;
    out.totalReports = reports_;
    if (exhausted_) {
        out.exhausted = true;
        return out;
    }
    const auto discardHistory = [this]() noexcept {
        previous_.reset();
        lastCellTime_ = -1.0;
        lastBoundaryTime_ = -1.0;
        lastPointerTime_ = -1.0;
        lastWeatherPairTime_ = -1.0;
    };
    const bool usable = Usable(input);
    if (!usable) {
        if (previous_) {
            out.status = RegionPointerTimingStatus::ResetUnavailable;
            out.report = true;
        }
        discardHistory();
    } else if (input.geographyDiscontinuity) {
        discardHistory();
        out.status = RegionPointerTimingStatus::ResetGeography;
        out.report = true;
    } else if (previous_ && input.sessionSeconds < previous_->seconds) {
        discardHistory();
        out.status = RegionPointerTimingStatus::ResetClock;
        out.report = true;
    } else if (previous_ && input.sky.worldspaceFormId != previous_->worldspaceFormId) {
        discardHistory();
        out.status = RegionPointerTimingStatus::ResetWorldspace;
        out.report = true;
    } else if (!previous_) {
        out.status = RegionPointerTimingStatus::Baseline;
        out.report = true;
    } else {
        const auto& prev = *previous_;
        out.previousCellFormId = prev.cellFormId;
        out.previousSkyPointerFormId = prev.skyRegionFormId;
        out.sampleGapSeconds = input.sessionSeconds - prev.seconds;
        out.cellChanged = prev.cellFormId != input.cellFormId;
        out.skyPointerChanged = prev.skyRegionFormId != input.sky.skyRegionPointerFormId;
        out.weatherPairChanged = prev.currentWeatherFormId != input.sky.currentWeatherFormId ||
            prev.lastWeatherFormId != input.sky.lastWeatherFormId;
        out.overrideChanged = prev.overrideWeatherFormId != input.sky.overrideWeatherFormId;
        out.sampledPolygonTransitions = input.sampledPolygonTransitions;

        // A single sampled observation can contain multiple changes; do not
        // impose an order within that interval. Use sample timestamps only.
        if (out.cellChanged) lastCellTime_ = input.sessionSeconds;
        if (out.sampledPolygonTransitions > 0) lastBoundaryTime_ = input.sessionSeconds;
        if (out.skyPointerChanged) lastPointerTime_ = input.sessionSeconds;
        if (out.weatherPairChanged) lastWeatherPairTime_ = input.sessionSeconds;
        const unsigned eventCount = static_cast<unsigned>(out.cellChanged) +
            static_cast<unsigned>(out.sampledPolygonTransitions > 0) +
            static_cast<unsigned>(out.skyPointerChanged) +
            static_cast<unsigned>(out.weatherPairChanged) +
            static_cast<unsigned>(out.overrideChanged);
        if (eventCount) {
            out.status = RegionPointerTimingStatus::Event;
            out.report = true;
            out.simultaneousSample = eventCount > 1;
            if (out.skyPointerChanged) {
                out.pointerSecondsAfterBoundary = TimeSince(input.sessionSeconds, lastBoundaryTime_);
                out.pointerSecondsAfterCell = TimeSince(input.sessionSeconds, lastCellTime_);
            }
            if (out.weatherPairChanged) {
                out.weatherSecondsAfterPointer = TimeSince(input.sessionSeconds, lastPointerTime_);
                out.weatherSecondsAfterBoundary = TimeSince(input.sessionSeconds, lastBoundaryTime_);
            }
            if (out.sampledPolygonTransitions > 0) {
                out.boundarySecondsAfterPointer = TimeSince(input.sessionSeconds, lastPointerTime_);
            }
        }
    }
    if (usable) {
        previous_ = Previous{input.sky.worldspaceFormId, input.cellFormId,
            input.sky.currentWeatherFormId, input.sky.lastWeatherFormId,
            input.sky.overrideWeatherFormId, input.sky.skyRegionPointerFormId,
            input.sessionSeconds};
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
