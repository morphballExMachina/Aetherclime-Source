#include "Aetherclime/GeographicGapReport.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using namespace aetherclime;

namespace {
constexpr auto Fallback = ClimateRegionSource::FallbackNoEvidence;
constexpr auto Anchor = ClimateRegionSource::GeographicAnchorProximity;
constexpr auto Named = ClimateRegionSource::LocationDisplayName;

TemperatureHandoffSample Sample(double x, double y, double c, const char* region,
                                ClimateRegionSource source, double hour,
                                std::uint32_t world = 0x3c) {
    TemperatureHandoffSample s;
    s.exterior = true;
    s.position = ClimateWorldPoint{world, x, y};
    s.date = {4, 11, hour, 201};
    s.regionKey = region;
    s.source = source;
    s.weather = Weather::Clear;
    s.outdoorAirC = c;
    return s;
}

void BidirectionalShorteningAndFallbackTrace() {
    TemperatureHandoffAudit audit;
    GeographicGapReport report;
    auto push = [&](TemperatureHandoffSample s, std::uint64_t i,
                    GeographicFallbackReason reason = GeographicFallbackReason::NotFallback) {
        GeographicFallbackFinding f;
        f.reason = reason;
        f.directSource = s.source;
        f.continuityStatus = ContinuityStatus::NoVerifiedOrigin;
        report.Observe(s, audit.Observe(s), i, f);
    };
    push(Sample(0, 0, 15, "falkreath", Anchor, 12.00), 1);
    push(Sample(1000, 0, 21, "temperate", Fallback, 12.02), 2,
         GeographicFallbackReason::NoVerifiedContinuityOrigin);
    push(Sample(0, 0, 15, "falkreath", Anchor, 12.04), 3);
    push(Sample(300, 0, 15.1, "falkreath", Anchor, 12.05), 4);
    push(Sample(700, 0, 21.2, "temperate", Fallback, 12.06), 5,
         GeographicFallbackReason::ContinuityOriginTooFar);

    const auto& stat = report.Summary();
    assert(stat.qualifyingEvents == 3);
    assert(stat.corridorCount == 1);
    assert(stat.corridorMergedOccurrences == 2);
    assert(stat.corridorRepresentativesReplaced == 1);
    const auto c = report.RankedCorridors()[0];
    assert(c.corridorId == 1);
    assert(c.occurrences == 3);
    assert(c.sameDirectionOccurrences == 2);
    assert(c.reverseDirectionOccurrences == 1);
    assert(c.observedBothDirections);
    assert(c.firstSampleIndex == 2 && c.lastSampleIndex == 5);
    assert(std::abs(c.shortestObservedSpanRawUnits - 400.0) < 1e-9);
    assert(c.representative.sampleIndex == 5);
    assert(c.representative.previousFallback.reason == GeographicFallbackReason::NotFallback);
    assert(c.representative.currentFallback.reason ==
           GeographicFallbackReason::ContinuityOriginTooFar);
    assert(c.representative.previousXRaw == 300 && c.representative.currentXRaw == 700);
    // No exact boundary or atlas edit is inferred from this bracket.
}

void DifferentCorridorsAndSessionReset() {
    TemperatureHandoffAudit audit;
    GeographicGapReport report;
    std::uint64_t i = 0;
    auto push = [&](const TemperatureHandoffSample& s) {
        report.Observe(s, audit.Observe(s), ++i);
    };
    push(Sample(0, 0, 15, "falkreath", Anchor, 12.00));
    push(Sample(1000, 0, 21, "temperate", Fallback, 12.01));
    // Same profile/source labels but much farther down the route: separate.
    push(Sample(10000, 0, 21, "temperate", Fallback, 12.02));
    push(Sample(11000, 0, 15, "falkreath", Anchor, 12.03));
    push(Sample(12000, 0, 21, "temperate", Fallback, 12.04));
    assert(report.Summary().corridorCount == 3);
    assert(report.Summary().corridorMergedOccurrences == 0);
    // Incompatible worldspaces and weather cannot produce corridors.
    push(Sample(12010, 0, 15, "falkreath", Anchor, 12.05, 0x99));
    auto storm = Sample(12020, 0, 21, "temperate", Fallback, 12.06, 0x99);
    storm.weather = Weather::Storm;
    push(storm);
    assert(report.Summary().corridorCount == 3);
    report.Reset();
    assert(report.Summary().corridorCount == 0);
    assert(report.Summary().qualifyingEvents == 0);
    assert(report.RankedCorridors()[0].corridorId == 0);
}

void ReverseRepresentativeDoesNotFlipDirectionCounts() {
    TemperatureHandoffAudit audit;
    GeographicGapReport report;
    std::uint64_t i = 0;
    auto push = [&](const TemperatureHandoffSample& s) {
        report.Observe(s, audit.Observe(s), ++i);
    };
    push(Sample(0, 0, 15, "falkreath", Anchor, 10.00));
    push(Sample(1200, 0, 21, "temperate", Fallback, 10.01));
    // Return walk offers the shorter representative (700 raw units).
    push(Sample(500, 0, 15, "falkreath", Anchor, 10.02));
    // Outbound walk offers a still-shorter bracket (600 units).
    push(Sample(1100, 0, 21, "temperate", Fallback, 10.03));
    auto c = report.RankedCorridors()[0];
    assert(report.Summary().corridorCount == 1);
    assert(c.occurrences == 3 && c.sameDirectionOccurrences == 2);
    assert(c.reverseDirectionOccurrences == 1 && c.observedBothDirections);
    assert(c.representative.sampleIndex == 4);
    assert(c.routeReference.sampleIndex == 2); // immutable comparison orientation
    assert(std::abs(c.shortestObservedSpanRawUnits - 600.0) < 1e-9);
}

void CapacityAndPriority() {
    TemperatureHandoffAudit audit;
    GeographicGapReport report;
    std::uint64_t i = 0;
    auto push = [&](const TemperatureHandoffSample& s) {
        report.Observe(s, audit.Observe(s), ++i);
    };
    // Eight distinct fallback-entry corridors, with a different source label
    // on each start. Alternate via named locations to avoid same-route merging.
    for (int k = 0; k < 9; ++k) {
        const double x = k * 10000.0;
        push(Sample(x, 0, 10, "falkreath", Named, 10.0 + k * 0.02));
        push(Sample(x + 500, 0, k == 8 ? 19 : 11, "temperate", Fallback,
                    10.01 + k * 0.02));
    }
    assert(report.Summary().corridorCount == GeographicGapReport::kMaxCorridors);
    // Last high-priority profile step displaces an earlier low-priority one.
    assert(report.Summary().corridorListReplacements == 1);
    assert(report.RankedCorridors()[0].priority == GeographicGapPriority::High);
    assert(report.RankedCorridors()[0].representative.sampleIndex == 18);
}
}

int main() {
    BidirectionalShorteningAndFallbackTrace();
    DifferentCorridorsAndSessionReset();
    ReverseRepresentativeDoesNotFlipDirectionCounts();
    CapacityAndPriority();
    std::cout << "Alpha-35 geographic corridor review tests passed\n";
}
