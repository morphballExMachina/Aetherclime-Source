#pragma once

#include "Aetherclime/TemperatureHandoffAudit.h"
#include "Aetherclime/GeographicFallbackReason.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {

// Alpha-34: event-sampled, bounded REVIEW CANDIDATES, never verified geographic
// holes, hold boundaries, causes of thermal changes or new climate observations.
enum class GeographicGapKind : std::uint8_t {
    FallbackEntry,
    FallbackExit,
    ProvisionalProfileStep,
    OtherSignificantProfileStep
};
[[nodiscard]] std::string_view GeographicGapKindName(GeographicGapKind) noexcept;

enum class GeographicGapPriority : std::uint8_t { Context = 1, Review = 2, High = 3 };
[[nodiscard]] std::string_view GeographicGapPriorityName(GeographicGapPriority) noexcept;

struct GeographicGapCandidate {
    GeographicGapKind kind{GeographicGapKind::FallbackEntry};
    GeographicGapPriority priority{GeographicGapPriority::Context};
    std::uint64_t sampleIndex{0}; // most informative occurrence of this nearby route
    std::uint64_t occurrences{0}; // comparable adjacent events, not unique cells
    std::uint32_t worldspaceFormId{0};
    double previousXRaw{0};
    double previousYRaw{0};
    double currentXRaw{0};
    double currentYRaw{0};
    double distanceRawUnits{0};
    double ageGameHours{0};
    double airDeltaC{0};
    bool profileChanged{false};
    bool significant{false};
    ClimateRegionSource previousSource{ClimateRegionSource::FallbackNoEvidence};
    ClimateRegionSource currentSource{ClimateRegionSource::FallbackNoEvidence};
    std::string previousRegion;
    std::string currentRegion;
    GeographicFallbackFinding previousFallback{};
    GeographicFallbackFinding currentFallback{};
};

struct GeographicGapSummary {
    std::uint64_t comparablePairs{0};
    std::uint64_t fallbackEntries{0};
    std::uint64_t fallbackExits{0};
    std::uint64_t provisionalProfileSteps{0};
    std::uint64_t otherSignificantProfileSteps{0};
    std::uint64_t qualifyingEvents{0};
    std::uint64_t mergedNearbyOccurrences{0};
    std::uint64_t replacedRepresentatives{0};
    std::uint64_t droppedFromTopList{0};
    std::size_t candidateCount{0};
    std::uint64_t corridorMergedOccurrences{0};
    std::uint64_t corridorRepresentativesReplaced{0};
    std::uint64_t corridorListReplacements{0};
    std::uint64_t corridorDroppedFromTopList{0};
    std::size_t corridorCount{0};
};

// Each corridor is a grouped pair of sampled endpoints, never a traced route,
// verified boundary or inferred polygon. "Shortest span" is only the
// narrowest *observed bracket* among the grouped event pairs.
struct GeographicCorridorCandidate {
    std::uint64_t corridorId{0}; // session-local, not a stable geographic ID
    GeographicGapCandidate representative{};
    GeographicGapCandidate routeReference{}; // fixed orientation for reverse-route counts
    GeographicGapPriority priority{GeographicGapPriority::Context}; // highest observed priority
    std::uint64_t occurrences{0};
    std::uint64_t sameDirectionOccurrences{0};
    std::uint64_t reverseDirectionOccurrences{0};
    std::uint64_t firstSampleIndex{0};
    std::uint64_t lastSampleIndex{0};
    double shortestObservedSpanRawUnits{0.0};
    bool observedBothDirections{false};
};

class GeographicGapReport {
public:
    static constexpr std::size_t kMaxCandidates = 6;
    // Raw Bethesda units, used for approximate near-repeat clustering ONLY.
    static constexpr double kNearbyRawUnits = 2048.0;
    static constexpr std::size_t kMaxCorridors = 8;
    static constexpr double kCorridorEndpointMatchRawUnits = 1024.0;

    void Reset() noexcept {
        summary_ = {};
        candidates_ = {};
        corridors_ = {};
        lastFallback_ = {};
        lastCandidate_.reset();
        nextCorridorId_ = 1;
    }
    void Observe(const TemperatureHandoffSample& current,
                 const TemperatureHandoffAssessment& assessment,
                 std::uint64_t sampleIndex,
                 GeographicFallbackFinding currentFallback = {});
    [[nodiscard]] const GeographicGapSummary& Summary() const noexcept { return summary_; }
    // Sorted best-first. Only first Summary().candidateCount entries are valid.
    [[nodiscard]] std::array<GeographicGapCandidate, kMaxCandidates> RankedCandidates() const;
    [[nodiscard]] std::array<GeographicCorridorCandidate, kMaxCorridors> RankedCorridors() const;

    [[nodiscard]] const std::optional<GeographicGapCandidate>& LastCandidate() const noexcept { return lastCandidate_; }

private:
    std::optional<GeographicGapCandidate> lastCandidate_{};
    GeographicGapSummary summary_{};
    std::array<GeographicGapCandidate, kMaxCandidates> candidates_{};
    std::array<GeographicCorridorCandidate, kMaxCorridors> corridors_{};
    GeographicFallbackFinding lastFallback_{};
    std::uint64_t nextCorridorId_{1};
    void RecordCorridor(const GeographicGapCandidate& candidate);
};

} // namespace aetherclime
