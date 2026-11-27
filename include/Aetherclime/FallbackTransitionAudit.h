#pragma once
// Alpha-50: bounded observation-only report gate for uncertain outdoor handoffs.
// No engine dependencies, no weather writes, no subscriptions or polling.
#include "Aetherclime/ClimateEstimates.h"
#include <cstdint>
#include <optional>

namespace aetherclime {
enum class FallbackReportReason { None, Entered, BasisChanged, NearbyTemperatureStep };
const char* FallbackReportReasonName(FallbackReportReason) noexcept;
struct FallbackTransitionSample {
    bool interior{false};
    OutdoorEstimateBasis basis{OutdoorEstimateBasis::Unavailable};
    std::optional<double> estimateC;
    std::uint32_t worldspaceFormId{0};
    std::uint32_t cellFormId{0};
    std::optional<double> xRaw, yRaw;
};
struct FallbackTransitionDecision {
    FallbackReportReason reason{FallbackReportReason::None};
    unsigned reportNumber{0};
    bool Report() const noexcept {return reason!=FallbackReportReason::None;}
};
class FallbackTransitionAudit {
public:
    static constexpr unsigned kMaxReports=12;
    static constexpr double kNearbyDistanceRaw=8192.0; // two exterior cells
    static constexpr double kTemperatureStepC=8.0;
    void Reset() noexcept {prior_.reset(); reports_=0;}
    FallbackTransitionDecision Observe(const FallbackTransitionSample&) noexcept;
    unsigned Reports() const noexcept {return reports_;}
private:
    std::optional<FallbackTransitionSample> prior_;
    unsigned reports_{0};
};
} // namespace aetherclime
