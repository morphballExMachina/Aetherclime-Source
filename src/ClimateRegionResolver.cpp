#include "Aetherclime/ClimateRegionResolver.h"
#include "Aetherclime/RegionAtlas.h"

#include <array>
#include <cmath>
#include <limits>
#include <cctype>
#include <string_view>

namespace aetherclime {
namespace {
struct Alias {
    std::string_view text;
    std::string_view key;
};

// These are routing aliases, not authoritative claims about geographic REGN
// polygons. A city and its named hold intentionally use the same profile.
constexpr std::array<Alias, 18> kAliases{{
    {"Winterhold", "winterhold"}, {"Whiterun", "whiterun"},
    {"Falkreath", "falkreath"}, {"Riften", "rift"},
    {"The Rift", "rift"}, {"Rift", "rift"},
    {"Windhelm", "eastmarch"}, {"Eastmarch", "eastmarch"},
    {"Dawnstar", "pale"}, {"The Pale", "pale"},
    {"Pale", "pale"}, {"Markarth", "reach"},
    {"The Reach", "reach"}, {"Reach", "reach"},
    {"Solitude", "haafingar"}, {"Haafingar", "haafingar"},
    {"Morthal", "hjaalmarch"}, {"Hjaalmarch", "hjaalmarch"}
}};

constexpr char Lower(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EqualsInsensitive(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (Lower(a[i]) != Lower(b[i])) return false;
    }
    return true;
}

bool StartsInsensitive(std::string_view text, std::string_view prefix) noexcept {
    return text.size() >= prefix.size() &&
           EqualsInsensitive(text.substr(0, prefix.size()), prefix);
}

bool LetterOrDigit(char c) noexcept {
    const char x = Lower(c);
    return (x >= 'a' && x <= 'z') || (x >= '0' && x <= '9');
}

bool EditorPrefix(std::string_view before) noexcept {
    for (const auto p : {"Loc", "Location", "Hold", "City", "Tamriel", "Skyrim", "LocType"}) {
        if (EqualsInsensitive(before, p)) return true;
    }
    return false;
}

bool EditorSuffix(std::string_view after) noexcept {
    for (const auto s : {"Location", "Hold", "City", "Worldspace", "Exterior", "Interior", "Marker", "Region"}) {
        if (StartsInsensitive(after, s)) return true;
    }
    return false;
}

// A bounded, explicit alias match prevents ordinary words like
// 'NotWinterhold' or 'WeatherWinterhold' from selecting Winterhold climate.
// Some familiar vanilla editor-ID shapes (WinterholdHoldLocation,
// LocWinterhold) are accepted without treating arbitrary substrings as holds.
bool ContainsAlias(std::string_view label, std::string_view alias, bool editorId) noexcept {
    if (label.empty()) return false;
    for (std::size_t pos = 0; pos + alias.size() <= label.size(); ++pos) {
        if (!EqualsInsensitive(label.substr(pos, alias.size()), alias)) continue;
        const auto before = label.substr(0, pos);
        const auto after = label.substr(pos + alias.size());
        const bool left = pos == 0 || !LetterOrDigit(label[pos - 1]) ||
                          (editorId && EditorPrefix(before));
        const bool right = after.empty() || !LetterOrDigit(after.front()) ||
                           (editorId && EditorSuffix(after));
        if (left && right) return true;
    }
    return false;
}

struct Match {
    std::string_view key{};
    std::string_view alias{};
    bool ambiguous{false};
};

Match FindMatch(std::string_view label, bool editorId) noexcept {
    Match out;
    for (const auto& alias : kAliases) {
        if (!ContainsAlias(label, alias.text, editorId)) continue;
        if (!out.key.empty() && out.key != alias.key) {
            out.ambiguous = true;
        } else if (out.key.empty()) {
            out.key = alias.key;
            out.alias = alias.text;
        }
    }
    return out;
}
} // namespace

std::string_view ClimateRegionSourceName(ClimateRegionSource source) noexcept {
    switch (source) {
    case ClimateRegionSource::LocationDisplayName: return "location-display-name";
    case ClimateRegionSource::LocationEditorId: return "location-editor-id";
    case ClimateRegionSource::GeographicAnchorProximity: return "geographic-sample-proximity";
    case ClimateRegionSource::ProvisionalRecentExterior: return "provisional-recent-exterior";
    case ClimateRegionSource::ProvisionalSurveySample: return "provisional-survey-sample";
    case ClimateRegionSource::ProvisionalAtlasZone: return "provisional-atlas-zone";
    case ClimateRegionSource::FallbackGeographyAmbiguous: return "fallback-geography-overlap-ambiguous";
    case ClimateRegionSource::FallbackGeographyUnconfigured: return "fallback-geography-profile-unconfigured";
    case ClimateRegionSource::FallbackNoEvidence: return "fallback-no-location-evidence";
    case ClimateRegionSource::FallbackUnmatched: return "fallback-no-matching-location";
    case ClimateRegionSource::FallbackAmbiguous: return "fallback-ambiguous-location";
    case ClimateRegionSource::FallbackUnconfigured: return "fallback-profile-unconfigured";
    }
    return "fallback-unknown";
}

ClimateRegionResolution ResolveClimateRegion(
    const Config& config, const ClimateRegionContext& context) {
    ClimateRegionResolution out;
    out.regionKey = config.fallbackRegion;
    out.usedCellLocation = context.usedCellLocation;
    out.interior = context.interior;
    out.source = context.locationChain.empty() ? ClimateRegionSource::FallbackNoEvidence
                                               : ClimateRegionSource::FallbackUnmatched;

    for (std::size_t depth = 0; depth < context.locationChain.size(); ++depth) {
        const auto& loc = context.locationChain[depth];
        for (unsigned field = 0; field < 2; ++field) {
            const auto found = FindMatch(field == 0 ? loc.displayName : loc.editorId, field == 1);
            if (found.key.empty() && !found.ambiguous) continue;
            out.chainDepth = depth;
            out.matchedLocationFormId = loc.formId;
            out.matchedAlias = std::string(found.alias);
            if (found.ambiguous) {
                out.source = ClimateRegionSource::FallbackAmbiguous;
                return out;
            }
            if (!config.regions.contains(std::string(found.key))) {
                out.source = ClimateRegionSource::FallbackUnconfigured;
                return out;
            }
            out.regionKey = std::string(found.key);
            out.source = field == 0 ? ClimateRegionSource::LocationDisplayName
                                    : ClimateRegionSource::LocationEditorId;
            return out;
        }
    }

    // Conservative fallback: only when location ancestry has NO name-based
    // classification, player is outdoors, the position is valid, worldspace
    // matches an explicit sample and proximity stays within a bounded radius.
    if (!config.geographicFallbackEnabled || !context.hasCell || context.interior ||
        !context.exteriorPosition || context.exteriorPosition->worldspaceFormId == 0) {
        return out;
    }
    const auto& point = *context.exteriorPosition;
    if (!std::isfinite(point.xRaw) || !std::isfinite(point.yRaw)) return out;

    const GeographicClimateAnchor* selected = nullptr;
    double selectedDistance = std::numeric_limits<double>::infinity();
    bool conflictingOverlap = false;
    for (const auto& anchor : config.geographicAnchors) {
        if (anchor.worldspaceFormId != point.worldspaceFormId ||
            !std::isfinite(anchor.xRaw) || !std::isfinite(anchor.yRaw) ||
            !std::isfinite(anchor.radiusRawUnits) || anchor.radiusRawUnits <= 0.0) continue;
        const double distance = std::hypot(point.xRaw - anchor.xRaw, point.yRaw - anchor.yRaw);
        if (!std::isfinite(distance) || distance > anchor.radiusRawUnits) continue;
        if (selected && selected->regionKey != anchor.regionKey) conflictingOverlap = true;
        if (!selected || distance < selectedDistance) {
            selected = &anchor;
            selectedDistance = distance;
        }
    }
    if (!selected && config.geographicAtlasEnabled) {
        const auto atlas = EvaluateRegionAtlas(config, context);
        if (atlas.status == AtlasStatus::ConflictingRegions) {
            out.source = ClimateRegionSource::FallbackGeographyAmbiguous;
            return out;
        }
        if (atlas.status == AtlasStatus::UnconfiguredProfile) {
            out.source = ClimateRegionSource::FallbackGeographyUnconfigured;
            return out;
        }
        if (atlas.provisional()) {
            // A separately enabled circle that disagrees with an atlas patch
            // cannot be silently ignored; both are lower-tier observations.
            if (config.geographicSurveyEnabled) {
                for (const auto& sample : config.provisionalSurveySamples) {
                    if (sample.worldspaceFormId != point.worldspaceFormId ||
                        !std::isfinite(sample.xRaw) || !std::isfinite(sample.yRaw) ||
                        !std::isfinite(sample.radiusRawUnits) ||
                        sample.radiusRawUnits <= 0.0) continue;
                    if (std::hypot(point.xRaw-sample.xRaw,
                                   point.yRaw-sample.yRaw) <= sample.radiusRawUnits &&
                        sample.regionKey != atlas.regionKey) {
                        out.source = ClimateRegionSource::FallbackGeographyAmbiguous;
                        return out;
                    }
                }
            }
            out.source = ClimateRegionSource::ProvisionalAtlasZone;
            out.regionKey = atlas.regionKey;
            out.anchorLabel = atlas.zoneLabel;
            out.anchorWorldspaceFormId = atlas.worldspaceFormId;
            // A rectangle has no meaningful radius/distance from a center.
            // Preserve -1 diagnostics instead of inventing a circle.
            return out;
        }
    }
    if (!selected && config.geographicSurveyEnabled) {
        const ProvisionalClimateSample* survey = nullptr;
        double distanceToSurvey = std::numeric_limits<double>::infinity();
        bool surveyOverlapConflict = false;
        for (const auto& item : config.provisionalSurveySamples) {
            if (item.worldspaceFormId != point.worldspaceFormId ||
                !std::isfinite(item.xRaw) || !std::isfinite(item.yRaw) ||
                !std::isfinite(item.radiusRawUnits) || item.radiusRawUnits <= 0.0) continue;
            const double distance = std::hypot(point.xRaw - item.xRaw, point.yRaw - item.yRaw);
            if (!std::isfinite(distance) || distance > item.radiusRawUnits) continue;
            if (survey && survey->regionKey != item.regionKey) surveyOverlapConflict = true;
            if (!survey || distance < distanceToSurvey) {
                survey = &item;
                distanceToSurvey = distance;
            }
        }
        if (!survey) return out;
        // Uncertain conflicting samples must not select a random climate.
        if (surveyOverlapConflict) {
            out.source = ClimateRegionSource::FallbackGeographyAmbiguous;
            return out;
        }
        if (!config.regions.contains(survey->regionKey)) {
            out.source = ClimateRegionSource::FallbackGeographyUnconfigured;
            return out;
        }
        out.source = ClimateRegionSource::ProvisionalSurveySample;
        out.regionKey = survey->regionKey;
        out.anchorLabel = survey->label;
        out.anchorDistanceRawUnits = distanceToSurvey;
        out.anchorRadiusRawUnits = survey->radiusRawUnits;
        out.anchorWorldspaceFormId = survey->worldspaceFormId;
        return out;
    }
    if (!selected) return out;
    if (conflictingOverlap) {
        out.source = ClimateRegionSource::FallbackGeographyAmbiguous;
        return out;
    }
    if (!config.regions.contains(selected->regionKey)) {
        out.source = ClimateRegionSource::FallbackGeographyUnconfigured;
        return out;
    }
    out.source = ClimateRegionSource::GeographicAnchorProximity;
    out.regionKey = selected->regionKey;
    out.anchorLabel = selected->label;
    out.anchorDistanceRawUnits = selectedDistance;
    out.anchorRadiusRawUnits = selected->radiusRawUnits;
    out.anchorWorldspaceFormId = selected->worldspaceFormId;
    return out;
}
} // namespace aetherclime
