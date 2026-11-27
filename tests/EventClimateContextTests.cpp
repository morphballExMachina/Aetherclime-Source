#include "Aetherclime/EventClimateContext.h"
#include <cassert>
#include <iostream>
using namespace aetherclime;
namespace {
Date Time(double hour, int day = 17) {return Date{.month=7,.day=day,.hour=hour,.year=201};}
ClimateEventFrame Frame(bool interior, std::uint32_t cell, std::string region, double hour, std::uint32_t sky=0x123) {
    return {true, true, interior, cell, std::move(region), true, sky, Time(hour)};
}
void AssertKind(std::string_view cell, std::string_view loc, BuildingKind kind) {
    const auto classification = ClassifyBuilding(true,true,cell,loc,false);
    assert(classification.kind == kind);
    assert(!classification.heatingAssumed);
    assert(!classification.knownHeatedInnRule);
}
}
int main() {
    assert(!DefaultConfig().eventClimateEnabled);
    ClimateEventGate gate;
    auto first = Frame(false, 101, "winterhold", 8.0);
    assert(gate.Evaluate(first)==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(first)==ClimateEventGateStatus::Duplicate);
    assert(gate.Evaluate(Frame(false,101,"winterhold",8.24))==ClimateEventGateStatus::Duplicate);
    assert(gate.Evaluate(Frame(false,101,"winterhold",8.25))==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(Frame(false,102,"winterhold",8.26))==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(Frame(true,201,"winterhold",8.27))==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(Frame(true,201,"falkreath",8.28))==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(Frame(false,101,"falkreath",8.29))==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(Frame(false,101,"falkreath",8.30,0x456))==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(Frame(false,101,"falkreath",8.31,0x456))==ClimateEventGateStatus::Duplicate);
    assert(gate.Evaluate(Frame(false,101,"falkreath",7.0,0x456))==ClimateEventGateStatus::Accept); // time reversal cannot suppress
    auto disabled=Frame(false,101,"falkreath",7.1); disabled.enabled=false;
    assert(gate.Evaluate(disabled)==ClimateEventGateStatus::Disabled);
    auto untrusted=Frame(false,101,"falkreath",7.1); untrusted.regionTrusted=false;
    assert(gate.Evaluate(untrusted)==ClimateEventGateStatus::UntrustedRegion);
    auto noCell=Frame(false,0,"falkreath",7.1);
    assert(gate.Evaluate(noCell)==ClimateEventGateStatus::MissingCell);
    gate.Reset();
    assert(gate.Evaluate(first)==ClimateEventGateStatus::Accept);
    assert(gate.Evaluate(first)==ClimateEventGateStatus::Duplicate);
    assert(gate.Evaluate(Frame(false,101,"winterhold",8.10,0x123))==ClimateEventGateStatus::Duplicate);
    assert(gate.Evaluate(Frame(false,101,"winterhold",8.10,0x123))==ClimateEventGateStatus::Duplicate);
    auto tomorrow=Frame(false,101,"winterhold",8.10); tomorrow.observedAt.day=18;
    assert(gate.Evaluate(tomorrow)==ClimateEventGateStatus::Accept);
    auto unknownClock=Frame(false,101,"winterhold",26.0);
    assert(gate.Evaluate(unknownClock)==ClimateEventGateStatus::Accept);

    // Deferred native event handoff: early event, settled state, bounded
    // retries, stale session, and null locations all have explicit outcomes.
    DeferredClimateFrame deferred{};
    deferred.sessionActive = true;
    deferred.hasPlayer = true;
    deferred.target = DeferredClimateTarget::Cell;
    deferred.expectedId = 0x1A274;
    deferred.currentCellId = 0; // exactly what the Whiterun runtime log showed
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Retry);
    deferred.checkNumber = kMaxDeferredClimateChecks;
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Exhausted);
    deferred.currentCellId = 0x1A273;
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Exhausted);
    deferred.checkNumber = 2;
    deferred.currentCellId = 0x1A274; // world state settled after scheduling
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Ready);
    deferred.sessionActive = false;
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Inactive);
    deferred.sessionActive = true;
    deferred.target = DeferredClimateTarget::Location;
    deferred.expectedId = 0x12345;
    deferred.currentCellId = 0x1A274;
    deferred.currentLocationId = 0;
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Retry);
    deferred.currentLocationId = 0x12345;
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Ready);
    deferred.expectedId = 0; // leaving a named location is valid if cell settled
    deferred.currentLocationId = 0;
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Ready);
    deferred.hasPlayer = false;
    assert(EvaluateDeferredClimate(deferred) == DeferredClimateStatus::Retry);
    assert(DeferredClimateStatusName(DeferredClimateStatus::Ready) == "deferred-ready");

    AssertKind("The Frozen Hearth","The Frozen Hearth",BuildingKind::Inn);
    AssertKind("Dead Man's Drink","Dead Man's Drink",BuildingKind::Inn);
    AssertKind("Blue Palace","Solitude",BuildingKind::PalaceOrCourt);
    AssertKind("Dragonsreach","Whiterun",BuildingKind::PalaceOrCourt);
    AssertKind("Breezehome","Whiterun",BuildingKind::Home);
    AssertKind("Belethor's General Goods","Whiterun",BuildingKind::Shop);
    AssertKind("Jorrvaskr","Jorrvaskr",BuildingKind::Hall);
    AssertKind("Embershard Mine","Embershard",BuildingKind::CaveOrMine);
    AssertKind("Bleak Falls Barrow","Bleak Falls Barrow",BuildingKind::RuinOrCrypt);
    AssertKind("Temple of Kynareth","Temple of Kynareth",BuildingKind::TempleOrShrine);
    AssertKind("Temple of Mara","Riften",BuildingKind::TempleOrShrine);
    AssertKind("Hall of the Dead","Hall of the Dead",BuildingKind::Funerary);
    AssertKind("Arcadia's Cauldron","Whiterun",BuildingKind::Shop);
    AssertKind("The Drunken Huntsman","Whiterun",BuildingKind::Shop);
    AssertKind("Angeline's Aromatics","Solitude",BuildingKind::Shop);
    AssertKind("Radiant Raiment","Solitude",BuildingKind::Shop);
    AssertKind("Temple of the Divines","Solitude",BuildingKind::TempleOrShrine);
    AssertKind("Blue Palace","Solitude",BuildingKind::PalaceOrCourt);
    AssertKind("The Winking Skeever","Solitude",BuildingKind::Inn);
    AssertKind("Mistveil Keep","Riften",BuildingKind::PalaceOrCourt);
    AssertKind("The Bee and Barb","Riften",BuildingKind::Inn);
    AssertKind("Mausoleum of the Forgotten","Unknown",BuildingKind::Funerary);
    AssertKind("The Silent Chapel","Unknown",BuildingKind::TempleOrShrine);
    assert(BuildingEvidenceConfidence(BuildingEvidence::KnownName) == "curated-name");
    assert(BuildingEvidenceConfidence(BuildingEvidence::NameHint) == "weak-name-hint");
    assert(BuildingEvidenceConfidence(BuildingEvidence::None) == "unknown");
    AssertKind("Unknown Tower","Winterhold",BuildingKind::UnknownInterior);
    AssertKind("My Cozy Inn","Solitude",BuildingKind::Inn);
    AssertKind("Old House","Old Mine",BuildingKind::Ambiguous);
    assert(ClassifyBuilding(true,false,"Blue Palace","Solitude",false).kind==BuildingKind::Exterior);
    assert(ClassifyBuilding(false,true,"Blue Palace","Solitude",false).kind==BuildingKind::UnknownInterior);
    const auto explicitInn=ClassifyBuilding(true,true,"Random modded interior","Unknown",true);
    assert(explicitInn.kind==BuildingKind::Inn && explicitInn.knownHeatedInnRule);
    assert(!explicitInn.heatingAssumed); // separate actual ambient model determines availability
    std::cout << "EventClimateContext: all assertions passed\n";
}
