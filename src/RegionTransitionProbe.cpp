#include "Aetherclime/RegionTransitionProbe.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {
namespace {
[[nodiscard]] bool DefiniteInsideOrOutside(RegionFootprintStatus status) noexcept {
    return status == RegionFootprintStatus::InsideRecordedPolygon ||
           status == RegionFootprintStatus::OutsideRecordedPolygons;
}

[[nodiscard]] bool Valid(const BoundaryFrame& frame) noexcept {
    return frame.candidateSnapshotComplete && frame.cellFormId != 0 &&
           frame.worldspaceFormId != 0 && frame.position &&
           IsFinitePoint(*frame.position);
}
} // namespace

RegionTransitionProbe::RegionTransitionProbe(double maxContinuousStepRawUnits) noexcept
    : maxContinuousStepRawUnits_(
        std::isfinite(maxContinuousStepRawUnits)
            ? std::clamp(maxContinuousStepRawUnits, 256.0, 16384.0)
            : 4096.0) {}

void RegionTransitionProbe::Reset() noexcept { previous_.reset(); }

BoundaryFrameResult RegionTransitionProbe::Observe(const BoundaryFrame& frame) {
    BoundaryFrameResult result;
    if (!Valid(frame)) {
        Reset();
        result.status = BoundaryFrameStatus::ResetUnavailable;
        return result;
    }
    if (!previous_) {
        previous_ = frame;
        result.status = BoundaryFrameStatus::Baseline;
        return result;
    }
    if (previous_->worldspaceFormId != frame.worldspaceFormId) {
        previous_ = frame;
        result.status = BoundaryFrameStatus::ResetWorldspace;
        return result;
    }
    result.fromPosition = previous_->position;
    result.horizontalStepRawUnits = std::hypot(
        frame.position->x - previous_->position->x,
        frame.position->y - previous_->position->y);
    if (!std::isfinite(result.horizontalStepRawUnits) ||
        result.horizontalStepRawUnits > maxContinuousStepRawUnits_) {
        previous_ = frame;
        result.status = BoundaryFrameStatus::ResetLargeJump;
        result.fromPosition.reset();
        return result;
    }

    for (const auto& current : frame.regions) {
        if (current.regionFormId == 0) continue;
        const auto old = std::find_if(previous_->regions.begin(), previous_->regions.end(),
            [&](const BoundaryRegionReading& r) { return r.regionFormId == current.regionFormId; });
        if (old == previous_->regions.end()) continue;
        ++result.sharedCandidates;
        // A region's weather pool, RPLI setting, proximity, or priority cannot
        // substitute for known inside/outside geometry at BOTH sample points.
        if (DefiniteInsideOrOutside(old->footprint) &&
            DefiniteInsideOrOutside(current.footprint) &&
            old->footprint != current.footprint) {
            result.transitions.push_back({current.regionFormId, old->footprint, current.footprint});
        }
    }
    result.status = result.sharedCandidates ? BoundaryFrameStatus::Compared
                                            : BoundaryFrameStatus::NoSharedCandidates;
    previous_ = frame;
    return result;
}
} // namespace aetherclime
