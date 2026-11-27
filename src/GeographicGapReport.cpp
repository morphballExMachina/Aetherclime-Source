#include "Aetherclime/GeographicGapReport.h"

#include "Aetherclime/GeographicEvidence.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace aetherclime {
namespace {

int Rank(const GeographicGapCandidate& c) noexcept {
    return static_cast<int>(c.priority);
}

bool Better(const GeographicGapCandidate& a, const GeographicGapCandidate& b) noexcept {
    if (Rank(a) != Rank(b)) return Rank(a) > Rank(b);
    const double aa = std::abs(a.airDeltaC);
    const double bb = std::abs(b.airDeltaC);
    if (aa != bb) return aa > bb;
    return a.sampleIndex < b.sampleIndex;
}

// Match BOTH observed endpoints and the source/profile pair. Reversing the
// walking direction is allowed; a shared midpoint alone is not sufficient.
// This clustering tolerance is for log review, never geometric inference.
bool SameEndpoint(double ax, double ay, double bx, double by) noexcept {
    return std::hypot(ax - bx, ay - by) <=
           GeographicGapReport::kCorridorEndpointMatchRawUnits;
}

// 0 = unrelated; 1 = same ordered route; 2 = reversed source/profile route.
int CorridorDirection(const GeographicGapCandidate& reference,
                      const GeographicGapCandidate& incoming) noexcept {
    if (reference.worldspaceFormId != incoming.worldspaceFormId) return 0;
    const bool sameLabels = reference.previousSource == incoming.previousSource &&
        reference.currentSource == incoming.currentSource &&
        reference.previousRegion == incoming.previousRegion &&
        reference.currentRegion == incoming.currentRegion;
    if (sameLabels &&
        SameEndpoint(reference.previousXRaw, reference.previousYRaw,
                     incoming.previousXRaw, incoming.previousYRaw) &&
        SameEndpoint(reference.currentXRaw, reference.currentYRaw,
                     incoming.currentXRaw, incoming.currentYRaw)) return 1;
    const bool reverseLabels = reference.previousSource == incoming.currentSource &&
        reference.currentSource == incoming.previousSource &&
        reference.previousRegion == incoming.currentRegion &&
        reference.currentRegion == incoming.previousRegion;
    if (reverseLabels &&
        SameEndpoint(reference.previousXRaw, reference.previousYRaw,
                     incoming.currentXRaw, incoming.currentYRaw) &&
        SameEndpoint(reference.currentXRaw, reference.currentYRaw,
                     incoming.previousXRaw, incoming.previousYRaw)) return 2;
    return 0;
}

bool BetterCorridor(const GeographicCorridorCandidate& a,
                    const GeographicCorridorCandidate& b) noexcept {
    if (a.priority != b.priority)
        return static_cast<int>(a.priority) > static_cast<int>(b.priority);
    if (a.observedBothDirections != b.observedBothDirections)
        return a.observedBothDirections;
    if (a.occurrences != b.occurrences) return a.occurrences > b.occurrences;
    if (std::abs(a.representative.airDeltaC) != std::abs(b.representative.airDeltaC))
        return std::abs(a.representative.airDeltaC) > std::abs(b.representative.airDeltaC);
    return a.firstSampleIndex < b.firstSampleIndex;
}

bool NearbyRepeat(const GeographicGapCandidate& a, const GeographicGapCandidate& b) noexcept {
    return a.kind == b.kind && a.worldspaceFormId == b.worldspaceFormId &&
           a.previousRegion == b.previousRegion && a.currentRegion == b.currentRegion &&
           a.previousSource == b.previousSource && a.currentSource == b.currentSource &&
           std::hypot(a.currentXRaw - b.currentXRaw, a.currentYRaw - b.currentYRaw) <=
               GeographicGapReport::kNearbyRawUnits;
}

} // namespace

std::string_view GeographicGapKindName(GeographicGapKind kind) noexcept {
    switch (kind) {
    case GeographicGapKind::FallbackEntry: return "fallback-entry";
    case GeographicGapKind::FallbackExit: return "fallback-exit";
    case GeographicGapKind::ProvisionalProfileStep: return "provisional-profile-step";
    case GeographicGapKind::OtherSignificantProfileStep: return "other-significant-profile-step";
    }
    return "unknown";
}

