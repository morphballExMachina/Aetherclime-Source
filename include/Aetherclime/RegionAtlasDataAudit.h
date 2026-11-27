#pragma once

#include "Aetherclime/Climate.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aetherclime {

// Alpha-29: pure, offline topology preflight for *provisional* atlas content.
// Findings do not mutate configuration, infer hold boundaries, or change the
// existing ambiguity-safe runtime resolver. Config/Climate validates geometry.
enum class AtlasDataFindingKind : std::uint8_t {
    ConflictingAtlasProfiles,
    ConflictingAtlasSurvey,
    DuplicateAtlasLabel,
    SameProfileAtlasOverlap
};

struct AtlasDataFinding {
    AtlasDataFindingKind kind{};
    std::string firstLabel;
    std::string secondLabel;
    std::uint32_t worldspaceFormId{0};
};

[[nodiscard]] std::vector<AtlasDataFinding> AuditAtlasData(const Config& config);

} // namespace aetherclime
