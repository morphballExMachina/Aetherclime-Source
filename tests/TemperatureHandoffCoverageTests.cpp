#include "Aetherclime/TemperatureHandoffCoverage.h"

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

TemperatureHandoffSample Sample(double x, double air, const std::string& region,
                                ClimateRegionSource source, double hour = 10.0) {
    TemperatureHandoffSample result;
    result.exterior = true;
    result.position = ClimateWorldPoint{0x3c, x, 0.0};
    result.date = {0, 3, hour, 201};
    result.regionKey = region;
    result.source = source;
    result.weather = Weather::Clear;
    result.outdoorAirC = air;
    return result;
}

void FalkreathReproductionAndTraceability() {
    TemperatureHandoffAudit audit;
    TemperatureHandoffCoverage coverage;
    const auto baseline = Sample(0, 20.63, "temperate", Fallback);
    const auto a = audit.Observe(baseline);
    assert(!a.previousReadingAvailable);
    coverage.Record(baseline, a);
    // Not every eligible same-profile sample was printed by Alpha-32. The
    // immediately prior reading, not last printed baseline, determines delta.
    const auto unprinted = Sample(100, 21.15, "temperate", Fallback, 10.01);
    const auto b = audit.Observe(unprinted);
    coverage.Record(unprinted, b);
    const auto falkreath = Sample(2986.9, 15.41, "falkreath", Anchor, 10.043);
    const auto c = audit.Observe(falkreath);
    coverage.Record(falkreath, c);
    assert(c.status == TemperatureHandoffStatus::SignificantProfileStep);
    assert(c.previousReadingAvailable && std::abs(c.previousOutdoorAirC - 21.15) < 1e-9);
    assert(std::abs(c.airDeltaC + 5.74) < 1e-9);
    assert(c.distanceRawUnits < TemperatureHandoffAudit::kMaxSampleDistanceRawUnits);
    assert(ClassifyTemperatureHandoffStep(c, falkreath.source) ==
           TemperatureHandoffStepClass::SignificantTowardStronger);
    const auto named = Sample(3300, 15.65, "falkreath", Named, 10.07);
    const auto d = audit.Observe(named);
    coverage.Record(named, d);
    const auto back = Sample(4000, 15.70, "falkreath", Anchor, 10.09);
    const auto e = audit.Observe(back);
    coverage.Record(back, e);
    assert(ClassifyTemperatureHandoffStep(e, back.source) == TemperatureHandoffStepClass::SameProfile);
    const auto& stats = coverage.Snapshot();
    assert(stats.observations == 5 && stats.baselines == 1 && stats.comparablePairs == 4);
    assert(stats.sourceSwitchSameProfile == 2);
    assert(stats.fallbackSamples == 2 && stats.anchorSamples == 2 && stats.namedSamples == 1);
    assert(stats.provisionalSamples == 0);
    assert(stats.significantProfileSteps == 1 && stats.significantTowardStronger == 1);
    assert(stats.significantTowardWeaker == 0 && stats.significantOther == 0);
    assert(stats.fallbackToAnchor == 1 && stats.anchorToFallback == 0);
    assert(stats.uniqueTierRoutes == 3); // fallback->anchor, anchor->named, named->anchor
    assert(stats.hasPeakProfileStep && std::abs(stats.peakSignedProfileStepC + 5.74) < 1e-9);
    assert(stats.peakPreviousRegion == "temperate" && stats.peakCurrentRegion == "falkreath");
    assert(stats.peakPreviousSource == Fallback && stats.peakCurrentSource == Anchor);
    assert(stats.peakDistanceRawUnits > 2800.0 && stats.peakAgeGameHours > 0);
}

