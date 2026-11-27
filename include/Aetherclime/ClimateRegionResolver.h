#pragma once

#include "Aetherclime/Climate.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>
#include <string>
#include <string_view>

namespace aetherclime {

// Climate profiles and Skyrim's TESRegion/RE::Sky weather selection remain
// separate concepts. This resolver uses name evidence first, then only a
// bounded reference-neighborhood lookup against exterior worldspace XY. It
// does not read weather pointers, region priority, polygons or elevation.
struct ClimateLocationEvidence {
    std::string_view displayName{};
    std::string_view editorId{};
    std::uint32_t formId{0};
};

// Only worldspace-qualified outdoor coordinates may be routed by geography.
// Source is a bounded, measured reference point, not a hold polygon.
struct ClimateWorldPoint {
    std::uint32_t worldspaceFormId{0};
    double xRaw{0.0};
    double yRaw{0.0};
    // Raw world Z is optional diagnostic evidence. Never interpret it as
    // meters or feed it to temperature lapse calculations without calibration.
    std::optional<double> zRaw{};
};

struct ClimateRegionContext {
    std::span<const ClimateLocationEvidence> locationChain{}; // nearest first
    bool hasCell{false};
    bool interior{false};
    bool usedCellLocation{false}; // if player's location was absent
    std::optional<ClimateWorldPoint> exteriorPosition{};
};

enum class ClimateRegionSource : std::uint8_t {
    LocationDisplayName,
    LocationEditorId,
    GeographicAnchorProximity,
    ProvisionalRecentExterior, // never counts as matched/trusted
    ProvisionalSurveySample,   // static nearby survey, likewise NEVER trusted
    ProvisionalAtlasZone,     // static bounded coverage patch, NEVER trusted
    FallbackGeographyAmbiguous,
    FallbackGeographyUnconfigured,
    FallbackNoEvidence,
    FallbackUnmatched,
    FallbackAmbiguous,
    FallbackUnconfigured
};

[[nodiscard]] std::string_view ClimateRegionSourceName(ClimateRegionSource source) noexcept;

struct ClimateRegionResolution {
    std::string regionKey{}; // existing configured climate profile key
    ClimateRegionSource source{ClimateRegionSource::FallbackNoEvidence};
    std::size_t chainDepth{0}; // 0 = nearest, only meaningful when matched
    std::uint32_t matchedLocationFormId{0};
    std::string matchedAlias{}; // copied, not a pointer into a game record
    bool usedCellLocation{false};
    bool interior{false}; // indoor temperature remains outdoor baseline
    std::string anchorLabel{}; // copied from config, empty unless geographic evidence
    double anchorDistanceRawUnits{-1.0};
    double anchorRadiusRawUnits{-1.0};
    std::uint32_t anchorWorldspaceFormId{0};
    bool locationMatched() const noexcept {
        return source == ClimateRegionSource::LocationDisplayName ||
               source == ClimateRegionSource::LocationEditorId;
    }
    bool provisional() const noexcept {
        return source == ClimateRegionSource::ProvisionalRecentExterior ||
               source == ClimateRegionSource::ProvisionalSurveySample ||
               source == ClimateRegionSource::ProvisionalAtlasZone;
    }
    bool matched() const noexcept {
        return locationMatched() || source == ClimateRegionSource::GeographicAnchorProximity;
    }
};

// Named location evidence wins. Only otherwise, for outdoor Tamriel-like
// worldspace-matching coordinates, try user-configured measured anchor areas.
// No Sky pointer, weather-region selection, or elevation inference is used.
// Geography is a proximity estimate, NOT a geographic-containment guarantee.
// Ambiguous/unconfigured *named* evidence never silently falls through.
[[nodiscard]] ClimateRegionResolution ResolveClimateRegion(
    const Config& config, const ClimateRegionContext& context);

} // namespace aetherclime
