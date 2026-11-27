#include "Aetherclime/TemperatureContinuityPreview.h"
#include "Aetherclime/GeographicEvidence.h"

#include <algorithm>
#include <cmath>

namespace aetherclime {

std::string_view TemperatureContinuityReasonName(TemperatureContinuityReason reason) noexcept {
    switch (reason) {
    case TemperatureContinuityReason::DisabledBySetting: return "preview-disabled";
    case TemperatureContinuityReason::InvalidSettings: return "invalid-preview-settings";
    case TemperatureContinuityReason::MissingExteriorEvidence: return "missing-exterior-evidence";
    case TemperatureContinuityReason::FirstBaseline: return "first-baseline";
    case TemperatureContinuityReason::ComparisonRejected: return "non-comparable-reset";
    case TemperatureContinuityReason::SameProfile: return "profile-unchanged";
    case TemperatureContinuityReason::ExistingTransitionCancelled: return "overlapping-profile-step-cancelled";
    case TemperatureContinuityReason::BelowStepThreshold: return "below-significant-step";
    case TemperatureContinuityReason::SourceUnchanged: return "source-unchanged";
    case TemperatureContinuityReason::DestinationNotWeak: return "destination-not-weak";
    case TemperatureContinuityReason::ZeroOrInvalidOffset: return "no-finite-offset";
    case TemperatureContinuityReason::EligibleStep: return "eligible-profile-step";
    case TemperatureContinuityReason::ActiveTransitionDecay: return "active-shadow-decay";
    case TemperatureContinuityReason::LifetimeOrDistanceExhausted: return "fade-limit-reached";
    case TemperatureContinuityReason::NonfiniteCandidate: return "nonfinite-shadow-reset";
    }
    return "unknown";
}

bool ValidTemperatureContinuitySettings(const TemperatureContinuitySettings& s) noexcept {
    return std::isfinite(s.fadeDistanceRawUnits) && s.fadeDistanceRawUnits >= 512.0 &&
           s.fadeDistanceRawUnits <= 16384.0 &&
           std::isfinite(s.maxCompensationC) && s.maxCompensationC >= 0.5 &&
           s.maxCompensationC <= 10.0 &&
           std::isfinite(s.maxLifetimeGameHours) && s.maxLifetimeGameHours >= 0.05 &&
           s.maxLifetimeGameHours <= 1.0;
}

std::string_view TemperatureContinuityStatusName(TemperatureContinuityStatus s) noexcept {
    switch (s) {
    case TemperatureContinuityStatus::Disabled: return "disabled";
    case TemperatureContinuityStatus::NoEvidence: return "no-exterior-evidence";
    case TemperatureContinuityStatus::Baseline: return "baseline";
    case TemperatureContinuityStatus::NonComparable: return "non-comparable-reset";
    case TemperatureContinuityStatus::Unchanged: return "unchanged";
    case TemperatureContinuityStatus::IneligibleHandoff: return "ineligible-handoff";
    case TemperatureContinuityStatus::Started: return "shadow-started";
    case TemperatureContinuityStatus::Fading: return "shadow-fading";
    case TemperatureContinuityStatus::Expired: return "shadow-expired";
    }
    return "unknown";
}

void TemperatureContinuityPreview::Reset() noexcept {
    previous_.reset();
    active_.reset();
    previousCandidateC_ = 0.0;
}

TemperatureContinuityResult TemperatureContinuityPreview::Observe(
    const TemperatureHandoffSample& current,
    const TemperatureHandoffAssessment& handoff,
    const TemperatureContinuitySettings& settings) {
    TemperatureContinuityResult out;
    out.rawOutdoorAirC = current.outdoorAirC;
    out.previewOutdoorAirC = current.outdoorAirC;
    if (!settings.previewEnabled || !ValidTemperatureContinuitySettings(settings)) {
        out.reason = settings.previewEnabled ? TemperatureContinuityReason::InvalidSettings
                                             : TemperatureContinuityReason::DisabledBySetting;
        Reset();
        return out;
    }
    // These statuses represent the audit's conservative input validation and
    // adjacency checks; do not carry a correction across a teleported load,
    // elapsed day, weather change, indoor visit, or unknown climate evidence.
    if (handoff.status == TemperatureHandoffStatus::NoExteriorEvidence ||
        !std::isfinite(current.outdoorAirC)) {
        Reset();
        out.status = TemperatureContinuityStatus::NoEvidence;
        out.reason = TemperatureContinuityReason::MissingExteriorEvidence;
        return out;
    }
    if (handoff.status == TemperatureHandoffStatus::Baseline) {
        Reset();
        previous_ = current;
        previousCandidateC_ = current.outdoorAirC;
        out.status = TemperatureContinuityStatus::Baseline;
        out.reason = TemperatureContinuityReason::FirstBaseline;
        return out;
    }
    if (!handoff.comparable || !previous_ || !current.position || !previous_->position) {
        Reset();
        // The CURRENT valid sample is the new baseline; do not blend it.
        previous_ = current;
        previousCandidateC_ = current.outdoorAirC;
        out.status = TemperatureContinuityStatus::NonComparable;
        out.reason = TemperatureContinuityReason::ComparisonRejected;
        return out;
    }
    const double previousShadow = previousCandidateC_;
    const double hop = std::hypot(current.position->xRaw - previous_->position->xRaw,
                                  current.position->yRaw - previous_->position->yRaw);
    previous_ = current;
    if (!std::isfinite(hop)) {
        active_.reset();
        previousCandidateC_ = current.outdoorAirC;
        out.status = TemperatureContinuityStatus::NonComparable;
        out.reason = TemperatureContinuityReason::ComparisonRejected;
        return out;
    }

    if (handoff.profileChanged) {
        // Do not chain a second correction while the previous correction is
        // active: repeated uncertain profile toggles must not sustain a
        // synthetic temperature indefinitely.
        if (active_) {
            active_.reset();
            previousCandidateC_ = current.outdoorAirC;
            out.status = TemperatureContinuityStatus::IneligibleHandoff;
            out.reason = TemperatureContinuityReason::ExistingTransitionCancelled;
            return out;
        }
        // Only propose corrections when *entering* untrusted/provisional
        // geography. Never modify even the shadow candidate for entry into a
        // trusted named place or anchor, or for a same-source named boundary.
        if (!handoff.significant || !handoff.sourceChanged || !IsWeakTemperatureEvidence(current.source)) {
            active_.reset();
            previousCandidateC_ = current.outdoorAirC;
            out.status = TemperatureContinuityStatus::IneligibleHandoff;
            out.reason = !handoff.significant ? TemperatureContinuityReason::BelowStepThreshold :
                !handoff.sourceChanged ? TemperatureContinuityReason::SourceUnchanged :
                TemperatureContinuityReason::DestinationNotWeak;
            return out;
        }
        const double difference = previousShadow - current.outdoorAirC;
        const double initial = std::clamp(difference,
            -settings.maxCompensationC, settings.maxCompensationC);
        if (!std::isfinite(initial) || initial == 0.0) {
            active_.reset();
            previousCandidateC_ = current.outdoorAirC;
            out.status = TemperatureContinuityStatus::Unchanged;
            out.reason = TemperatureContinuityReason::ZeroOrInvalidOffset;
            return out;
        }
        out.compensationCapped = std::abs(difference) > settings.maxCompensationC;
        active_ = ActiveTransition{initial, current.date.hour, 0.0, out.compensationCapped};
        out.status = TemperatureContinuityStatus::Started;
        out.reason = TemperatureContinuityReason::EligibleStep;
    } else if (active_) {
        active_->traveledRawUnits += hop;
        out.status = TemperatureContinuityStatus::Fading;
        out.reason = TemperatureContinuityReason::ActiveTransitionDecay;
    } else {
        out.status = TemperatureContinuityStatus::Unchanged;
        out.reason = TemperatureContinuityReason::SameProfile;
    }

    if (active_) {
        out.compensationCapped = active_->compensationCapped;
        out.pathDistanceRawUnits = active_->traveledRawUnits;
        out.elapsedGameHours = current.date.hour - active_->startingHour;
        // Clamp both diagnostics to [0,1] even at/after expiry. The same
        // factors are used in the shadow math, not inferred later from logs.
        out.distanceRemainingFraction = std::clamp(
            1.0 - out.pathDistanceRawUnits / settings.fadeDistanceRawUnits, 0.0, 1.0);
        out.timeRemainingFraction = std::isfinite(out.elapsedGameHours) ? std::clamp(
            1.0 - out.elapsedGameHours / settings.maxLifetimeGameHours, 0.0, 1.0) : 0.0;
        if (!std::isfinite(out.elapsedGameHours) || out.elapsedGameHours < 0.0 ||
            out.elapsedGameHours >= settings.maxLifetimeGameHours ||
            out.pathDistanceRawUnits >= settings.fadeDistanceRawUnits) {
            active_.reset();
            out.status = TemperatureContinuityStatus::Expired;
            out.reason = TemperatureContinuityReason::LifetimeOrDistanceExhausted;
        } else {
            out.previewOffsetC = active_->initialOffsetC *
                out.distanceRemainingFraction * out.timeRemainingFraction;
            out.previewOutdoorAirC = current.outdoorAirC + out.previewOffsetC;
            if (std::isfinite(out.previewOutdoorAirC)) out.candidateActive = true;
            else {
                out.previewOutdoorAirC = current.outdoorAirC;
                out.previewOffsetC = 0.0;
                active_.reset();
                out.status = TemperatureContinuityStatus::NonComparable;
                out.reason = TemperatureContinuityReason::NonfiniteCandidate;
            }
        }
    }
    previousCandidateC_ = out.previewOutdoorAirC;
    return out;
}

} // namespace aetherclime
