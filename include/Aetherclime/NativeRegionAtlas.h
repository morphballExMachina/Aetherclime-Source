#pragma once
#include "Aetherclime/ClimateRegionResolver.h"
#include "Aetherclime/RegionGeometry.h"
#include "Aetherclime/NativeBiomeRegistry.h"
#include <iosfwd>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aetherclime {
// Copied REGN geometry; no engine pointers. Mapping is an explicit hypothesis,
// not a weather selection rule, hold boundary, or source of trust.
struct NativeRegionRecord {
    std::uint32_t formId{}, worldspace{};
    std::string profile; // empty = unmapped weather/region footprint
    bool incomplete{};
    std::vector<RegionPolygon> polygons;
    std::string editorId, biome;
    NativeBiomeRole biomeRole{NativeBiomeRole::Unclassified};
};
enum class NativeAtlasStatus { Unavailable, NoExteriorPosition, NoCoverage,
    UnmappedCoverage, IncompleteMappedGeometry, Boundary, ConflictingProfiles, UniqueProfile };
const char* NativeAtlasStatusName(NativeAtlasStatus status) noexcept;
enum class NativeBiomeStatus { Unavailable, NoPosition, NoCoverage, Incomplete, Boundary, Conflict, Unique };
const char* NativeBiomeStatusName(NativeBiomeStatus status) noexcept;
struct BiomeSpatialEvidence {
    std::string biome;
    double edgeDistanceRaw{-1}; // nearest contour when outside; deepest containing contour otherwise
    bool inside{}, boundary{};
};
struct NativeAtlasResult {
    NativeAtlasStatus status{NativeAtlasStatus::Unavailable};
    std::string profile;
    std::size_t mappedContaining{}, unmappedContaining{}, incompleteUnmapped{}, boundaryUnmapped{};
    std::array<std::uint32_t, 8> unmappedIds{};
    NativeBiomeStatus biomeStatus{NativeBiomeStatus::Unavailable};
    std::string biome;
    std::size_t biomeContaining{}, localOverrideContaining{};
    std::vector<BiomeSpatialEvidence> biomeEvidence;
};
struct NativeAtlasStats { std::size_t records{}, mapped{}, polygons{}, vertices{}, incomplete{}; bool complete{}; std::size_t quarantined{}, biomeRecords{}; };
class NativeRegionAtlas {
public:
    static constexpr std::size_t maxRecords=512, maxPolygons=4096, maxVertices=262144, maxPolygonVertices=8192;
    // If the engine enumeration is truncated, reject ALL queries.
    void Build(std::vector<NativeRegionRecord> records, bool complete);
    NativeAtlasResult Query(const std::optional<ClimateWorldPoint>& position, bool measureBiomeEdges=false) const;
    // Versioned, locale-independent CSV of validated cached geometry, including quarantined metadata.
    bool ExportGeometry(std::ostream& out) const;
    const NativeAtlasStats& Stats() const noexcept { return stats_; }
private:
    struct Polygon { RegionPolygon points; double minX{},maxX{},minY{},maxY{}; };
    struct Record { std::uint32_t id{},worldspace{}; std::string profile; bool incomplete{}; std::vector<Polygon> polygons; std::string editorId,biome; NativeBiomeRole biomeRole; };
    std::vector<Record> records_;
    NativeAtlasStats stats_;
};
// Proposed hierarchy ONLY: direct/anchor > unique polygon > existing result.
// Ambiguous named/anchor evidence is not overridden. No mutation of resolution.
struct NativeShadowDecision { std::string profile; const char* reason; bool wouldReplaceFallback{}; };
NativeShadowDecision CompareNativeShadow(const ClimateRegionResolution& direct,
    const ClimateRegionResolution& effective, const NativeAtlasResult& atlas);
struct NativeShadowCoverage {
    std::size_t samples{}, noPosition{}, unavailable{}, unique{}, unmapped{}, conflicts{}, boundary{}, incomplete{}, noCoverage{};
    std::size_t fallbackWouldReplace{}, sameProfile{}, differentProfile{}, directPreserved{};
    void Observe(const NativeAtlasResult&, const NativeShadowDecision&, const ClimateRegionResolution&);
};
}
