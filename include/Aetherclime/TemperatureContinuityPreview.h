#pragma once

#include "Aetherclime/TemperatureHandoffAudit.h"

#include <optional>
#include <string_view>

namespace aetherclime {

// Alpha-31: a SHADOW proposal only. Raw readings, climate-profile selection,
// geographic trust, EnvironmentSnapshot V1, and Skyrim weather remain untouched.
// The default-off preview never writes a temperature to the game or API.

[[nodiscard]] bool ValidTemperatureContinuitySettings(const TemperatureContinuitySettings&) noexcept;

// Specific why-not/why-now codes accompany the broad Alpha-31 status.
// These are observational decisions, not an authority to alter temperature.
enum class TemperatureContinuityReason : std::uint8_t {
    DisabledBySetting,
    InvalidSettings,
    MissingExteriorEvidence,
    FirstBaseline,
    ComparisonRejected,
    SameProfile,
    ExistingTransitionCancelled,
    BelowStepThreshold,
    SourceUnchanged,
    DestinationNotWeak,
    ZeroOrInvalidOffset,
    EligibleStep,
    ActiveTransitionDecay,
    LifetimeOrDistanceExhausted,
    NonfiniteCandidate
};
[[nodiscard]] std::string_view TemperatureContinuityReasonName(TemperatureContinuityReason) noexcept;

enum class TemperatureContinuityStatus : std::uint8_t {
    Disabled,
    NoEvidence,
    Baseline,
    NonComparable,
    Unchanged,
    IneligibleHandoff,
    Started,
    Fading,
    Expired
};
[[nodiscard]] std::string_view TemperatureContinuityStatusName(TemperatureContinuityStatus) noexcept;

struct TemperatureContinuityResult {
    TemperatureContinuityStatus status{TemperatureContinuityStatus::Disabled};
    TemperatureContinuityReason reason{TemperatureContinuityReason::DisabledBySetting};
    double rawOutdoorAirC{0.0};
    double previewOutdoorAirC{0.0};
    double previewOffsetC{0.0};
    double pathDistanceRawUnits{0.0};
    double elapsedGameHours{0.0};
    bool candidateActive{false};
    // Diagnostic fade factors, normalized to 0..1. A baseline/inactive
    // candidate reports 1.0; an exhausted candidate reports 0.0 as relevant.
    double distanceRemainingFraction{1.0};
    double timeRemainingFraction{1.0};
    bool compensationCapped{false};
    // Always false: a shadow result is not an authorized engine/API modification.
    bool publishedTemperatureModified{false};
};

// Fed ONLY the one observation already captured by the Alpha-30 audit.
// This uses event/save/load observations, never a PlayerCharacter::Update hook.
class TemperatureContinuityPreview {
public:
    void Reset() noexcept;
    [[nodiscard]] TemperatureContinuityResult Observe(
        const TemperatureHandoffSample& sample,
        const TemperatureHandoffAssessment& handoff,
        const TemperatureContinuitySettings& settings);

private:
    struct ActiveTransition {
        double initialOffsetC{0.0};
        double startingHour{0.0};
        double traveledRawUnits{0.0};
        bool compensationCapped{false};
    };
    std::optional<TemperatureHandoffSample> previous_{};
    std::optional<ActiveTransition> active_{};
    double previousCandidateC_{0.0};
};

} // namespace aetherclime
