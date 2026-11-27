#pragma once

#include "Aetherclime/RegionGeometry.h"
#include "Aetherclime/RegionEdgeAudit.h"
#include "Aetherclime/RegionPolygonAudit.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace aetherclime {

// A read-only copy of live region-weather entries. FormIDs are runtime IDs;
// the load-index bytes can vary with each modlist and should not be persisted
// as stable identifiers in external configuration files.
struct RegionWeatherChoice {
    std::uint32_t weatherFormId{0};
    std::uint32_t rawWeight{0};
    std::uint32_t chanceGlobalFormId{0};  // 0 means no global reference
};

struct RegionWeatherCandidate {
    std::uint32_t regionFormId{0};
    std::uint32_t worldspaceFormId{0};
    std::uint8_t priority{0};
    bool overrideFlag{false};
    RegionGeometryAssessment footprint;  // diagnostic only; never an engine selection claim
    RegionEdgeEvidence edgeEvidence;  // nearest *recorded* segment, not engine fall-off
    RegionPolygonAudit polygonAudit;  // per-polygon RPLI inventory, not a blend model
    std::vector<RegionWeatherChoice> choices;
};

struct RegionWeatherSummary {
    std::uint64_t rawWeightTotal{0};
    std::size_t choices{0};
    std::size_t choicesWithGlobal{0};
    bool containsObservedWeather{false};
};

[[nodiscard]] inline RegionWeatherSummary SummarizeRegionWeather(
    const RegionWeatherCandidate& region, std::uint32_t observedWeatherId) noexcept {
    RegionWeatherSummary result;
    result.choices = region.choices.size();
    for (const auto& choice : region.choices) {
        result.rawWeightTotal += choice.rawWeight; // uint32 input, uint64 output
        result.choicesWithGlobal += (choice.chanceGlobalFormId != 0);
        result.containsObservedWeather |=
            (observedWeatherId != 0 && observedWeatherId == choice.weatherFormId);
    }
    return result;
}

// Sort for human-readable diagnostics ONLY. This does NOT reproduce the
// engine's geographic/polygon selection or guarantee an active weather region.
inline void SortWeatherCandidatesForDisplay(std::vector<RegionWeatherCandidate>& regions) {
    std::sort(regions.begin(), regions.end(),
        [](const RegionWeatherCandidate& a, const RegionWeatherCandidate& b) {
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.regionFormId < b.regionFormId;
        });
}

} // namespace aetherclime
