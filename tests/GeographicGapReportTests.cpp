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
constexpr auto Provisional = ClimateRegionSource::ProvisionalSurveySample;

TemperatureHandoffSample Sample(double x, double y, double temp, const std::string& region,
                                ClimateRegionSource source, double hour = 10.0,
                                std::uint32_t worldspace = 0x3c) {
    TemperatureHandoffSample s;
    s.exterior = true;
    s.position = ClimateWorldPoint{worldspace, x, y};
    s.date = {0, 3, hour, 201};
    s.regionKey = region;
    s.source = source;
    s.weather = Weather::Clear;
    s.outdoorAirC = temp;
    return s;
}

void GeographicFalkreathRoute() {
    TemperatureHandoffAudit audit;
    GeographicGapReport report;
    std::uint64_t sampleIndex = 0;
    auto observe = [&](const TemperatureHandoffSample& s) {
        const auto a = audit.Observe(s);
        report.Observe(s, a, ++sampleIndex);
        return a;
    };
    const auto start = observe(Sample(0, 0, 21.15, "temperate", Fallback));
    assert(!start.previousPosition);
    const auto falkreath = observe(Sample(2887, 0, 15.41, "falkreath", Anchor, 10.03));
    assert(falkreath.previousPosition && falkreath.previousPosition->xRaw == 0);
    assert(report.Summary().fallbackExits == 1);
    const auto nominal = observe(Sample(3200, 0, 15.50, "falkreath", Provisional, 10.04));
    assert(nominal.comparable && !nominal.profileChanged);
    observe(Sample(4271, 0, 21.37, "temperate", Fallback, 10.07));
    const auto& stats = report.Summary();
    assert(stats.comparablePairs == 3 && stats.qualifyingEvents == 2);
    assert(stats.fallbackEntries == 1 && stats.fallbackExits == 1);
    assert(stats.candidateCount == 2);
    const auto candidates = report.RankedCandidates();
    assert(candidates[0].priority == GeographicGapPriority::High);
    assert(candidates[1].priority == GeographicGapPriority::High);
    assert(candidates[0].significant && candidates[1].significant);
    assert(candidates[0].worldspaceFormId == 0x3c);
    assert((candidates[0].kind == GeographicGapKind::FallbackEntry ||
            candidates[1].kind == GeographicGapKind::FallbackEntry));
    assert(GeographicGapKindName(GeographicGapKind::FallbackEntry) == "fallback-entry");
    assert(GeographicGapPriorityName(GeographicGapPriority::High) == "high");
}

void NearbyDedupAndPriorityReplacement() {
    TemperatureHandoffAudit audit;
    GeographicGapReport report;
    std::uint64_t index = 0;
    auto push = [&](const TemperatureHandoffSample& s) {
        auto a = audit.Observe(s);
        report.Observe(s, a, ++index);
    };
    push(Sample(0, 0, 15, "falkreath", Anchor));
    push(Sample(100, 0, 21, "temperate", Fallback, 10.01));
    push(Sample(130, 0, 15, "falkreath", Anchor, 10.02));
    push(Sample(150, 0, 22, "temperate", Fallback, 10.03));
    assert(report.Summary().fallbackEntries == 2);
    assert(report.Summary().fallbackExits == 1);
    assert(report.Summary().qualifyingEvents == 3);
    assert(report.Summary().candidateCount == 2);
    assert(report.Summary().mergedNearbyOccurrences == 1);
    auto top = report.RankedCandidates();
    assert(top[0].kind == GeographicGapKind::FallbackEntry);
    assert(top[0].occurrences == 2);
    assert(top[0].sampleIndex == 4);
    assert(std::abs(top[0].airDeltaC - 7.0) < 1e-9);

    // Six bounded slots and a higher priority candidate in a seventh region.
    report.Reset(); audit.Reset(); index = 0;
    push(Sample(0, 0, 0, "baseline", Named));
    for (unsigned i = 1; i <= 7; ++i) {
        // Different route labels prevent accidental spatial merging.
        push(Sample(i * 100.0, 0, 1.0 * i, "region" + std::to_string(i), Provisional,
                    10.0 + i * .01));
    }
    assert(report.Summary().candidateCount == GeographicGapReport::kMaxCandidates);
    assert(report.Summary().qualifyingEvents == 7);
    assert(report.Summary().droppedFromTopList == 1);
    // A later high-value fallback profile jump should displace a low-value
    // provisional example instead of being lost after the list fills.
    push(Sample(800, 0, 7, "region7", Named, 10.08)); // same profile, source-only
    push(Sample(900, 0, 14, "temperate", Fallback, 10.09));
    assert(report.Summary().replacedRepresentatives == 1);
    assert(report.Summary().fallbackEntries == 1);
    top = report.RankedCandidates();
    assert(top[0].kind == GeographicGapKind::FallbackEntry);
    assert(top[0].priority == GeographicGapPriority::High);
    assert(top[0].sampleIndex == 10);
    report.Reset();
    assert(report.Summary().candidateCount == 0 && report.Summary().qualifyingEvents == 0);
}

void RejectUnsafeComparisons() {
    GeographicGapReport report;
    TemperatureHandoffAudit audit;
    auto base = Sample(0, 0, 10, "falkreath", Named);
    auto a = audit.Observe(base);
    report.Observe(base, a, 1);
    assert(report.Summary().comparablePairs == 0);
    auto tooFar = Sample(9000, 0, 20, "temperate", Fallback, 10.03);
    auto b = audit.Observe(tooFar);
    assert(!b.comparable);
    report.Observe(tooFar, b, 2);
    assert(report.Summary().candidateCount == 0);
    auto otherWorld = Sample(9001, 0, 20, "temperate", Fallback, 10.04, 0x99);
    auto c = audit.Observe(otherWorld);
    assert(!c.comparable);
    report.Observe(otherWorld, c, 3);
    auto wet = Sample(9002, 0, 10, "falkreath", Anchor, 10.05, 0x99);
    wet.weather = Weather::Rain;
    auto d = audit.Observe(wet);
    assert(!d.comparable);
    report.Observe(wet, d, 4);
    auto interior = Sample(9003, 0, 11, "temperate", Fallback, 10.06, 0x99);
    interior.exterior = false;
    report.Observe(interior, audit.Observe(interior), 5);
    assert(report.Summary().candidateCount == 0);
    assert(report.Summary().comparablePairs == 0);
}

void OtherStepClasses() {
    GeographicGapReport report;
    TemperatureHandoffAudit audit;
    auto first = Sample(0, 0, 10, "regionA", Named);
    report.Observe(first, audit.Observe(first), 1);
    auto other = Sample(100, 0, 16, "regionB", Named, 10.01);
    report.Observe(other, audit.Observe(other), 2);
    auto provisional = Sample(200, 0, 16.5, "regionC", Provisional, 10.02);
    report.Observe(provisional, audit.Observe(provisional), 3);
    assert(report.Summary().otherSignificantProfileSteps == 1);
    assert(report.Summary().provisionalProfileSteps == 1);
    assert(report.Summary().candidateCount == 2);
    const auto top = report.RankedCandidates();
    assert(top[0].priority == GeographicGapPriority::Review);
    assert(top[1].priority == GeographicGapPriority::Context);
}
} // namespace

int main() {
    GeographicFalkreathRoute();
    NearbyDedupAndPriorityReplacement();
    RejectUnsafeComparisons();
    OtherStepClasses();
    std::cout << "Alpha-34 geographic gap report tests passed\n";
}