std::string_view GeographicGapPriorityName(GeographicGapPriority priority) noexcept {
    switch (priority) {
    case GeographicGapPriority::Context: return "context";
    case GeographicGapPriority::Review: return "review";
    case GeographicGapPriority::High: return "high";
    }
    return "unknown";
}

void GeographicGapReport::Observe(const TemperatureHandoffSample& current,
                                   const TemperatureHandoffAssessment& assessment,
                                   std::uint64_t sampleIndex,
                                   GeographicFallbackFinding currentFallback) {
    lastCandidate_.reset();
    const auto previousFinding = lastFallback_;
    // An invalid exterior breaks the previous-finding context; a new valid
    // baseline establishes one without creating a candidate.
    lastFallback_ = assessment.status == TemperatureHandoffStatus::NoExteriorEvidence
        ? GeographicFallbackFinding{} : currentFallback;
    // Never bridge a save/load baseline, weather change, teleport, different
    // worldspace, invalid exterior or unknown previous coordinates.
    if (!assessment.comparable || !assessment.previousPosition || !current.position) return;
    ++summary_.comparablePairs;
    if (!assessment.sourceChanged && !assessment.profileChanged) return;

    const bool previousIsFallback =
        ClassifyGeographicEvidence(assessment.previousSource) == GeographicEvidenceTier::Fallback;
    const bool currentIsFallback =
        ClassifyGeographicEvidence(current.source) == GeographicEvidenceTier::Fallback;

    GeographicGapCandidate candidate;
    if (!previousIsFallback && currentIsFallback && assessment.sourceChanged) {
        candidate.kind = GeographicGapKind::FallbackEntry;
        ++summary_.fallbackEntries;
    } else if (previousIsFallback && !currentIsFallback && assessment.sourceChanged) {
        candidate.kind = GeographicGapKind::FallbackExit;
        ++summary_.fallbackExits;
    } else if (assessment.profileChanged &&
               (ClassifyGeographicEvidence(current.source) == GeographicEvidenceTier::Provisional ||
                ClassifyGeographicEvidence(assessment.previousSource) == GeographicEvidenceTier::Provisional)) {
        candidate.kind = GeographicGapKind::ProvisionalProfileStep;
        ++summary_.provisionalProfileSteps;
    } else if (assessment.profileChanged && assessment.significant) {
        candidate.kind = GeographicGapKind::OtherSignificantProfileStep;
        ++summary_.otherSignificantProfileSteps;
    } else {
        return;
    }
    ++summary_.qualifyingEvents;

    if ((candidate.kind == GeographicGapKind::FallbackEntry ||
         candidate.kind == GeographicGapKind::FallbackExit) && assessment.profileChanged && assessment.significant) {
        candidate.priority = GeographicGapPriority::High;
    } else if (candidate.kind == GeographicGapKind::FallbackEntry ||
               candidate.kind == GeographicGapKind::FallbackExit ||
               assessment.significant) {
        candidate.priority = GeographicGapPriority::Review;
    } else {
        candidate.priority = GeographicGapPriority::Context;
    }
    candidate.sampleIndex = sampleIndex;
    candidate.occurrences = 1;
    candidate.worldspaceFormId = current.position->worldspaceFormId;
    candidate.previousXRaw = assessment.previousPosition->xRaw;
    candidate.previousYRaw = assessment.previousPosition->yRaw;
    candidate.currentXRaw = current.position->xRaw;
    candidate.currentYRaw = current.position->yRaw;
    candidate.distanceRawUnits = assessment.distanceRawUnits;
    candidate.ageGameHours = assessment.ageGameHours;
    candidate.airDeltaC = assessment.airDeltaC;
    candidate.profileChanged = assessment.profileChanged;
    candidate.significant = assessment.significant;
    candidate.previousSource = assessment.previousSource;
    candidate.currentSource = current.source;
    candidate.previousRegion = assessment.previousRegion;
    candidate.currentRegion = current.regionKey;
    candidate.previousFallback = previousFinding;
    candidate.currentFallback = currentFallback;

    // Corridor observations include ALL qualifying events, not only the six
    // retained Alpha-34 representatives. A rejected pair never reaches here.
    lastCandidate_ = candidate;
    RecordCorridor(candidate);

    // Consolidate repeated source flicker in the same small vicinity and route.
    for (std::size_t i = 0; i < summary_.candidateCount; ++i) {
        if (!NearbyRepeat(candidates_[i], candidate)) continue;
        const auto occurrences = candidates_[i].occurrences + 1;
        if (Better(candidate, candidates_[i])) {
            candidates_[i] = std::move(candidate);
            ++summary_.replacedRepresentatives;
        }
        candidates_[i].occurrences = occurrences;
        ++summary_.mergedNearbyOccurrences;
        return;
    }

    if (summary_.candidateCount < kMaxCandidates) {
        candidates_[summary_.candidateCount++] = std::move(candidate);
        return;
    }
    // Fixed memory and bounded output: keep the most actionable examples, not
    // just the first six encounters in a long expedition.
    auto worst = std::size_t{0};
    for (std::size_t i = 1; i < kMaxCandidates; ++i) {
        if (Better(candidates_[worst], candidates_[i])) worst = i;
    }
    if (Better(candidate, candidates_[worst])) {
        candidates_[worst] = std::move(candidate);
        ++summary_.replacedRepresentatives;
    } else {
        ++summary_.droppedFromTopList;
    }
}

