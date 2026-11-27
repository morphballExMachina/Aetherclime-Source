#include "Aetherclime/TemperatureContinuityPreview.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace aetherclime;
namespace {
TemperatureHandoffSample At(double x, double hour, std::string profile,
                             ClimateRegionSource source, double raw) {
    TemperatureHandoffSample sample;
    sample.exterior = true;
    sample.position = ClimateWorldPoint{0x3C, x, 2000};
    sample.date = {0, 3, hour, 201};
    sample.regionKey = std::move(profile);
    sample.source = source;
    sample.weather = Weather::Snow;
    sample.outdoorAirC = raw;
    return sample;
}
struct Harness {
    TemperatureHandoffAudit audit;
    TemperatureContinuityPreview preview;
    TemperatureContinuitySettings settings;
    TemperatureContinuityResult Observe(const TemperatureHandoffSample& s) {
        const auto assessment = audit.Observe(s);
        return preview.Observe(s, assessment, settings);
    }
    void Reload() {
        audit.Reset();
        preview.Reset();
    }
};
void DisabledIsNoOp() {
    Harness h;
    auto a = At(0, 10, "winterhold", ClimateRegionSource::LocationDisplayName, -12);
    auto b = At(100, 10.01, "temperate", ClimateRegionSource::FallbackNoEvidence, 18);
    auto x = h.Observe(a);
    auto y = h.Observe(b);
    assert(x.status == TemperatureContinuityStatus::Disabled);
    assert(y.status == TemperatureContinuityStatus::Disabled);
    assert(y.rawOutdoorAirC == 18 && y.previewOutdoorAirC == 18);
    assert(!y.candidateActive && !y.publishedTemperatureModified);
}
void EligibleHandoffAndBoundedDecay() {
    Harness h; h.settings.previewEnabled = true;
    auto north = At(0, 10, "winterhold", ClimateRegionSource::LocationDisplayName, -12.5);
    auto fallback = At(100, 10.02, "temperate", ClimateRegionSource::FallbackNoEvidence, 18);
    assert(h.Observe(north).status == TemperatureContinuityStatus::Baseline);
    const auto jump = h.Observe(fallback);
    assert(jump.status == TemperatureContinuityStatus::Started);
    assert(jump.candidateActive && jump.previewOffsetC == -6);
    assert(jump.rawOutdoorAirC == 18 && jump.previewOutdoorAirC == 12);
    assert(!jump.publishedTemperatureModified);
    assert(fallback.outdoorAirC == 18); // source/model output is never rewritten
    auto still = At(100, 10.02, "temperate", ClimateRegionSource::FallbackNoEvidence, 18);
    const auto repeat = h.Observe(still);
    assert(repeat.status == TemperatureContinuityStatus::Fading);
    assert(repeat.previewOffsetC == -6); // zero travel/time does not add fade
    auto travel = At(1124, 10.12, "temperate", ClimateRegionSource::FallbackNoEvidence, 19);
    const auto fade = h.Observe(travel);
    assert(fade.status == TemperatureContinuityStatus::Fading);
    assert(fade.pathDistanceRawUnits == 1024);
    assert(std::abs(fade.elapsedGameHours - 0.1) < 1e-8);
    const double expectedOffset = -6 * (1 - 1024.0 / 4096.0) * (1 - 0.1 / 0.5);
    assert(std::abs(fade.previewOffsetC - expectedOffset) < 1e-8);
    assert(std::abs(fade.previewOutdoorAirC - (19 + expectedOffset)) < 1e-8);
    auto expired = At(4200, 10.20, "temperate", ClimateRegionSource::FallbackNoEvidence, 20);
    const auto finish = h.Observe(expired);
    assert(finish.status == TemperatureContinuityStatus::Expired);
    assert(!finish.candidateActive && finish.previewOutdoorAirC == 20);
    const auto settled = h.Observe(At(4300, 10.21, "temperate", ClimateRegionSource::FallbackNoEvidence, 20));
    assert(settled.status == TemperatureContinuityStatus::Unchanged);
}
void ExpireOnTimeAndConfigurableCaps() {
    Harness h; h.settings.previewEnabled = true;
    h.settings.fadeDistanceRawUnits = 2048;
    h.settings.maxCompensationC = 2;
    h.settings.maxLifetimeGameHours = 0.2;
    (void)h.Observe(At(0, 10.0, "winterhold", ClimateRegionSource::LocationDisplayName, -10));
    const auto jump = h.Observe(At(100, 10.01, "temperate", ClimateRegionSource::FallbackNoEvidence, 15));
    assert(jump.previewOffsetC == -2);
    const auto expire = h.Observe(At(200, 10.21, "temperate", ClimateRegionSource::FallbackNoEvidence, 15));
    assert(expire.status == TemperatureContinuityStatus::Expired);
    assert(expire.previewOutdoorAirC == 15);
}
void NeverSmoothEntrancesToTrustedOrNonSignificantBoundaries() {
    {
        Harness h; h.settings.previewEnabled = true;
        (void)h.Observe(At(0, 10, "temperate", ClimateRegionSource::FallbackNoEvidence, 18));
        auto trusted = h.Observe(At(100, 10.02, "winterhold", ClimateRegionSource::LocationDisplayName, -12));
        assert(trusted.status == TemperatureContinuityStatus::IneligibleHandoff);
        assert(trusted.previewOutdoorAirC == -12 && !trusted.candidateActive);
    }
    {
        Harness h; h.settings.previewEnabled = true;
        (void)h.Observe(At(0, 10, "winterhold", ClimateRegionSource::LocationDisplayName, -12));
        auto named = h.Observe(At(100, 10.02, "whiterun", ClimateRegionSource::LocationEditorId, 18));
        assert(named.status == TemperatureContinuityStatus::IneligibleHandoff);
        assert(named.previewOutdoorAirC == 18);
    }
    {
        Harness h; h.settings.previewEnabled = true;
        (void)h.Observe(At(0, 10, "winterhold", ClimateRegionSource::LocationDisplayName, -12));
        auto mild = h.Observe(At(100, 10.02, "temperate", ClimateRegionSource::FallbackNoEvidence, -9));
        assert(mild.status == TemperatureContinuityStatus::IneligibleHandoff);
        assert(mild.previewOutdoorAirC == -9);
    }
    {
        Harness h; h.settings.previewEnabled = true;
        (void)h.Observe(At(0, 10, "winterhold", ClimateRegionSource::LocationDisplayName, -12));
        auto sameProfile = h.Observe(At(100, 10.02, "winterhold", ClimateRegionSource::ProvisionalAtlasZone, -12));
        assert(sameProfile.status == TemperatureContinuityStatus::Unchanged);
        assert(sameProfile.previewOutdoorAirC == -12);
    }
}
void InvalidContextAndTeleportsReset() {
    // The handoff audit provides the reset reason. Never roll a correction
    // through changing weather, missing exterior cell or a large jump.
    for (int caseId = 0; caseId < 6; ++caseId) {
        Harness h; h.settings.previewEnabled = true;
        (void)h.Observe(At(0, 10, "winterhold", ClimateRegionSource::LocationDisplayName, -12));
        (void)h.Observe(At(100, 10.02, "temperate", ClimateRegionSource::FallbackNoEvidence, 18));
        auto next = At(200, 10.03, "temperate", ClimateRegionSource::FallbackNoEvidence, 18);
        switch (caseId) {
        case 0: next.position->worldspaceFormId = 0x99; break;
        case 1: next.position->xRaw = 20000; break;
        case 2: next.weather = Weather::Rain; break;
        case 3: next.exterior = false; break;
        case 4: next.date.hour = 9.0; break;
        case 5: next.date.day++; break;
        }
        const auto reset = h.Observe(next);
        assert(!reset.candidateActive && reset.previewOutdoorAirC == next.outdoorAirC);
        assert(reset.status == (caseId == 3 ? TemperatureContinuityStatus::NoEvidence :
                                TemperatureContinuityStatus::NonComparable));
        const auto nextBaseline = h.Observe(At(210, 10.04, "temperate", ClimateRegionSource::FallbackNoEvidence, 18));
        assert(!nextBaseline.candidateActive && nextBaseline.previewOutdoorAirC == 18);
    }
}
void SaveReloadResetAndWeakToWeak() {
    Harness h; h.settings.previewEnabled = true;
    (void)h.Observe(At(0, 10, "winterhold", ClimateRegionSource::ProvisionalSurveySample, -12));
    auto jump = h.Observe(At(100, 10.01, "temperate", ClimateRegionSource::FallbackNoEvidence, 18));
    assert(jump.status == TemperatureContinuityStatus::Started);
    // Repeated save at same position/time retains the candidate, not a second handoff.
    const auto same = h.Observe(At(100, 10.01, "temperate", ClimateRegionSource::FallbackNoEvidence, 18));
    assert(same.status == TemperatureContinuityStatus::Fading);
    assert(same.previewOffsetC == jump.previewOffsetC);
    h.Reload();
    const auto load = h.Observe(At(100, 10.01, "temperate", ClimateRegionSource::FallbackNoEvidence, 18));
    assert(load.status == TemperatureContinuityStatus::Baseline);
    assert(!load.candidateActive && load.previewOutdoorAirC == 18);
}
void PreventRepeatedCorrectionChains() {
    Harness h; h.settings.previewEnabled = true;
    (void)h.Observe(At(0, 10, "winterhold", ClimateRegionSource::LocationDisplayName, -12));
    const auto first = h.Observe(At(100, 10.01, "temperate", ClimateRegionSource::FallbackNoEvidence, 18));
    assert(first.status == TemperatureContinuityStatus::Started);
    const auto second = h.Observe(At(200, 10.02, "falkreath", ClimateRegionSource::ProvisionalSurveySample, -8));
    assert(second.status == TemperatureContinuityStatus::IneligibleHandoff);
    assert(!second.candidateActive && second.previewOutdoorAirC == -8);
    const auto stable = h.Observe(At(300, 10.03, "falkreath", ClimateRegionSource::ProvisionalSurveySample, -8));
    assert(stable.status == TemperatureContinuityStatus::Unchanged);
}
void RejectBrokenSettings() {
    TemperatureContinuitySettings s;
    assert(ValidTemperatureContinuitySettings(s));
    s.fadeDistanceRawUnits = std::numeric_limits<double>::infinity();
    assert(!ValidTemperatureContinuitySettings(s));
    s = {}; s.maxCompensationC = 100;
    assert(!ValidTemperatureContinuitySettings(s));
    s = {}; s.maxLifetimeGameHours = 0;
    assert(!ValidTemperatureContinuitySettings(s));
    s = {}; s.maxCompensationC = std::numeric_limits<double>::quiet_NaN();
    assert(!ValidTemperatureContinuitySettings(s));
}
} // namespace
int main() {
    DisabledIsNoOp();
    EligibleHandoffAndBoundedDecay();
    ExpireOnTimeAndConfigurableCaps();
    NeverSmoothEntrancesToTrustedOrNonSignificantBoundaries();
    InvalidContextAndTeleportsReset();
    SaveReloadResetAndWeakToWeak();
    PreventRepeatedCorrectionChains();
    RejectBrokenSettings();
    std::cout << "Alpha-31 shadow temperature continuity tests passed\n";
}
