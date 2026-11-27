#include "Aetherclime/IndoorClimateModel.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <utility>

namespace aetherclime {
namespace {
    bool EqualAsciiIgnoreCase(std::string_view left, std::string_view right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            const auto a = static_cast<unsigned char>(left[i]);
            const auto b = static_cast<unsigned char>(right[i]);
            if (std::tolower(a) != std::tolower(b)) return false;
        }
        return true;
    }
}

std::string_view InteriorClimateStatusName(InteriorClimateStatus status) noexcept {
    switch (status) {
    case InteriorClimateStatus::MissingCell: return "no-cell-evidence";
    case InteriorClimateStatus::Exterior: return "exterior-outdoor-air";
    case InteriorClimateStatus::UnclassifiedInterior: return "unclassified-interior";
    case InteriorClimateStatus::AmbiguousInnRule: return "ambiguous-inn-rules";
    case InteriorClimateStatus::ConflictingRegionEvidence: return "conflicting-region-evidence";
    case InteriorClimateStatus::RecognizedInn: return "recognized-heated-inn";
    }
    return "unknown";
}

InteriorClimateContext ResolveInteriorClimateContext(
    const Config& config, bool hasCell, bool isInterior,
    std::string_view currentLocationName, std::string_view cellDisplayName,
    std::string_view resolvedRegion, bool regionFromTrustedEvidence) {
    InteriorClimateContext answer;
    answer.regionKey = std::string(resolvedRegion);
    if (!hasCell) return answer;
    if (!isInterior) {
        answer.status = InteriorClimateStatus::Exterior;
        return answer;
    }
    answer.status = InteriorClimateStatus::UnclassifiedInterior;
    const InteriorLocationRule* selected = nullptr;
    for (const auto& rule : config.interior.locations) {
        if (!EqualAsciiIgnoreCase(rule.name, currentLocationName) &&
            !EqualAsciiIgnoreCase(rule.name, cellDisplayName)) continue;
        if (selected && (selected->regionKey != rule.regionKey ||
                         !EqualAsciiIgnoreCase(selected->name, rule.name))) {
            answer.status = InteriorClimateStatus::AmbiguousInnRule;
            return answer;
        }
        selected = &rule;
    }
    if (!selected) return answer;
    answer.matchedRule = selected->name;
    if (regionFromTrustedEvidence && selected->regionKey != resolvedRegion) {
        answer.status = InteriorClimateStatus::ConflictingRegionEvidence;
        return answer;
    }
    answer.status = InteriorClimateStatus::RecognizedInn;
    answer.eligible = true;
    if (!regionFromTrustedEvidence) {
        answer.regionKey = selected->regionKey;
        answer.usedRuleRegion = true;
    }
    return answer;
}

std::string_view IndoorEstimateBasisName(IndoorEstimateBasis basis) noexcept {
    switch (basis) {
    case IndoorEstimateBasis::ExplicitInn:return "explicit-inn-rule";
    case IndoorEstimateBasis::BuildingClass:return "building-class-estimate";
    case IndoorEstimateBasis::Generic:return "generic-interior-estimate";
    default:return "unavailable";
    }
}

IndoorAmbientAssessment AssessIndoorAmbient(
    const Config& config, InteriorClimateContext context, double outdoorAirC, BuildingKind kind) {
    IndoorAmbientAssessment out;
    out.context = std::move(context);
    out.enabled = config.interior.enabled;
    out.outdoorAirC = outdoorAirC;
    const auto& s=config.interior;
    if (!std::isfinite(outdoorAirC) || !s.enabled) return out;
    const bool explicitInn = out.context.eligible;
    const bool generic = !explicitInn && s.estimateUnclassified &&
        (out.context.status == InteriorClimateStatus::UnclassifiedInterior ||
         out.context.status == InteriorClimateStatus::ConflictingRegionEvidence ||
         out.context.status == InteriorClimateStatus::AmbiguousInnRule);
    if (!explicitInn && !generic) return out;
    double baseline=s.baselineC, coupling=s.outdoorCoupling;
    double floor=s.minAmbientC, ceiling=s.maxAmbientC;
    if (generic) {
        out.basis = IndoorEstimateBasis::BuildingClass;
        // Constants are deliberately broad conceptual shelter models, never
        // inferred heat from nearby fire references or object scanning.
        switch (kind) {
        case BuildingKind::Inn:case BuildingKind::Home:case BuildingKind::PalaceOrCourt:
            baseline=18.0;coupling=0.20;floor=7.0;ceiling=29.0;break;
        case BuildingKind::Shop:case BuildingKind::Hall:case BuildingKind::TempleOrShrine:
            baseline=17.0;coupling=0.25;floor=5.0;ceiling=29.0;break;
        case BuildingKind::CaveOrMine:
            baseline=10.0;coupling=0.10;floor=0.0;ceiling=18.0;break;
        case BuildingKind::RuinOrCrypt:case BuildingKind::Funerary:
            baseline=11.0;coupling=0.35;floor=-8.0;ceiling=25.0;break;
        default:
            out.basis=IndoorEstimateBasis::Generic;
            baseline=15.0;coupling=0.20;floor=0.0;ceiling=27.0;break;
        }
    } else out.basis=IndoorEstimateBasis::ExplicitInn;
    const double unbounded=baseline+coupling*outdoorAirC;
    out.projectedAmbientC=std::clamp(unbounded,floor,ceiling);
    out.projectedDeltaC=out.projectedAmbientC-outdoorAirC;
    out.clamped=std::abs(unbounded-out.projectedAmbientC)>1e-9;
    out.available=true;
    out.actualAmbientC=out.projectedAmbientC;
    return out;
}
} // namespace aetherclime