void GeographicGapReport::RecordCorridor(const GeographicGapCandidate& candidate) {
    for (std::size_t i = 0; i < summary_.corridorCount; ++i) {
        auto& corridor = corridors_[i];
        const int direction = CorridorDirection(corridor.routeReference, candidate);
        if (direction == 0) continue;
        ++corridor.occurrences;
        if (static_cast<int>(candidate.priority) > static_cast<int>(corridor.priority))
            corridor.priority = candidate.priority;
        corridor.lastSampleIndex = candidate.sampleIndex;
        if (direction == 1) ++corridor.sameDirectionOccurrences;
        else ++corridor.reverseDirectionOccurrences;
        corridor.observedBothDirections =
            corridor.sameDirectionOccurrences > 0 && corridor.reverseDirectionOccurrences > 0;
        // Only replace with a truly shorter event bracket. The new endpoints
        // don't establish an exact boundary, even after multiple crossings.
        if (candidate.distanceRawUnits < corridor.shortestObservedSpanRawUnits) {
            corridor.representative = candidate;
            corridor.shortestObservedSpanRawUnits = candidate.distanceRawUnits;
            // Preserve an explicit direction reference independent of the
            // chosen representative, which may be from the return trip.
            ++summary_.corridorRepresentativesReplaced;
        }
        ++summary_.corridorMergedOccurrences;
        return;
    }

    GeographicCorridorCandidate incoming;
    incoming.representative = candidate;
    incoming.routeReference = candidate;
    incoming.priority = candidate.priority;
    incoming.occurrences = 1;
    incoming.sameDirectionOccurrences = 1;
    incoming.firstSampleIndex = candidate.sampleIndex;
    incoming.lastSampleIndex = candidate.sampleIndex;
    incoming.shortestObservedSpanRawUnits = candidate.distanceRawUnits;

    if (summary_.corridorCount < kMaxCorridors) {
        incoming.corridorId = nextCorridorId_++;
        corridors_[summary_.corridorCount++] = std::move(incoming);
        return;
    }
    std::size_t worst = 0;
    for (std::size_t i = 1; i < kMaxCorridors; ++i) {
        if (BetterCorridor(corridors_[worst], corridors_[i])) worst = i;
    }
    if (BetterCorridor(incoming, corridors_[worst])) {
        incoming.corridorId = nextCorridorId_++;
        corridors_[worst] = std::move(incoming);
        ++summary_.corridorListReplacements;
    } else {
        ++summary_.corridorDroppedFromTopList;
    }
}

std::array<GeographicCorridorCandidate, GeographicGapReport::kMaxCorridors>
GeographicGapReport::RankedCorridors() const {
    auto sorted = corridors_;
    std::sort(sorted.begin(), sorted.begin() +
        static_cast<std::ptrdiff_t>(summary_.corridorCount), BetterCorridor);
    return sorted;
}

std::array<GeographicGapCandidate, GeographicGapReport::kMaxCandidates>
GeographicGapReport::RankedCandidates() const {
    auto sorted = candidates_;
    std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(summary_.candidateCount), Better);
    return sorted;
}

} // namespace aetherclime
