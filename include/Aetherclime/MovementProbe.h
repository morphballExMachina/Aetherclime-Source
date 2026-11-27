#pragma once

#include "Aetherclime/WorldPosition.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace aetherclime {

// Alpha-9: small, engine-independent, opt-in sampling policy. Neither this
// class nor the game adapter retains a pointer to a Skyrim form.
struct MovementProbeOptions {
    double minimumDistanceRawUnits{256.0};
    std::uint32_t maxSamples{600};
    std::uint32_t maxReports{80};
};

struct MovementPosition {
    std::uint32_t cellFormId{0};
    std::uint32_t worldspaceFormId{0};
    // Absent in interiors, missing worldspaces and invalid coordinates.
    std::optional<RawPosition> exteriorWorldPosition;
};

enum class MovementReason {
    None,
    Initial,
    Distance,
    CellChanged,
    WorldspaceChanged,
    PositionAvailabilityChanged
};

[[nodiscard]] std::string_view MovementReasonName(MovementReason reason) noexcept;

struct MovementDecision {
    bool report{false};
    bool exhausted{false};
    MovementReason reason{MovementReason::None};
    double distanceRawUnits{0.0};
};

class MovementProbe {
public:
    explicit MovementProbe(MovementProbeOptions options = {});
    void Reset() noexcept;
    [[nodiscard]] MovementDecision Observe(const MovementPosition& current) noexcept;
    [[nodiscard]] bool Exhausted() const noexcept { return exhausted_; }
    [[nodiscard]] std::uint32_t Samples() const noexcept { return sampleCount_; }
    [[nodiscard]] std::uint32_t Reports() const noexcept { return reportCount_; }

private:
    MovementProbeOptions options_;
    std::optional<MovementPosition> lastReported_;
    std::uint32_t sampleCount_{0};
    std::uint32_t reportCount_{0};
    bool exhausted_{false};
};

} // namespace aetherclime
