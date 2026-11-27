#pragma once

#include "Aetherclime/RegionGeometry.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace aetherclime {

// Alpha-10: comparisons of READ-ONLY, periodically sampled *recorded*
// polygon footprint statuses. These do not identify the engine's selected
// weather region or prove the exact crossing point along the player's path.
struct BoundaryRegionReading {
    std::uint32_t regionFormId{0};
    RegionFootprintStatus footprint{RegionFootprintStatus::MissingGeometry};
};

struct BoundaryFrame {
    std::uint32_t cellFormId{0};
    std::uint32_t worldspaceFormId{0};
    std::optional<RegionPoint2> position;
    std::vector<BoundaryRegionReading> regions;
    // False if a safety cap, missing cell/worldspace, or severe inspection
    // failure prevents a complete candidate snapshot.
    bool candidateSnapshotComplete{false};
};

enum class BoundaryFrameStatus {
    Baseline,
    Compared,
    NoSharedCandidates,
    ResetUnavailable,
    ResetWorldspace,
    ResetLargeJump
};

[[nodiscard]] inline const char* BoundaryFrameStatusName(BoundaryFrameStatus status) noexcept {
    switch (status) {
    case BoundaryFrameStatus::Baseline: return "baseline";
    case BoundaryFrameStatus::Compared: return "compared";
    case BoundaryFrameStatus::NoSharedCandidates: return "no-shared-candidates";
    case BoundaryFrameStatus::ResetUnavailable: return "reset-unavailable";
    case BoundaryFrameStatus::ResetWorldspace: return "reset-worldspace";
    case BoundaryFrameStatus::ResetLargeJump: return "reset-large-jump";
    }
    return "unknown";
}

struct BoundaryTransition {
    std::uint32_t regionFormId{0};
    RegionFootprintStatus from{RegionFootprintStatus::MissingGeometry};
    RegionFootprintStatus to{RegionFootprintStatus::MissingGeometry};
};

struct BoundaryFrameResult {
    BoundaryFrameStatus status{BoundaryFrameStatus::ResetUnavailable};
    std::size_t sharedCandidates{0};
    double horizontalStepRawUnits{0.0};
    std::optional<RegionPoint2> fromPosition;
    std::vector<BoundaryTransition> transitions;
};

// One observation per bounded movement report (never called every frame).
// A noncontiguous jump, unknown data or worldspace transition clears the
// baseline rather than manufacturing a false boundary crossing.
class RegionTransitionProbe {
public:
    explicit RegionTransitionProbe(double maxContinuousStepRawUnits = 4096.0) noexcept;
    void Reset() noexcept;
    [[nodiscard]] BoundaryFrameResult Observe(const BoundaryFrame& frame);

private:
    double maxContinuousStepRawUnits_;
    std::optional<BoundaryFrame> previous_;
};

} // namespace aetherclime
