#pragma once
#include "Aetherclime/EnvironmentalSnapshot.h"
namespace aetherclime {
inline constexpr std::uint32_t kEnvironmentContractChecks=13;
inline constexpr const char* kEnvironmentContractNames[kEnvironmentContractChecks]{
    "snapshot-shape", "v1-v2-consistency", "regional-trust", "weather-provenance",
    "local-ambient-eligibility", "interior-biome-exclusion", "preview-availability",
    "calendar-validity", "regional-query-gate", "ambient-query-gate", "biome-query-gate",
    "weather-query-gate", "preview-query-gate"
};
struct EnvironmentContextAudit {
    std::uint32_t failedMask{};
    [[nodiscard]] bool Passed() const noexcept {return failedMask==0;}
};
// Diagnostic categories describe sampled data, not geographic coverage or game state.
[[nodiscard]] const char* EnvironmentContextName(const EnvironmentSnapshot&) noexcept;
// Pure read-only check of an already-owned snapshot. No new engine reads.
[[nodiscard]] EnvironmentContextAudit AuditEnvironmentContext(const EnvironmentSnapshot&) noexcept;
} // namespace aetherclime
