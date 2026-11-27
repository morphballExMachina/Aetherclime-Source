#include "Aetherclime/TemperatureHandoffAudit.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace aetherclime;
namespace {
TemperatureHandoffSample SampleAt(double x, double y, std::string region = "winterhold",
                                   ClimateRegionSource source = ClimateRegionSource::LocationDisplayName,
                                   double air = -12.0) {
    TemperatureHandoffSample s;
    s.exterior = true;
    s.position = ClimateWorldPoint{0x3C, x, y};
    s.date = {0, 3, 10.0, 201};
    s.regionKey = std::move(region);
    s.source = source;
    s.weather = Weather::Snow;
    s.outdoorAirC = air;
    return s;
}
void BaselineAndHandoffs() {
    TemperatureHandoffAudit audit;
    auto direct = SampleAt(1000, 1000);
    assert(audit.Observe(direct).status == TemperatureHandoffStatus::Baseline);
    auto same = SampleAt(1100, 1000);
    assert(audit.Observe(same).status == TemperatureHandoffStatus::Unchanged);
    auto survey = SampleAt(1300, 1000, "winterhold", ClimateRegionSource::ProvisionalSurveySample, -12.5);
    const auto s = audit.Observe(survey);
    assert(s.comparable && s.sourceChanged && !s.profileChanged);
    assert(s.status == TemperatureHandoffStatus::SourceChangedSameProfile);
    assert(std::abs(s.airDeltaC + 0.5) < 1e-6);
    auto differentSourceSameProfile = SampleAt(1350, 1000, "winterhold", ClimateRegionSource::ProvisionalAtlasZone, -12.5);
    const auto sameProfile = audit.Observe(differentSourceSameProfile);
    assert(sameProfile.comparable && sameProfile.sourceChanged && !sameProfile.profileChanged);
    assert(sameProfile.status == TemperatureHandoffStatus::SourceChangedSameProfile);
    auto fallback = SampleAt(1450, 1000, "temperate", ClimateRegionSource::FallbackNoEvidence, 18.0);
    const auto jump = audit.Observe(fallback);
    assert(jump.status == TemperatureHandoffStatus::SignificantProfileStep);
    assert(jump.previousSource == ClimateRegionSource::ProvisionalAtlasZone);
    assert(jump.previousRegion == "winterhold");
    assert(jump.comparable && jump.profileChanged && jump.sourceChanged && jump.significant);
    assert(std::abs(jump.airDeltaC - 30.5) < 1e-6);
    // No fake correction: the caller-owned model output is untouched.
    assert(fallback.outdoorAirC == 18.0);
}
void OtherProfileChanges() {
    TemperatureHandoffAudit a;
    (void)a.Observe(SampleAt(0,0));
    auto other = SampleAt(200,100,"whiterun", ClimateRegionSource::LocationDisplayName, -10);
    const auto r = a.Observe(other);
    assert(r.status == TemperatureHandoffStatus::ProfileChanged);
    assert(!r.sourceChanged && r.profileChanged && !r.significant);
}
void NoSpeculativeComparison() {
    const auto base = SampleAt(1000,1000);
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(base);
        auto different = SampleAt(1100,1000); different.position->worldspaceFormId = 0x55;
        assert(a.Observe(different).status == TemperatureHandoffStatus::DifferentWorldspace);
        assert(a.Observe(SampleAt(1100,1000)).status == TemperatureHandoffStatus::DifferentWorldspace);
    }
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(base);
        auto day = SampleAt(1100,1000); ++day.date.day;
        assert(a.Observe(day).status == TemperatureHandoffStatus::DifferentCalendarDay);
    }
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(base);
        auto backwards = SampleAt(1100,1000); backwards.date.hour = 9.9;
        assert(a.Observe(backwards).status == TemperatureHandoffStatus::InvalidTime);
    }
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(base);
        auto late = SampleAt(1100,1000); late.date.hour = 10.6;
        assert(a.Observe(late).status == TemperatureHandoffStatus::TooOld);
    }
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(base);
        assert(a.Observe(SampleAt(9500,1000)).status == TemperatureHandoffStatus::TooFar);
    }
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(base);
        auto wet = SampleAt(1100,1000,"temperate",ClimateRegionSource::FallbackNoEvidence,18);
        wet.weather = Weather::Rain;
        assert(a.Observe(wet).status == TemperatureHandoffStatus::WeatherChanged);
    }
}
void ThresholdsAndExactLimits() {
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(SampleAt(0, 0, "winterhold", ClimateRegionSource::LocationDisplayName, -10.0));
        const auto justUnder = a.Observe(SampleAt(50, 0, "rift", ClimateRegionSource::LocationDisplayName, -6.01));
        assert(justUnder.comparable && justUnder.profileChanged);
        assert(!justUnder.significant && justUnder.status == TemperatureHandoffStatus::ProfileChanged);
        const auto exactly = a.Observe(SampleAt(100, 0, "winterhold", ClimateRegionSource::LocationDisplayName, -10.01));
        assert(exactly.comparable && exactly.significant);
        assert(exactly.status == TemperatureHandoffStatus::SignificantProfileStep);
    }
    {
        TemperatureHandoffAudit a;
        (void)a.Observe(SampleAt(0, 0));
        auto edge = SampleAt(8192, 0, "temperate", ClimateRegionSource::FallbackNoEvidence, 18.0);
        edge.date.hour = 10.5;
        const auto r = a.Observe(edge);
        assert(r.comparable && r.status == TemperatureHandoffStatus::SignificantProfileStep);
        assert(r.distanceRawUnits == TemperatureHandoffAudit::kMaxSampleDistanceRawUnits);
        assert(r.ageGameHours == TemperatureHandoffAudit::kMaxSampleAgeGameHours);
    }
}
void ResetAndInvalid() {
    TemperatureHandoffAudit a;
    auto initial = SampleAt(0,0);
    (void)a.Observe(initial);
    auto inside = SampleAt(10,0); inside.exterior = false;
    assert(a.Observe(inside).status == TemperatureHandoffStatus::NoExteriorEvidence);
    assert(a.Observe(SampleAt(100,0)).status == TemperatureHandoffStatus::Baseline);
    auto invalid = SampleAt(101,0); invalid.outdoorAirC = std::numeric_limits<double>::quiet_NaN();
    assert(a.Observe(invalid).status == TemperatureHandoffStatus::NoExteriorEvidence);
    assert(a.Observe(SampleAt(105,0)).status == TemperatureHandoffStatus::Baseline);
    // Unknown weather is not reliable evidence of unchanged weather.
    auto noWeather = SampleAt(109,0); noWeather.weather = Weather::Unknown;
    assert(a.Observe(noWeather).status == TemperatureHandoffStatus::NoExteriorEvidence);
    assert(a.Observe(SampleAt(110,0)).status == TemperatureHandoffStatus::Baseline);
    a.Reset();
    assert(a.Observe(SampleAt(110,0)).status == TemperatureHandoffStatus::Baseline);
}
void SlidingObservationsNotFixedOrigin() {
    TemperatureHandoffAudit a;
    (void)a.Observe(SampleAt(0,0));
    (void)a.Observe(SampleAt(7000,0));
    // A nearby move relative to the immediately previous point is valid.
    const auto r = a.Observe(SampleAt(8500,0,"temperate",ClimateRegionSource::FallbackNoEvidence,18));
    assert(r.status == TemperatureHandoffStatus::SignificantProfileStep);
    assert(r.distanceRawUnits == 1500);
}
} // namespace
int main() {
    BaselineAndHandoffs();
    OtherProfileChanges();
    NoSpeculativeComparison();
    ResetAndInvalid();
    ThresholdsAndExactLimits();
    SlidingObservationsNotFixedOrigin();
    std::cout << "Alpha-30 temperature handoff audit tests passed\n";
}
