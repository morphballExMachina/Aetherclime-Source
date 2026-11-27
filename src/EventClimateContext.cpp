#include "Aetherclime/EventClimateContext.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>

namespace aetherclime {
namespace {
bool ValidDate(const Date& date) {
    return date.month >= 0 && date.month < 12 && date.day >= 1 && date.day <= 31 &&
        std::isfinite(date.hour) && date.hour >= 0.0 && date.hour < 24.0;
}
std::string Lower(std::string_view value) {
    std::string output;
    output.reserve(value.size());
    for (char c : value) output.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return output;
}
bool HasWord(const std::string& haystack, std::string_view word) {
    const std::string needle(word);
    auto i = haystack.find(needle);
    while (i != std::string::npos) {
        const auto wordChar = [](char c){ return std::isalnum(static_cast<unsigned char>(c)) != 0; };
        const bool left = i == 0 || !wordChar(haystack[i - 1]);
        const auto after = i + needle.size();
        const bool right = after == haystack.size() || !wordChar(haystack[after]);
        if (left && right) return true;
        i = haystack.find(needle, i + 1);
    }
    return false;
}
struct NameGuess {
    BuildingKind kind{BuildingKind::UnknownInterior};
    BuildingEvidence evidence{BuildingEvidence::None};
};
NameGuess Guess(std::string_view name) {
    const auto s = Lower(name);
    if (s.empty()) return {};
    // Curated vanilla examples take precedence over weak words. Whiterun
    // field surveys in Alpha-23 exposed several names with no obvious hint.
    // These are read-only building labels, not claims of heating or shelter.
    for (const auto* known : {"the frozen hearth", "dead man's drink", "the bannered mare",
          "the bee and barb", "the winking skeever", "candlehearth hall", "silver-blood inn"}) {
        if (s == known) return {BuildingKind::Inn, BuildingEvidence::KnownName};
    }
    for (const auto* known : {"blue palace", "palace of the kings", "dragonsreach",
          "mistveil keep", "understone keep"}) {
        if (s == known) return {BuildingKind::PalaceOrCourt, BuildingEvidence::KnownName};
    }
    for (const auto* known : {"breezehome", "honeyside", "hjerim", "proudspire manor",
          "vlindrel hall", "severin manor", "black-briar manor"}) {
        if (s == known) return {BuildingKind::Home, BuildingEvidence::KnownName};
    }
    for (const auto* known : {"belethor's general goods", "warmaiden's", "the pawned prawn",
          "arcadia's cauldron", "the drunken huntsman", "radiant raiment",
          "angeline's aromatics"}) {
        // The Drunken Huntsman is mixed use, classified here as a merchant.
        if (s == known) return {BuildingKind::Shop, BuildingEvidence::KnownName};
    }
    for (const auto* known : {"jorrvaskr", "jorrvaskr living quarters"}) {
        if (s == known) return {BuildingKind::Hall, BuildingEvidence::KnownName};
    }
    for (const auto* known : {"temple of kynareth", "temple of mara", "temple of the divines",
          "temple of dibella", "temple of talos"}) {
        if (s == known) return {BuildingKind::TempleOrShrine, BuildingEvidence::KnownName};
    }
    for (const auto* known : {"hall of the dead", "whiterun hall of the dead"}) {
        if (s == known) return {BuildingKind::Funerary, BuildingEvidence::KnownName};
    }
    // Weak hints must be delimited words, not substrings (e.g. 'inn' cannot
    // match 'Winterhold' or 'inner'). They never imply a temperature.
    if (HasWord(s, "inn") || HasWord(s, "tavern")) return {BuildingKind::Inn, BuildingEvidence::NameHint};
    if (HasWord(s, "palace")) return {BuildingKind::PalaceOrCourt, BuildingEvidence::NameHint};
    if (HasWord(s, "house") || HasWord(s, "home") || HasWord(s, "manor"))
        return {BuildingKind::Home, BuildingEvidence::NameHint};
    if (HasWord(s, "shop") || HasWord(s, "goods") || HasWord(s, "store"))
        return {BuildingKind::Shop, BuildingEvidence::NameHint};
    // Strong compound phrases must win over their weaker component words.
    if (s.find("hall of the dead") != std::string::npos || HasWord(s, "mausoleum") ||
        HasWord(s, "ossuary")) return {BuildingKind::Funerary, BuildingEvidence::NameHint};
    if (HasWord(s, "temple") || HasWord(s, "chapel") || HasWord(s, "shrine"))
        return {BuildingKind::TempleOrShrine, BuildingEvidence::NameHint};
    if (HasWord(s, "hall") || HasWord(s, "longhouse")) return {BuildingKind::Hall, BuildingEvidence::NameHint};
    if (HasWord(s, "mine") || HasWord(s, "cave") || HasWord(s, "cavern"))
        return {BuildingKind::CaveOrMine, BuildingEvidence::NameHint};
    if (HasWord(s, "ruins") || HasWord(s, "barrow") || HasWord(s, "crypt") || HasWord(s, "tomb"))
        return {BuildingKind::RuinOrCrypt, BuildingEvidence::NameHint};
    return {};
}
} // namespace

std::string_view ClimateEventGateStatusName(ClimateEventGateStatus s) noexcept {
    switch (s) {
    case ClimateEventGateStatus::Disabled: return "disabled";
    case ClimateEventGateStatus::MissingCell: return "cell-unavailable";
    case ClimateEventGateStatus::UntrustedRegion: return "untrusted-region";
    case ClimateEventGateStatus::Duplicate: return "duplicate-suppressed";
    case ClimateEventGateStatus::Accept: return "accepted";
    }
    return "unknown";
}
void ClimateEventGate::Reset() noexcept { last_.reset(); }
ClimateEventGateStatus ClimateEventGate::Evaluate(const ClimateEventFrame& frame) {
    if (!frame.enabled) return ClimateEventGateStatus::Disabled;
    if (!frame.hasCell || frame.cellFormId == 0) return ClimateEventGateStatus::MissingCell;
    if (!frame.regionTrusted || frame.regionKey.empty()) return ClimateEventGateStatus::UntrustedRegion;
    if (last_) {
        const auto& old = *last_;
        const bool sameCell = old.cellFormId == frame.cellFormId && old.regionKey == frame.regionKey &&
            old.interior == frame.interior && old.skyWeatherFormId == frame.skyWeatherFormId;
        const bool comparableClock = ValidDate(old.observedAt) && ValidDate(frame.observedAt) &&
            old.observedAt.year == frame.observedAt.year && old.observedAt.month == frame.observedAt.month &&
            old.observedAt.day == frame.observedAt.day && frame.observedAt.hour >= old.observedAt.hour;
        if (sameCell && comparableClock &&
            frame.observedAt.hour - old.observedAt.hour < kSameCellMinRefreshGameHours) {
            return ClimateEventGateStatus::Duplicate;
        }
    }
    last_ = frame;
    return ClimateEventGateStatus::Accept;
}
DeferredClimateStatus EvaluateDeferredClimate(const DeferredClimateFrame& frame) noexcept {
    if (!frame.sessionActive) return DeferredClimateStatus::Inactive;
    const bool currentCellReady = frame.hasPlayer && frame.currentCellId != 0;
    const bool targetMatches = frame.target == DeferredClimateTarget::Cell
        ? frame.currentCellId == frame.expectedId
        : frame.currentLocationId == frame.expectedId;
    if (currentCellReady && targetMatches) return DeferredClimateStatus::Ready;
    return frame.checkNumber < kMaxDeferredClimateChecks
        ? DeferredClimateStatus::Retry : DeferredClimateStatus::Exhausted;
}
std::string_view DeferredClimateStatusName(DeferredClimateStatus status) noexcept {
    switch (status) {
    case DeferredClimateStatus::Ready: return "deferred-ready";
    case DeferredClimateStatus::Retry: return "deferred-not-yet-current";
    case DeferredClimateStatus::Exhausted: return "deferred-retry-budget-exhausted";
    case DeferredClimateStatus::Inactive: return "deferred-session-inactive";
    }
    return "unknown";
}
std::string_view BuildingKindName(BuildingKind kind) noexcept {
    switch (kind) {
    case BuildingKind::Exterior: return "exterior";
    case BuildingKind::UnknownInterior: return "unknown-interior";
    case BuildingKind::Inn: return "inn-or-tavern";
    case BuildingKind::PalaceOrCourt: return "palace-or-court";
    case BuildingKind::Home: return "home-or-dwelling";
    case BuildingKind::Shop: return "shop-or-business";
    case BuildingKind::Hall: return "hall-or-longhouse";
    case BuildingKind::CaveOrMine: return "cave-or-mine";
    case BuildingKind::RuinOrCrypt: return "ruin-or-crypt";
    case BuildingKind::TempleOrShrine: return "temple-or-shrine";
    case BuildingKind::Funerary: return "funerary-or-tomb";
    case BuildingKind::Ambiguous: return "ambiguous-evidence";
    }
    return "unknown";
}
std::string_view BuildingEvidenceName(BuildingEvidence evidence) noexcept {
    switch (evidence) {
    case BuildingEvidence::Exterior: return "exterior";
    case BuildingEvidence::None: return "none";
    case BuildingEvidence::ConfiguredInn: return "explicit-heated-inn-rule";
    case BuildingEvidence::KnownName: return "known-vanilla-name";
    case BuildingEvidence::NameHint: return "name-hint-only";
    case BuildingEvidence::Conflict: return "conflicting-cell-and-location-names";
    }
    return "unknown";
}
std::string_view BuildingEvidenceConfidence(BuildingEvidence evidence) noexcept {
    switch (evidence) {
    case BuildingEvidence::Exterior: return "direct-cell";
    case BuildingEvidence::ConfiguredInn: return "explicit-config";
    case BuildingEvidence::KnownName: return "curated-name";
    case BuildingEvidence::NameHint: return "weak-name-hint";
    case BuildingEvidence::Conflict: return "conflicting";
    case BuildingEvidence::None: return "unknown";
    }
    return "unknown";
}

BuildingContext ClassifyBuilding(bool hasCell, bool interior,
    std::string_view cellDisplayName, std::string_view locationDisplayName,
    bool explicitConfiguredHeatedInn) {
    if (!hasCell) return {};
    if (!interior) return {BuildingKind::Exterior, BuildingEvidence::Exterior, false, false};
    if (explicitConfiguredHeatedInn) return {BuildingKind::Inn, BuildingEvidence::ConfiguredInn, false, true};
    const auto cell = Guess(cellDisplayName);
    const auto location = Guess(locationDisplayName);
    if (cell.kind != BuildingKind::UnknownInterior && location.kind != BuildingKind::UnknownInterior &&
        cell.kind != location.kind) {
        return {BuildingKind::Ambiguous, BuildingEvidence::Conflict, false, false};
    }
    if (cell.kind != BuildingKind::UnknownInterior &&
        (location.kind == BuildingKind::UnknownInterior || cell.evidence == BuildingEvidence::KnownName ||
         location.evidence != BuildingEvidence::KnownName)) {
        return {cell.kind, cell.evidence, false, false};
    }
    if (location.kind != BuildingKind::UnknownInterior) return {location.kind, location.evidence, false, false};
    return {};
}
std::string_view OutdoorChangeKindName(OutdoorChangeKind kind) noexcept {
    switch (kind) {
    case OutdoorChangeKind::NotExteriorVerified: return "not-verified-exterior";
    case OutdoorChangeKind::Baseline: return "first-verified-baseline";
    case OutdoorChangeKind::Unchanged: return "same-weather-and-region";
    case OutdoorChangeKind::WeatherChanged: return "weather-changed";
    case OutdoorChangeKind::RegionChanged: return "region-changed";
    case OutdoorChangeKind::BothChanged: return "weather-and-region-changed";
    }
    return "unknown";
}
bool IsMeaningfulOutdoorChange(OutdoorChangeKind kind) noexcept {
    return kind == OutdoorChangeKind::Baseline || kind == OutdoorChangeKind::WeatherChanged ||
           kind == OutdoorChangeKind::RegionChanged || kind == OutdoorChangeKind::BothChanged;
}
void OutdoorChangeGate::Reset() noexcept { last_.reset(); }
OutdoorChangeKind OutdoorChangeGate::Observe(const OutdoorChangeSample& sample) {
    // Failed evidence and interior transitions must NOT overwrite the last
    // verified outdoor state. Later exterior weather can still compare to it.
    if (!sample.exterior || !sample.trustedRegion || sample.regionKey.empty() ||
        sample.weatherFormId == 0) return OutdoorChangeKind::NotExteriorVerified;
    if (!last_) {
        last_ = sample;
        return OutdoorChangeKind::Baseline;
    }
    const bool regionChanged = last_->regionKey != sample.regionKey;
    const bool weatherChanged = last_->weatherFormId != sample.weatherFormId;
    if (!regionChanged && !weatherChanged) return OutdoorChangeKind::Unchanged;
    last_ = sample;
    if (regionChanged && weatherChanged) return OutdoorChangeKind::BothChanged;
    return regionChanged ? OutdoorChangeKind::RegionChanged : OutdoorChangeKind::WeatherChanged;
}

} // namespace aetherclime
