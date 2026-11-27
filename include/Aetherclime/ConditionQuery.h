#pragma once

#include "Aetherclime/EnvironmentalSnapshot.h"
#include <cstddef>
#include <optional>
#include <string_view>

namespace aetherclime {
// Pure, allocation-free evaluation over one owned snapshot. No engine access.
inline constexpr std::size_t kConditionMaxBytes = 1024;
inline constexpr std::size_t kConditionMaxTokens = 256;
inline constexpr unsigned kConditionMaxDepth = 16;
enum class ConditionStatus : std::uint32_t { False, True, Unknown, Invalid };
enum class ConditionError : std::uint32_t {
    None, Empty, TooLong, UnexpectedToken, UnknownField, InvalidLiteral,
    InvalidOperator, LimitExceeded, InternalError, InvalidArgument
};
struct ConditionResult {
    ConditionStatus status{ConditionStatus::Invalid};
    ConditionError error{ConditionError::None};
    std::uint32_t errorOffset{0}; // zero-based byte offset; meaningful for Invalid
};
[[nodiscard]] std::string_view ConditionStatusName(ConditionStatus) noexcept;
[[nodiscard]] ConditionResult EvaluateCondition(std::string_view rule,
    const EnvironmentSnapshot& state, std::optional<Season> season = {}) noexcept;
// Portable native-ABI adapter, also used by the DLL export. Return 1 means the
// result was written, NOT that the rule matched. Return 0 = invalid output buffer.
std::int32_t WriteConditionResultV1(const char* rule, std::uint32_t ruleBytes,
    const EnvironmentSnapshot& state, std::optional<Season> season,
    AetherclimeConditionResultV1* output, std::uint32_t outputBytes) noexcept;
bool ValidConditionBatchEnvelope(const AetherclimeConditionRuleV1*, std::uint32_t count,
    std::uint32_t descriptorBytes, AetherclimeConditionBatchV1*, std::uint32_t outputBytes) noexcept;
std::int32_t WriteConditionBatchV1(const AetherclimeConditionRuleV1*, std::uint32_t count,
    std::uint32_t descriptorBytes, const EnvironmentSnapshot& state,
    AetherclimeConditionBatchV1*, std::uint32_t outputBytes, bool captureFailed = false) noexcept;
} // namespace aetherclime
