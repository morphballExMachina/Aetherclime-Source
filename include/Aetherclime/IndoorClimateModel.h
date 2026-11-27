#pragma once

#include "Aetherclime/Climate.h"
#include "Aetherclime/EventClimateContext.h"
#include <string>
#include <string_view>

namespace aetherclime {

enum class InteriorClimateStatus : unsigned char {
    MissingCell,
    Exterior,
    UnclassifiedInterior,
    AmbiguousInnRule,
    ConflictingRegionEvidence,
    RecognizedInn
};

std::string_view InteriorClimateStatusName(InteriorClimateStatus status) noexcept;

struct InteriorClimateContext {
    InteriorClimateStatus status{InteriorClimateStatus::MissingCell};
    std::string regionKey;             // outdoor regional key, never interior-cell coordinates
    std::string matchedRule;           // recognized inn name, empty otherwise
    bool eligible{false};
    bool usedRuleRegion{false};        // only when Skyrim name evidence was absent
};

// Inputs are copied out of engine records before calling. Never infer an
// interior world position from CELL local coordinates. A named inn may supply
// a region only when the normal location resolver did not find one.
InteriorClimateContext ResolveInteriorClimateContext(
    const Config& config,
    bool hasCell, bool isInterior,
    std::string_view currentLocationName, std::string_view cellDisplayName,
    std::string_view resolvedRegion, bool regionFromTrustedEvidence);

enum class IndoorEstimateBasis : unsigned char { Unavailable, ExplicitInn, BuildingClass, Generic };
std::string_view IndoorEstimateBasisName(IndoorEstimateBasis) noexcept;

struct IndoorAmbientAssessment {
    IndoorEstimateBasis basis{IndoorEstimateBasis::Unavailable};
    InteriorClimateContext context;
    bool enabled{false};
    bool available{false};
    double outdoorAirC{0.0};
    double projectedAmbientC{0.0};
    double actualAmbientC{0.0};
    double projectedDeltaC{0.0};
    bool clamped{false};
};

// Pure calculation: same conceptual heating in both inns; local outdoor
// weather changes the projected ambient temperature. It is NOT measured heat
// from an active fireplace or player-perceived warmth.
IndoorAmbientAssessment AssessIndoorAmbient(
    const Config& config, InteriorClimateContext context, double outdoorAirC,
    BuildingKind buildingKind = BuildingKind::UnknownInterior);

} // namespace aetherclime
