#include "Aetherclime/WeatherPriorityAudit.h"

#include <cassert>
#include <iostream>
#include <vector>

using namespace aetherclime;

namespace {
RegionWeatherCandidate Make(std::uint32_t id, int priority, bool overrideFlag,
                            RegionFootprintStatus footprint, std::uint32_t weatherId = 0xAB) {
    RegionWeatherCandidate c;
    c.regionFormId = id;
    c.worldspaceFormId = 0x3C;
    c.priority = static_cast<std::uint8_t>(priority);
    c.overrideFlag = overrideFlag;
    c.footprint.status = footprint;
    c.choices.push_back({weatherId, 10, 0});
    return c;
}
}

int main() {
    using S = RegionFootprintStatus;
    using A = WeatherPriorityStatus;
    const auto inside = S::InsideRecordedPolygon;
    const auto outside = S::OutsideRecordedPolygons;

    // Winterhold: high-priority small rectangle + broad snow footprint,
    // and coastal record linked via XCLR but outside the player's point.
    const auto winterhold = AuditWeatherRegionPriorities({
        Make(0x0010FDC6, 65, true, inside),
        Make(0x000C5853, 60, true, inside),
        Make(0x000C5852, 50, true, outside)}, 0xAB);
    assert(winterhold.status == A::UniqueOverridePriorityLeader);
    assert(winterhold.leadingRegionFormId == 0x0010FDC6);
    assert(winterhold.leadingPriority == 65 && winterhold.leadingOverrideFlag);
    assert(winterhold.insideCount == 2 && winterhold.outsideCount == 1);
    assert(winterhold.indeterminateCount == 0 && winterhold.observedWeatherInLeader);

    // Falkreath: two overlapping weather regions. Display order irrelevant.
    const auto falkreath = AuditWeatherRegionPriorities({
        Make(0x0002A72D, 52, true, inside),
        Make(0x0010474D, 95, true, inside, 0xDD)}, 0xAB);
    assert(falkreath.status == A::UniqueOverridePriorityLeader);
    assert(falkreath.leadingRegionFormId == 0x0010474D);
    assert(!falkreath.observedWeatherInLeader); // NOT proof leader is wrong

    const auto whiterun = AuditWeatherRegionPriorities({Make(0x000C5858, 50, true, inside)}, 0xAB);
    assert(whiterun.status == A::OnlyInsideCandidate && whiterun.insideCount == 1);
    assert(whiterun.leadingRegionFormId == 0x000C5858);

    const auto noRecords = AuditWeatherRegionPriorities({}, 0xAB);
    assert(noRecords.status == A::NoWeatherEntries && noRecords.leadingRegionFormId == 0);
    const auto noInside = AuditWeatherRegionPriorities({Make(1, 80, true, outside)}, 0xAB);
    assert(noInside.status == A::NoInsideCandidates && noInside.leadingRegionFormId == 0);
    const auto tied = AuditWeatherRegionPriorities({Make(1, 80, true, inside),Make(2, 80, true, inside)}, 0xAB);
    assert(tied.status == A::TiedPriority && tied.topPriorityTieCount == 2 && tied.leadingRegionFormId == 0);
    const auto unoverridden = AuditWeatherRegionPriorities({Make(1, 80, false, inside),Make(2, 50, true, inside)}, 0xAB);
    assert(unoverridden.status == A::UniqueNonOverridePriorityLeader);
    assert(unoverridden.leadingRegionFormId == 1 && !unoverridden.leadingOverrideFlag);

    // Conservative abstentions: a boundary or missing polygon must NOT be
    // silently ignored, even if there is already a strong inside candidate.
    for (const auto uncertain : {
        S::OnRecordedBoundary, S::IncompleteGeometry, S::MissingGeometry,
        S::MissingPosition, S::MissingWorldspace }) {
        const auto audit = AuditWeatherRegionPriorities({Make(1, 80, true, inside), Make(2, 50, true, uncertain)}, 0xAB);
        assert(audit.status == A::IndeterminateFootprints);
        assert(audit.leadingRegionFormId == 0);
        assert(audit.indeterminateCount == 1);
    }
    const auto mismatch = AuditWeatherRegionPriorities({Make(1, 50, true, S::WorldspaceMismatch)}, 0xAB);
    assert(mismatch.status == A::NoInsideCandidates && mismatch.outsideCount == 1);
    const auto truncated = AuditWeatherRegionPriorities({Make(1, 80, true, inside)}, 0xAB, true);
    assert(truncated.status == A::TruncatedDiscovery && truncated.leadingRegionFormId == 0);

    std::cout << "Weather priority-audit portable tests passed.\n";
}
