#include "Aetherclime/TemperatureHandoffCoverage.h"

#include <cmath>
#include <cstdint>
#include <utility>

namespace aetherclime {

TemperatureHandoffStepClass ClassifyTemperatureHandoffStep(
    const TemperatureHandoffAssessment& assessment,
    ClimateRegionSource currentSource) noexcept {
    if (!assessment.comparable) return TemperatureHandoffStepClass::NotComparable;
    if (!assessment.profileChanged) return TemperatureHandoffStepClass::SameProfile;
    if (!assessment.significant) return TemperatureHandoffStepClass::ProfileStepBelowThreshold;
    switch (CompareGeographicEvidence(assessment.previousSource, currentSource, true)) {
    case GeographicEvidenceChange::TowardStronger:
        return TemperatureHandoffStepClass::SignificantTowardStronger;
    case GeographicEvidenceChange::TowardWeaker:
        return TemperatureHandoffStepClass::SignificantTowardWeaker;
    default:
        return TemperatureHandoffStepClass::SignificantOther;
    }
}

std::string_view TemperatureHandoffStepClassName(TemperatureHandoffStepClass value) noexcept {
    switch (value) {
    case TemperatureHandoffStepClass::NotComparable: return "not-comparable";
    case TemperatureHandoffStepClass::SameProfile: return "same-profile";
    case TemperatureHandoffStepClass::ProfileStepBelowThreshold: return "profile-step-below-threshold";
    case TemperatureHandoffStepClass::SignificantTowardStronger: return "significant-toward-stronger";
    case TemperatureHandoffStepClass::SignificantTowardWeaker: return "significant-toward-weaker";
    case TemperatureHandoffStepClass::SignificantOther: return "significant-other";
    }
    return "unknown";
}

void TemperatureHandoffCoverage::Record(
    const TemperatureHandoffSample& current, const TemperatureHandoffAssessment& assessment) {
    ++data_.observations;
    // One observation contributes one source sample, even if its comparison
    // is rejected (teleport/time/weather). Invalid exteriors contribute none.
    if (assessment.status != TemperatureHandoffStatus::NoExteriorEvidence) {
        switch (ClassifyGeographicEvidence(current.source)) {
        case GeographicEvidenceTier::Fallback: ++data_.fallbackSamples; break;
        case GeographicEvidenceTier::Provisional: ++data_.provisionalSamples; break;
        case GeographicEvidenceTier::AnchorProximity: ++data_.anchorSamples; break;
        case GeographicEvidenceTier::NamedLocation: ++data_.namedSamples; break;
        }
    }
    if (assessment.status == TemperatureHandoffStatus::Baseline) {
        ++data_.baselines;
        return;
    }
    if (assessment.status == TemperatureHandoffStatus::NoExteriorEvidence) {
        ++data_.invalidExteriorSamples;
        return;
    }
    if (!assessment.comparable) {
        ++data_.rejectedComparisons;
        switch (assessment.status) {
        case TemperatureHandoffStatus::DifferentWorldspace:
            ++data_.rejectedWorldspace; break;
        case TemperatureHandoffStatus::DifferentCalendarDay:
        case TemperatureHandoffStatus::InvalidTime:
            ++data_.rejectedDayOrTime; break;
        case TemperatureHandoffStatus::TooOld:
        case TemperatureHandoffStatus::TooFar:
            ++data_.rejectedDistanceOrAge; break;
        case TemperatureHandoffStatus::WeatherChanged:
            ++data_.rejectedWeather; break;
        default: break;
        }
        return;
    }
    ++data_.comparablePairs;
    if (assessment.sourceChanged && !assessment.profileChanged) {
        ++data_.sourceSwitchSameProfile;
    }
    if (assessment.sourceChanged) {
        const auto from = ClassifyGeographicEvidence(assessment.previousSource);
        const auto to = ClassifyGeographicEvidence(current.source);
        // The 4-tier source model has 16 possible ordered tier pairs.
        const unsigned index = static_cast<unsigned>(from) * 4U + static_cast<unsigned>(to);
        const std::uint16_t bit = static_cast<std::uint16_t>(1U << index);
        if ((routesMask_ & bit) == 0) {
            routesMask_ = static_cast<std::uint16_t>(routesMask_ | bit);
            ++data_.uniqueTierRoutes;
        }
        if (from == GeographicEvidenceTier::Fallback && to == GeographicEvidenceTier::AnchorProximity)
            ++data_.fallbackToAnchor;
        if (from == GeographicEvidenceTier::AnchorProximity && to == GeographicEvidenceTier::Fallback)
            ++data_.anchorToFallback;
    }
    if (!assessment.profileChanged) return;
    if (!assessment.significant) {
        ++data_.profileStepsBelowThreshold;
    } else {
        ++data_.significantProfileSteps;
        switch (ClassifyTemperatureHandoffStep(assessment, current.source)) {
        case TemperatureHandoffStepClass::SignificantTowardStronger:
            ++data_.significantTowardStronger;
            break;
        case TemperatureHandoffStepClass::SignificantTowardWeaker:
            ++data_.significantTowardWeaker;
            break;
        default:
            ++data_.significantOther;
            break;
        }
    }
    // Peak covers ALL comparable profile changes, including sub-threshold
    // steps. Never compare across incompatible timestamps, weather or worlds.
    if (std::isfinite(assessment.airDeltaC) &&
        (!data_.hasPeakProfileStep ||
         std::abs(assessment.airDeltaC) > std::abs(data_.peakSignedProfileStepC))) {
        data_.hasPeakProfileStep = true;
        data_.peakSignedProfileStepC = assessment.airDeltaC;
        data_.peakDistanceRawUnits = assessment.distanceRawUnits;
        data_.peakAgeGameHours = assessment.ageGameHours;
        data_.peakPreviousSource = assessment.previousSource;
        data_.peakCurrentSource = current.source;
        data_.peakPreviousRegion = assessment.previousRegion;
        data_.peakCurrentRegion = current.regionKey;
    }
}

bool TemperatureHandoffReportBudget::Allow(bool reportable, bool priority) noexcept {
    if (!reportable || total_ >= kTotalLimit) return false;
    if (!priority && routine_ >= kRoutineLimit) return false;
    ++total_;
    if (!priority) ++routine_;
    return true;
}

} // namespace aetherclime
