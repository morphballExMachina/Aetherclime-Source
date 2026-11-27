#include "Aetherclime/RegionSpecificCorrelationProbe.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {
namespace {
constexpr double kNearWindowSeconds = 10.0;
[[nodiscard]] bool Valid(const RegionSpecificCorrelationInput& in) noexcept {
    return std::isfinite(in.sessionSeconds) && in.sessionSeconds >= 0.0 &&
        in.sky.exterior && in.sky.skyAvailable && in.sky.worldspaceFormId != 0 &&
        in.sky.currentWeatherFormId != 0 && in.cellFormId != 0;
}
[[nodiscard]] double Since(double now, double then) noexcept {
    return then >= 0 && now >= then ? now - then : -1.0;
}
[[nodiscard]] bool Definite(const BoundaryTransition& crossing) noexcept {
    return crossing.regionFormId != 0 &&
        (crossing.from == RegionFootprintStatus::InsideRecordedPolygon ||
         crossing.from == RegionFootprintStatus::OutsideRecordedPolygons) &&
        (crossing.to == RegionFootprintStatus::InsideRecordedPolygon ||
         crossing.to == RegionFootprintStatus::OutsideRecordedPolygons) &&
        crossing.from != crossing.to;
}
} // namespace

RegionSpecificCorrelationProbe::RegionSpecificCorrelationProbe(std::uint32_t maxReports) noexcept
    : maxReports_(std::clamp(maxReports, 1u, 128u)) {}

void RegionSpecificCorrelationProbe::ClearHistory() noexcept {
    previous_.reset();
    history_ = {};
    latestSwitchFrom_ = 0;
    latestSwitchTo_ = 0;
    latestSwitchSeconds_ = -1.0;
}
void RegionSpecificCorrelationProbe::Reset() noexcept {
    ClearHistory();
    reports_ = 0;
    exhausted_ = false;
}

void RegionSpecificCorrelationProbe::Remember(const BoundaryTransition& crossing, double atSeconds) noexcept {
    if (!Definite(crossing)) return;
    RegionHistory* slot = nullptr;
    for (auto& candidate : history_) {
        if (candidate.region == crossing.regionFormId) { slot = &candidate; break; }
        if (!slot && candidate.region == 0) slot = &candidate;
    }
    if (!slot) {
        // Oldest retained evidence is least helpful when the bounded window
        // is full. This is NOT a cache of live TESRegion pointers.
        slot = &*std::min_element(history_.begin(), history_.end(),
            [](const RegionHistory& a, const RegionHistory& b) { return a.seconds < b.seconds; });
    }
    *slot = RegionHistory{crossing.regionFormId, crossing.from, crossing.to, atSeconds};
}

RegionSpecificCrossingEvidence RegionSpecificCorrelationProbe::LastCrossing(
    std::uint32_t region, double now) const noexcept {
    RegionSpecificCrossingEvidence evidence;
    if (region == 0) return evidence; // null Sky pointer isn't a TESRegion
    for (const auto& entry : history_) {
        if (entry.region != region) continue;
        evidence.available = true;
        evidence.from = entry.from;
        evidence.to = entry.to;
        evidence.secondsBeforeCurrentSample = Since(now, entry.seconds);
        evidence.nearInTime = evidence.secondsBeforeCurrentSample >= 0.0 &&
            evidence.secondsBeforeCurrentSample <= kNearWindowSeconds;
        break;
    }
    return evidence;
}

