#include "Aetherclime/GeographicEvidence.h"
#include "Aetherclime/TemperatureContinuityPreview.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace aetherclime;
namespace {
constexpr auto Named = ClimateRegionSource::LocationDisplayName;
constexpr auto Editor = ClimateRegionSource::LocationEditorId;
constexpr auto Anchor = ClimateRegionSource::GeographicAnchorProximity;
constexpr auto Survey = ClimateRegionSource::ProvisionalSurveySample;
constexpr auto Atlas = ClimateRegionSource::ProvisionalAtlasZone;
constexpr auto NoEvidence = ClimateRegionSource::FallbackNoEvidence;

void EverySourceHasAProvenance() {
    constexpr std::array named{Named, Editor};
    constexpr std::array anchors{Anchor};
    constexpr std::array provisional{
        ClimateRegionSource::ProvisionalRecentExterior, Survey, Atlas};
    constexpr std::array fallback{
        ClimateRegionSource::FallbackGeographyAmbiguous,
        ClimateRegionSource::FallbackGeographyUnconfigured,
        NoEvidence, ClimateRegionSource::FallbackUnmatched,
        ClimateRegionSource::FallbackAmbiguous,
        ClimateRegionSource::FallbackUnconfigured};
    for (auto source : named) {
        assert(ClassifyGeographicEvidence(source) == GeographicEvidenceTier::NamedLocation);
        assert(!IsWeakTemperatureEvidence(source));
    }
    for (auto source : anchors) {
        assert(ClassifyGeographicEvidence(source) == GeographicEvidenceTier::AnchorProximity);
        assert(!IsWeakTemperatureEvidence(source));
    }
    for (auto source : provisional) {
        assert(ClassifyGeographicEvidence(source) == GeographicEvidenceTier::Provisional);
        assert(IsWeakTemperatureEvidence(source));
    }
    for (auto source : fallback) {
        assert(ClassifyGeographicEvidence(source) == GeographicEvidenceTier::Fallback);
        assert(IsWeakTemperatureEvidence(source));
    }
    assert(GeographicEvidenceTierName(GeographicEvidenceTier::Provisional) == "provisional");
    assert(GeographicEvidenceTierName(GeographicEvidenceTier::NamedLocation) == "named-location");
}
void TransitionsAreDiagnosticOnly() {
    assert(CompareGeographicEvidence(Named, Survey, false) == GeographicEvidenceChange::NotComparable);
    assert(CompareGeographicEvidence(Named, Named, true) == GeographicEvidenceChange::SameSource);
    assert(CompareGeographicEvidence(Named, Editor, true) == GeographicEvidenceChange::SameTierDifferentSource);
    assert(CompareGeographicEvidence(Survey, Atlas, true) == GeographicEvidenceChange::SameTierDifferentSource);
    assert(CompareGeographicEvidence(Named, Survey, true) == GeographicEvidenceChange::TowardWeaker);
    assert(CompareGeographicEvidence(Anchor, NoEvidence, true) == GeographicEvidenceChange::TowardWeaker);
    assert(CompareGeographicEvidence(NoEvidence, Survey, true) == GeographicEvidenceChange::TowardStronger);
    assert(CompareGeographicEvidence(Survey, Anchor, true) == GeographicEvidenceChange::TowardStronger);
    assert(GeographicEvidenceChangeName(GeographicEvidenceChange::NotComparable) == "not-comparable");
}
TemperatureHandoffSample Sample(double x, double hour, const char* region,
                                 ClimateRegionSource source, double temperature) {
    TemperatureHandoffSample s;
    s.exterior = true;
    s.position = ClimateWorldPoint{0x3C, x, 1000};
    s.date = {0, 3, hour, 201};
    s.regionKey = region;
    s.source = source;
    s.weather = Weather::Snow;
    s.outdoorAirC = temperature;
    return s;
}
struct Harness {
    TemperatureHandoffAudit audit;
    TemperatureContinuityPreview preview;
    TemperatureContinuitySettings settings;
    TemperatureContinuityResult Observe(const TemperatureHandoffSample& s) {
        const auto handoff = audit.Observe(s);
        return preview.Observe(s, handoff, settings);
    }
};
void DecisionReasonsExplainConservativeGates() {
    {
        Harness h;
        const auto disabled = h.Observe(Sample(0, 10, "winterhold", Named, -12));
        assert(disabled.status == TemperatureContinuityStatus::Disabled);
        assert(disabled.reason == TemperatureContinuityReason::DisabledBySetting);
    }
    {
        Harness h; h.settings.previewEnabled = true;
        assert(h.Observe(Sample(0, 10, "winterhold", Named, -12)).reason == TemperatureContinuityReason::FirstBaseline);
        const auto same = h.Observe(Sample(100, 10.01, "winterhold", Survey, -12.1));
        assert(same.reason == TemperatureContinuityReason::SameProfile);
        const auto small = h.Observe(Sample(200, 10.02, "temperate", NoEvidence, -10));
        assert(small.reason == TemperatureContinuityReason::BelowStepThreshold);
        const auto trusted = h.Observe(Sample(300, 10.03, "winterhold", Anchor, -20));
        assert(trusted.reason == TemperatureContinuityReason::DestinationNotWeak);
        const auto sameSource = h.Observe(Sample(350, 10.04, "temperate", Anchor, 20));
        assert(sameSource.reason == TemperatureContinuityReason::SourceUnchanged);
        assert(!sameSource.candidateActive && sameSource.previewOutdoorAirC == 20);
    }
    {
        Harness h; h.settings.previewEnabled = true;
        (void)h.Observe(Sample(0, 10, "winterhold", Named, -12));
        auto close = Sample(100, 10.01, "temperate", NoEvidence, 18);
        close.weather = Weather::Rain;
        assert(h.Observe(close).reason == TemperatureContinuityReason::ComparisonRejected);
        assert(h.Observe(Sample(110, 10.02, "winterhold", Named, -12)).reason ==
               TemperatureContinuityReason::ComparisonRejected); // weather differed
    }
}
void FadeFractionsAndReasonsShareActualPreviewMath() {
    Harness h; h.settings.previewEnabled = true;
    (void)h.Observe(Sample(0, 10, "winterhold", Named, -12));
    const auto start = h.Observe(Sample(100, 10.01, "temperate", NoEvidence, 18));
    assert(start.status == TemperatureContinuityStatus::Started);
    assert(start.reason == TemperatureContinuityReason::EligibleStep);
    assert(start.distanceRemainingFraction == 1.0);
    assert(start.timeRemainingFraction == 1.0);
    assert(start.compensationCapped);
    assert(start.previewOffsetC == -6);
    const auto fade = h.Observe(Sample(1124, 10.11, "temperate", NoEvidence, 19));
    assert(fade.status == TemperatureContinuityStatus::Fading);
    assert(fade.reason == TemperatureContinuityReason::ActiveTransitionDecay);
    assert(std::abs(fade.distanceRemainingFraction - 0.75) < 1e-8);
    assert(std::abs(fade.timeRemainingFraction - 0.8) < 1e-8);
    assert(fade.compensationCapped);
    assert(std::abs(fade.previewOffsetC - (-6 * .75 * .8)) < 1e-8);
    const auto expired = h.Observe(Sample(5000, 10.20, "temperate", NoEvidence, 19));
    assert(expired.status == TemperatureContinuityStatus::Expired);
    assert(expired.reason == TemperatureContinuityReason::LifetimeOrDistanceExhausted);
    assert(expired.distanceRemainingFraction == 0.0);
    assert(!expired.candidateActive && expired.previewOutdoorAirC == 19);
    const auto settled = h.Observe(Sample(5100, 10.21, "temperate", NoEvidence, 19));
    assert(settled.reason == TemperatureContinuityReason::SameProfile);
}
void NoChainingOrTrustPromotion() {
    Harness h; h.settings.previewEnabled = true;
    (void)h.Observe(Sample(0, 10, "winterhold", Named, -12));
    (void)h.Observe(Sample(100, 10.01, "temperate", NoEvidence, 18));
    const auto overlap = h.Observe(Sample(200, 10.02, "rift", Survey, -11));
    assert(overlap.reason == TemperatureContinuityReason::ExistingTransitionCancelled);
    assert(!overlap.candidateActive && overlap.previewOutdoorAirC == -11);
    // Classification is a read-only function: source/trust flags cannot change.
    assert(IsWeakTemperatureEvidence(Survey));
    assert(ClassifyGeographicEvidence(Survey) == GeographicEvidenceTier::Provisional);
    const auto after = h.Observe(Sample(300, 10.03, "rift", Survey, -11));
    assert(after.reason == TemperatureContinuityReason::SameProfile);
}
void InvalidConfigDoesNotGenerateProposal() {
    Harness h; h.settings.previewEnabled = true;
    h.settings.maxCompensationC = std::numeric_limits<double>::quiet_NaN();
    const auto result = h.Observe(Sample(0, 10, "winterhold", Named, -12));
    assert(result.status == TemperatureContinuityStatus::Disabled);
    assert(result.reason == TemperatureContinuityReason::InvalidSettings);
    assert(!result.candidateActive && !result.publishedTemperatureModified);
}
} // namespace
int main() {
    EverySourceHasAProvenance();
    TransitionsAreDiagnosticOnly();
    DecisionReasonsExplainConservativeGates();
    FadeFractionsAndReasonsShareActualPreviewMath();
    NoChainingOrTrustPromotion();
    InvalidConfigDoesNotGenerateProposal();
    std::cout << "Alpha-32 geographic evidence and shadow decision tests passed\n";
}
