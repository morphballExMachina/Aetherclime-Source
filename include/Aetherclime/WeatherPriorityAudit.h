#pragma once

#include "Aetherclime/WeatherRegions.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aetherclime {

// A read-only, intentionally conservative *priority audit*, not an emulator
// of Skyrim's region/weather selector. Do not publish this as an API for the
// active regional weather, and never use it to force/change a TESWeather.
enum class WeatherPriorityStatus {
    NoWeatherEntries,
    TruncatedDiscovery,
    IndeterminateFootprints,
    NoInsideCandidates,
    OnlyInsideCandidate,
    UniqueOverridePriorityLeader,
    UniqueNonOverridePriorityLeader,
    TiedPriority
};

[[nodiscard]] inline const char* WeatherPriorityStatusName(WeatherPriorityStatus status) noexcept {
    switch (status) {
    case WeatherPriorityStatus::NoWeatherEntries: return "no-weather-entries";
    case WeatherPriorityStatus::TruncatedDiscovery: return "discovery-truncated";
    case WeatherPriorityStatus::IndeterminateFootprints: return "indeterminate-footprints";
    case WeatherPriorityStatus::NoInsideCandidates: return "no-inside-candidates";
    case WeatherPriorityStatus::OnlyInsideCandidate: return "only-inside-candidate";
    case WeatherPriorityStatus::UniqueOverridePriorityLeader: return "unique-override-priority-leader";
    case WeatherPriorityStatus::UniqueNonOverridePriorityLeader: return "unique-nonoverride-priority-leader";
    case WeatherPriorityStatus::TiedPriority: return "tied-priority";
    }
    return "unknown";
}

struct WeatherPriorityAudit {
    WeatherPriorityStatus status{WeatherPriorityStatus::NoWeatherEntries};
    std::size_t insideCount{0};
    std::size_t outsideCount{0};
    std::size_t indeterminateCount{0};
    std::size_t topPriorityTieCount{0};
    std::uint32_t leadingRegionFormId{0}; // 0 when ambiguous/unknown
    std::uint8_t leadingPriority{0};
    bool leadingOverrideFlag{false};
    bool observedWeatherInLeader{false}; // diagnostic only: cannot prove region selection
};

// Deliberately abstains if any candidate has uncertain footprint evidence,
// even if its priority happens to be lower than an inside candidate. Geometry
// boundary and edge-falloff semantics are not yet verified against the engine.
[[nodiscard]] inline WeatherPriorityAudit AuditWeatherRegionPriorities(
    const std::vector<RegionWeatherCandidate>& candidates,
    std::uint32_t observedWeatherId,
    bool discoveryTruncated = false) noexcept {
    WeatherPriorityAudit audit;
    if (discoveryTruncated) {
        audit.status = WeatherPriorityStatus::TruncatedDiscovery;
    } else if (candidates.empty()) {
        audit.status = WeatherPriorityStatus::NoWeatherEntries;
        return audit;
    }

    const RegionWeatherCandidate* top = nullptr;
    for (const auto& region : candidates) {
        switch (region.footprint.status) {
        case RegionFootprintStatus::InsideRecordedPolygon:
            ++audit.insideCount;
            if (!top || region.priority > top->priority) {
                top = &region;
                audit.topPriorityTieCount = 1;
            } else if (region.priority == top->priority) {
                ++audit.topPriorityTieCount;
            }
            break;
        case RegionFootprintStatus::OutsideRecordedPolygons:
        case RegionFootprintStatus::WorldspaceMismatch:
            ++audit.outsideCount;
            break;
        default:
            ++audit.indeterminateCount;
            break;
        }
    }
    if (discoveryTruncated) return audit;
    if (audit.indeterminateCount) {
        audit.status = WeatherPriorityStatus::IndeterminateFootprints;
        return audit;
    }
    if (!top) {
        audit.status = WeatherPriorityStatus::NoInsideCandidates;
        return audit;
    }
    if (audit.topPriorityTieCount > 1) {
        audit.status = WeatherPriorityStatus::TiedPriority;
        return audit;
    }

    audit.leadingRegionFormId = top->regionFormId;
    audit.leadingPriority = top->priority;
    audit.leadingOverrideFlag = top->overrideFlag;
    audit.observedWeatherInLeader = SummarizeRegionWeather(*top, observedWeatherId).containsObservedWeather;
    audit.status = audit.insideCount == 1 ? WeatherPriorityStatus::OnlyInsideCandidate :
        top->overrideFlag ? WeatherPriorityStatus::UniqueOverridePriorityLeader :
                            WeatherPriorityStatus::UniqueNonOverridePriorityLeader;
    return audit;
}

} // namespace aetherclime