RegionSpecificCorrelationDecision RegionSpecificCorrelationProbe::Observe(
    const RegionSpecificCorrelationInput& in) noexcept {
    RegionSpecificCorrelationDecision out;
    out.reports = reports_;
    if (exhausted_) { out.exhausted = true; return out; }
    out.cell = in.cellFormId;
    out.currentPointer = in.sky.skyRegionPointerFormId;
    const bool valid = Valid(in);
    if (!valid) {
        if (previous_) { out.status = RegionSpecificStatus::ResetUnavailable; out.report = true; }
        ClearHistory();
    } else if (in.geographyDiscontinuity) {
        ClearHistory();
        out.status = RegionSpecificStatus::ResetGeography;
        out.report = true;
    } else if (previous_ && in.sessionSeconds < previous_->seconds) {
        ClearHistory();
        out.status = RegionSpecificStatus::ResetClock;
        out.report = true;
    } else if (previous_ && in.sky.worldspaceFormId != previous_->worldspace) {
        ClearHistory();
        out.status = RegionSpecificStatus::ResetWorldspace;
        out.report = true;
    } else if (!previous_) {
        out.status = RegionSpecificStatus::Baseline;
        out.report = true;
    } else {
        out.previousPointer = previous_->pointer;
        out.previousCell = previous_->cell;
        out.cellChanged = previous_->cell != in.cellFormId;
        out.pointerChanged = previous_->pointer != in.sky.skyRegionPointerFormId;
        out.weatherPairChanged = previous_->currentWeather != in.sky.currentWeatherFormId ||
            previous_->lastWeather != in.sky.lastWeatherFormId;
        out.sampleGapSeconds = in.sessionSeconds - previous_->seconds;
        // Record every sampled region crossing, even if it isn't currently
        // related to the Sky pointer. The relevant ID might be selected later.
        for (const auto& crossing : in.sampledCrossings) {
            if (!Definite(crossing)) continue;
            ++out.allCrossingCount;
            Remember(crossing, in.sessionSeconds);
        }
        if (out.pointerChanged) {
            latestSwitchFrom_ = out.previousPointer;
            latestSwitchTo_ = out.currentPointer;
            latestSwitchSeconds_ = in.sessionSeconds;
            out.previousPointerCrossing = LastCrossing(out.previousPointer, in.sessionSeconds);
            out.currentPointerCrossing = LastCrossing(out.currentPointer, in.sessionSeconds);
        }
        // Log a crossing only when it touches the pointed-to region or
        // the old/new IDs from the latest recorded pointer switch.
        for (const auto& crossing : in.sampledCrossings) {
            if (!Definite(crossing)) continue;
            const bool current = crossing.regionFormId == out.currentPointer;
            const bool fromSwitch = latestSwitchSeconds_ >= 0.0 &&
                crossing.regionFormId == latestSwitchFrom_ && latestSwitchFrom_ != 0;
            const bool toSwitch = latestSwitchSeconds_ >= 0.0 &&
                crossing.regionFormId == latestSwitchTo_ && latestSwitchTo_ != 0;
            if (!(current || fromSwitch || toSwitch)) continue;
            if (out.relatedCrossingCount >= out.relatedCrossings.size()) break;
            auto& relevant = out.relatedCrossings[out.relatedCrossingCount++];
            relevant.available = true;
            relevant.regionFormId = crossing.regionFormId;
            relevant.from = crossing.from;
            relevant.to = crossing.to;
            relevant.matchesCurrentPointer = current;
            relevant.matchesLatestSwitchFrom = fromSwitch;
            relevant.matchesLatestSwitchTo = toSwitch;
            if (fromSwitch || toSwitch) {
                relevant.secondsAfterPointerSwitch = Since(in.sessionSeconds, latestSwitchSeconds_);
            }
        }
        out.sameSampleCrossingAndPointer = out.pointerChanged && out.allCrossingCount > 0;
        out.secondsSinceLastPointerSwitch = Since(in.sessionSeconds, latestSwitchSeconds_);
        if (out.pointerChanged) {
            out.status = out.sameSampleCrossingAndPointer
                ? RegionSpecificStatus::PointerAndCrossing : RegionSpecificStatus::PointerChanged;
            out.report = true;
        } else if (out.relatedCrossingCount > 0) {
            out.status = RegionSpecificStatus::RelatedCrossing;
            out.report = true;
        }
    }
    if (valid) {
        previous_ = Sample{in.sky.worldspaceFormId, in.cellFormId, in.sky.skyRegionPointerFormId,
            in.sky.currentWeatherFormId, in.sky.lastWeatherFormId, in.sessionSeconds};
    }
    if (out.report) {
        out.reports = ++reports_;
        if (reports_ >= maxReports_) { exhausted_ = true; out.exhausted = true; }
    }
    return out;
}
} // namespace aetherclime
