#pragma once
// Standalone consumer harness: public ABI only, no AetherclimeCore or SKSE types.
#include "Aetherclime/NativeClient.h"
#include <cstdint>

namespace aetherclime::probe {
inline constexpr std::uint32_t kCheckCount=26;
inline constexpr char kMarker[]="AETHERCLIME_API_CONSUMER_PROBE_ALPHA50";
// A result is SKIP, PASS or FAIL. Skipped work never counts as verified.
enum CheckStatus : std::uint32_t { Skipped=0, Passed=1, Failed=2 };
inline constexpr const char* kCheckNames[kCheckCount]{
    "required-exports", "legacy-api-version", "snapshot-v1-contract", "snapshot-v2-contract",
    "stable-v1-v2-consistency", "constant-queries", "invalid-branch-rejection", "calendar-query",
    "legacy-temperature-query", "biome-weight-query", "preview-temperature-query", "boolean-precedence",
    "caller-buffer-contract", "input-length-contract", "typed-literal-rejection", "v2-optional-invariants", "batch-snapshot-consistency", "batch-caller-contract", "client-helper-queries", "client-helper-batch", "context-availability-queries",
    "snapshot-v3-export", "snapshot-v3-shape", "snapshot-v3-v2-compatibility",
    "snapshot-v3-buffer-contract", "client-helper-v3"
};
using ApiFunctions = aetherclime::client::Functions;
struct ReportV1 {
    std::uint32_t structSize, schemaVersion, resolvedMask, completed;
    std::uint32_t passed, failed, skipped, contextAvailable;
    std::uint32_t checks[kCheckCount];
    AetherclimeEnvironmentSnapshotV2 observed;
    AetherclimeEnvironmentSnapshotV3 observedV3;
    char marker[64];
};
// Main/game-thread caller required for real APIs. One bounded run, no retained
// pointers, callbacks, file IO, polling, or gameplay operations.
ReportV1 RunChecks(const ApiFunctions&) noexcept;
using RunProbe = std::int32_t (*)(ReportV1*,std::uint32_t) noexcept;
} // namespace aetherclime::probe
