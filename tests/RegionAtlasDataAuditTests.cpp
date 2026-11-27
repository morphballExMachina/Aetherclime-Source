#include "Aetherclime/RegionAtlasDataAudit.h"

#include <cassert>
#include <iostream>

using namespace aetherclime;

namespace {
void Has(const std::vector<AtlasDataFinding>& findings,
         AtlasDataFindingKind kind, std::size_t count = 1) {
    std::size_t actual = 0;
    for (const auto& f : findings) if (f.kind == kind) ++actual;
    assert(actual == count);
}
ProvisionalAtlasZone Zone(const char* label, const char* region,
                          std::uint32_t world, double minX, double maxX,
                          double minY = 0.0, double maxY = 100.0) {
    return {label, region, world, minX, maxX, minY, maxY};
}
}

int main() {
    const Config defaults = DefaultConfig();
    assert(AuditAtlasData(defaults).empty()); // Original pilot and survey agree.

    Config cfg = defaults;
    cfg.provisionalSurveySamples.clear();
    cfg.provisionalAtlasZones = {
        Zone("Winterhold", "winterhold", 0x3C, 0.0, 100.0),
        Zone("Pale", "pale", 0x3C, 100.0, 200.0)
    };
    auto findings = AuditAtlasData(cfg);
    Has(findings, AtlasDataFindingKind::ConflictingAtlasProfiles);
    assert(findings[0].firstLabel == "Winterhold");
    assert(findings[0].secondLabel == "Pale");
    assert(findings[0].worldspaceFormId == 0x3C);

    cfg.provisionalAtlasZones[1].worldspaceFormId = 0x99;
    assert(AuditAtlasData(cfg).empty()); // Other worldspace never overlaps.

    cfg.provisionalAtlasZones[1].worldspaceFormId = 0x3C;
    cfg.provisionalAtlasZones[1].minXRaw = 100.01;
    assert(AuditAtlasData(cfg).empty()); // Gap, not a shared inclusive edge.

    cfg.provisionalAtlasZones[1] = Zone("Neighbor", "winterhold", 0x3C, 100.0, 200.0);
    Has(AuditAtlasData(cfg), AtlasDataFindingKind::SameProfileAtlasOverlap);

    cfg.provisionalAtlasZones[1].label = "Winterhold";
    findings = AuditAtlasData(cfg);
    Has(findings, AtlasDataFindingKind::DuplicateAtlasLabel);
    Has(findings, AtlasDataFindingKind::SameProfileAtlasOverlap);

    cfg.provisionalAtlasZones = {Zone("Atlas", "winterhold", 0x3C, 0.0, 100.0)};
    cfg.provisionalSurveySamples = { {"Other profile sample", "pale", 0x3C, 120.0, 50.0, 20.0} };
    Has(AuditAtlasData(cfg), AtlasDataFindingKind::ConflictingAtlasSurvey); // Touching.
    cfg.provisionalSurveySamples[0].radiusRawUnits = 19.0;
    assert(AuditAtlasData(cfg).empty());
    cfg.provisionalSurveySamples[0].worldspaceFormId = 0x88;
    cfg.provisionalSurveySamples[0].radiusRawUnits = 20.0;
    assert(AuditAtlasData(cfg).empty());
    cfg.provisionalSurveySamples[0].worldspaceFormId = 0x3C;
    cfg.provisionalSurveySamples[0].regionKey = "winterhold";
    assert(AuditAtlasData(cfg).empty()); // Agreeing survey is not an error.

    // Report only; actual geographic evaluator remains unchanged.
    assert(defaults.provisionalAtlasZones.size() == 1);
    std::cout << "Alpha-29 atlas data topology audit: passed\n";
}
