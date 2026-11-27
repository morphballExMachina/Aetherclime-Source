#include "Aetherclime/MovementProbe.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {

std::string_view MovementReasonName(MovementReason reason) noexcept {
    switch (reason) {
    case MovementReason::Initial: return "initial";
    case MovementReason::Distance: return "distance";
    case MovementReason::CellChanged: return "cell-changed";
    case MovementReason::WorldspaceChanged: return "worldspace-changed";
    case MovementReason::PositionAvailabilityChanged: return "position-availability-changed";
    default: return "none";
    }
}

MovementProbe::MovementProbe(MovementProbeOptions options) : options_(options) {
    if (!std::isfinite(options_.minimumDistanceRawUnits)) options_.minimumDistanceRawUnits = 256.0;
    options_.minimumDistanceRawUnits = std::clamp(options_.minimumDistanceRawUnits, 32.0, 8192.0);
    options_.maxSamples = std::clamp(options_.maxSamples, 1U, 3600U);
    options_.maxReports = std::clamp(options_.maxReports, 1U, 600U);
}

void MovementProbe::Reset() noexcept {
    lastReported_.reset();
    sampleCount_ = 0;
    reportCount_ = 0;
    exhausted_ = false;
}

MovementDecision MovementProbe::Observe(const MovementPosition& current) noexcept {
    MovementDecision decision;
    if (exhausted_) return {.exhausted = true};
    ++sampleCount_;
    MovementPosition sanitized = current;
    if (sanitized.worldspaceFormId == 0 || sanitized.cellFormId == 0) {
        sanitized.exteriorWorldPosition.reset();
    }
    if (sanitized.exteriorWorldPosition) {
        const auto& p = *sanitized.exteriorWorldPosition;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
            sanitized.exteriorWorldPosition.reset();
        }
    }

    if (!lastReported_) {
        decision.reason = MovementReason::Initial;
    } else if (sanitized.worldspaceFormId != lastReported_->worldspaceFormId) {
        decision.reason = MovementReason::WorldspaceChanged;
    } else if (sanitized.cellFormId != lastReported_->cellFormId) {
        decision.reason = MovementReason::CellChanged;
    } else if (static_cast<bool>(sanitized.exteriorWorldPosition) !=
               static_cast<bool>(lastReported_->exteriorWorldPosition)) {
        decision.reason = MovementReason::PositionAvailabilityChanged;
    } else if (sanitized.exteriorWorldPosition) {
        const auto& old = *lastReported_->exteriorWorldPosition;
        const auto& cur = *sanitized.exteriorWorldPosition;
        // Horizontal travel only: do not interpret vertical dungeon/terrain
        // changes as a crossing of a region's two-dimensional boundary.
        decision.distanceRawUnits = std::hypot(cur.x - old.x, cur.y - old.y);
        if (std::isfinite(decision.distanceRawUnits) &&
            decision.distanceRawUnits >= options_.minimumDistanceRawUnits) {
            decision.reason = MovementReason::Distance;
        }
    }

    if (decision.reason != MovementReason::None) {
        decision.report = true;
        lastReported_ = sanitized;
        ++reportCount_;
    }
    exhausted_ = sampleCount_ >= options_.maxSamples || reportCount_ >= options_.maxReports;
    decision.exhausted = exhausted_;
    return decision;
}

} // namespace aetherclime