void CounterexamplesAndRejects() {
    TemperatureHandoffAudit audit;
    TemperatureHandoffCoverage coverage;
    auto push = [&](const TemperatureHandoffSample& sample) {
        const auto assessment = audit.Observe(sample);
        coverage.Record(sample, assessment);
        return assessment;
    };
    (void)push(Sample(0, 10.0, "falkreath", Named));
    const auto weaker = push(Sample(100, 17.0, "temperate", Fallback, 10.01));
    assert(ClassifyTemperatureHandoffStep(weaker, Fallback) ==
           TemperatureHandoffStepClass::SignificantTowardWeaker);
    const auto small = push(Sample(200, 17.2, "falkreath", Anchor, 10.02));
    assert(ClassifyTemperatureHandoffStep(small, Anchor) ==
           TemperatureHandoffStepClass::ProfileStepBelowThreshold);
    const auto sameSource = push(Sample(250, 22.0, "temperate", Anchor, 10.03));
    assert(ClassifyTemperatureHandoffStep(sameSource, Anchor) == TemperatureHandoffStepClass::SignificantOther);
    auto wet = Sample(300, 10.0, "temperate", Anchor, 10.04);
    wet.weather = Weather::Rain;
    const auto wrongWeather = push(wet);
    assert(!wrongWeather.comparable);
    assert(ClassifyTemperatureHandoffStep(wrongWeather, Anchor) == TemperatureHandoffStepClass::NotComparable);
    const auto normal = push(Sample(301, 10.0, "temperate", Anchor, 10.05));
    assert(!normal.comparable); // weather changed back; previous still updated
    auto interior = Sample(302, 10.0, "temperate", Anchor, 10.06);
    interior.exterior = false;
    const auto noExterior = push(interior);
    assert(noExterior.status == TemperatureHandoffStatus::NoExteriorEvidence);
    const auto restart = push(Sample(303, 10.0, "temperate", Anchor, 10.07));
    assert(restart.status == TemperatureHandoffStatus::Baseline);
    const auto& stats = coverage.Snapshot();
    assert(stats.observations == 8 && stats.baselines == 2 && stats.invalidExteriorSamples == 1);
    assert(stats.rejectedComparisons == 2 && stats.comparablePairs == 3);
    assert(stats.rejectedWeather == 2 && stats.rejectedWorldspace == 0);
    assert(stats.namedSamples == 1 && stats.fallbackSamples == 1 && stats.anchorSamples == 5);
    assert(stats.significantProfileSteps == 2 && stats.profileStepsBelowThreshold == 1);
    assert(stats.significantTowardWeaker == 1 && stats.significantOther == 1);
    assert(stats.hasPeakProfileStep && stats.peakSignedProfileStepC == 7.0);
    coverage.Reset();
    assert(coverage.Snapshot().observations == 0 && !coverage.Snapshot().hasPeakProfileStep);
}

void SameTierAndDistinctRoutes() {
    TemperatureHandoffAudit audit;
    TemperatureHandoffCoverage coverage;
    auto initial = Sample(0, 0, "winterhold", Provisional);
    coverage.Record(initial, audit.Observe(initial));
    auto current = Sample(100, 0.5, "winterhold", ClimateRegionSource::ProvisionalAtlasZone, 10.01);
    coverage.Record(current, audit.Observe(current));
    current = Sample(200, 1.0, "winterhold", Provisional, 10.02);
    coverage.Record(current, audit.Observe(current));
    current = Sample(300, 1.2, "winterhold", ClimateRegionSource::ProvisionalAtlasZone, 10.03);
    coverage.Record(current, audit.Observe(current));
    assert(coverage.Snapshot().sourceSwitchSameProfile == 3);
    assert(coverage.Snapshot().uniqueTierRoutes == 1); // provisional -> provisional, not three unique routes
    assert(!coverage.Snapshot().hasPeakProfileStep);
}

void BudgetProtectsImportantReports() {
    TemperatureHandoffReportBudget budget;
    assert(!budget.Allow(false, false) && budget.TotalEmitted() == 0);
    for (unsigned i = 0; i < 35; ++i)
        assert(budget.Allow(true, false) == (i < TemperatureHandoffReportBudget::kRoutineLimit));
    assert(budget.TotalEmitted() == 18 && budget.RoutineEmitted() == 18);
    assert(!budget.Allow(true, false));
    for (unsigned i = 0; i < 30; ++i) assert(budget.Allow(true, true));
    assert(budget.TotalEmitted() == 48 && !budget.Allow(true, true));
    budget.Reset();
    assert(budget.TotalEmitted() == 0 && budget.RoutineEmitted() == 0);
    // Priority entries share the original total bound; a routine line cannot
    // be emitted once that total is exhausted even if routine budget remains.
    for (unsigned i = 0; i < 48; ++i) assert(budget.Allow(true, true));
    assert(!budget.Allow(true, false));
}

void SourceNamesAndStepClasses() {
    assert(TemperatureHandoffStepClassName(TemperatureHandoffStepClass::SameProfile) == "same-profile");
    assert(TemperatureHandoffStepClassName(TemperatureHandoffStepClass::SignificantTowardStronger) ==
           "significant-toward-stronger");
    TemperatureHandoffAssessment assessment;
    assessment.comparable = true;
    assessment.profileChanged = true;
    assessment.significant = true;
    assessment.previousSource = Fallback;
    assert(ClassifyTemperatureHandoffStep(assessment, Anchor) ==
           TemperatureHandoffStepClass::SignificantTowardStronger);
    assessment.previousSource = Named;
    assert(ClassifyTemperatureHandoffStep(assessment, Provisional) ==
           TemperatureHandoffStepClass::SignificantTowardWeaker);
    assessment.previousSource = Anchor;
    assert(ClassifyTemperatureHandoffStep(assessment, Anchor) == TemperatureHandoffStepClass::SignificantOther);
}
} // namespace

int main() {
    FalkreathReproductionAndTraceability();
    CounterexamplesAndRejects();
    SameTierAndDistinctRoutes();
    BudgetProtectsImportantReports();
    SourceNamesAndStepClasses();
    std::cout << "Alpha-33 temperature handoff coverage tests passed\n";
}
