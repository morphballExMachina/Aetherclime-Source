#include "Aetherclime/FallbackTransitionAudit.h"
#include <cmath>

namespace aetherclime {
const char* FallbackReportReasonName(FallbackReportReason reason) noexcept {
    switch(reason) {
    case FallbackReportReason::Entered:return "entered-low-confidence";
    case FallbackReportReason::BasisChanged:return "basis-changed";
    case FallbackReportReason::NearbyTemperatureStep:return "nearby-temperature-step";
    default:return "none";
    }
}
FallbackTransitionDecision FallbackTransitionAudit::Observe(const FallbackTransitionSample& current) noexcept {
    if(current.interior || !current.estimateC || !std::isfinite(*current.estimateC) ||
       *current.estimateC < -100 || *current.estimateC > 100) {
        prior_.reset(); return {};
    }
    const bool candidate=current.basis==OutdoorEstimateBasis::NearbyClimateReference ||
        current.basis==OutdoorEstimateBasis::BroadTamriel ||
        current.basis==OutdoorEstimateBasis::GenericWorldspace ||
        current.basis==OutdoorEstimateBasis::NearbyBiome;
    FallbackReportReason reason=FallbackReportReason::None;
    if(candidate) {
        if(!prior_ || prior_->worldspaceFormId!=current.worldspaceFormId) {
            reason=FallbackReportReason::Entered;
        } else if(prior_->basis!=current.basis) {
            reason=FallbackReportReason::BasisChanged;
        } else if(prior_->estimateC && prior_->xRaw && prior_->yRaw &&
                  current.xRaw && current.yRaw &&
                  std::isfinite(*prior_->xRaw) && std::isfinite(*prior_->yRaw) &&
                  std::isfinite(*current.xRaw) && std::isfinite(*current.yRaw) &&
                  std::hypot(*current.xRaw-*prior_->xRaw,*current.yRaw-*prior_->yRaw)<=kNearbyDistanceRaw &&
                  std::abs(*current.estimateC-*prior_->estimateC)>=kTemperatureStepC) {
            reason=FallbackReportReason::NearbyTemperatureStep;
        }
    }
    prior_=current; // Track even after report budget fills; no repeated log flood.
    if(reason==FallbackReportReason::None || reports_>=kMaxReports) return {};
    return {reason,++reports_};
}
} // namespace aetherclime
