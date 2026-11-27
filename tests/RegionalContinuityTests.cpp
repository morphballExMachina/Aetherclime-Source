#include "Aetherclime/RegionalContinuity.h"
#include "Aetherclime/Climate.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace aetherclime;
namespace {
Date When(double hour = 10, int day = 12) { return Date{.month = 9, .day = day, .hour = hour, .year = 201}; }
ClimateRegionContext Outdoors(double x, double y, std::uint32_t space=0x3C) {
    ClimateRegionContext ctx;
    ctx.hasCell = true;
    ctx.exteriorPosition = ClimateWorldPoint{space, x, y};
    return ctx;
}
ClimateRegionResolution Trusted(const char* key = "winterhold") {
    ClimateRegionResolution r;
    r.regionKey = key;
    r.source = ClimateRegionSource::LocationDisplayName;
    return r;
}
ClimateRegionResolution Unknown() {
    ClimateRegionResolution r;
    r.regionKey = "temperate";
    r.source = ClimateRegionSource::FallbackNoEvidence;
    return r;
}
}
int main() {
    RegionalContinuity continuity;
    const auto source = Outdoors(100000, 100000);
    const auto near = Outdoors(106000, 100000);
    const auto far = Outdoors(114000, 100000);
    auto output = continuity.Assess(Unknown(), near, When(), true);
    assert(output.status == ContinuityStatus::NoVerifiedOrigin);
    assert(!output.provisional() && output.resolution.regionKey == "temperate");
    assert(!continuity.ObserveVerified(Unknown(), source, When()));
    assert(continuity.ObserveVerified(Trusted(), source, When()));
    assert(continuity.Assess(Unknown(), near, When(10.5), false).status == ContinuityStatus::Disabled);
    output = continuity.Assess(Unknown(), near, When(10.5), true);
    assert(output.provisional() && output.resolution.provisional());
    assert(!output.resolution.matched()); // NEVER promote the guessed region to weather trust
    assert(output.resolution.regionKey == "winterhold");
    assert(output.distanceRawUnits == 6000.0 && output.ageGameHours == 0.5);
    assert(continuity.Assess(Unknown(), far, When(10.5), true).status == ContinuityStatus::TooFar);
    assert(continuity.Assess(Unknown(), near, When(12.01), true).status == ContinuityStatus::Expired);
    assert(continuity.Assess(Unknown(), near, When(0.01, 13), true).status == ContinuityStatus::DifferentCalendarDay);
    assert(continuity.Assess(Unknown(), Outdoors(106000,100000,999), When(), true).status == ContinuityStatus::DifferentWorldspace);
    assert(continuity.Assess(Unknown(), Outdoors(std::numeric_limits<double>::quiet_NaN(), 0), When(), true).status == ContinuityStatus::NoExteriorPosition);
    auto inside = near; inside.interior = true;
    assert(continuity.Assess(Unknown(), inside, When(), true).status == ContinuityStatus::NoExteriorPosition);
    assert(!continuity.ObserveVerified(Trusted(), inside, When()));
    assert(continuity.Assess(Trusted("rift"), near, When(), true).status == ContinuityStatus::DirectEvidence);
    for (auto conflict : {ClimateRegionSource::FallbackAmbiguous,
                          ClimateRegionSource::FallbackGeographyAmbiguous,
                          ClimateRegionSource::FallbackUnconfigured,
                          ClimateRegionSource::FallbackGeographyUnconfigured}) {
        auto ambiguous = Unknown(); ambiguous.source = conflict;
        assert(continuity.Assess(ambiguous, near, When(), true).status == ContinuityStatus::ExplicitAmbiguity);
    }
    // Repeating Assess() cannot extend an origin by chaining unverified points.
    for (unsigned i = 0; i < 25; ++i) assert(continuity.Assess(Unknown(), near, When(10.5), true).provisional());
    assert(continuity.Assess(Unknown(), far, When(10.5), true).status == ContinuityStatus::TooFar);
    assert(continuity.ObserveVerified(Trusted("pale"), far, When(11)));
    output = continuity.Assess(Unknown(), Outdoors(115000,100000), When(11.5), true);
    assert(output.provisional() && output.resolution.regionKey == "pale");
    continuity.Reset();
    assert(continuity.Assess(Unknown(), near, When(), true).status == ContinuityStatus::NoVerifiedOrigin);
    std::cout << "Alpha-26 regional continuity: all assertions passed\n";
}
