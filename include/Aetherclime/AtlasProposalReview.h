#pragma once

#include "Aetherclime/GeographicGapReport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aetherclime {
// Alpha-36: local, manually reviewed experimental rectangles. Neither the
// geometry nor a nearby named location proves the fallback endpoint's region.
enum class AtlasProposalStatus : std::uint8_t {
    Draft, IneligibleHandoff, WeakSupport, UnsafeFallback, BracketTooWide,
    InvalidGeometry, Capacity, ExistingCoverage, ConflictingAtlas,
    ConflictingSurvey, ConflictingAnchor
};
[[nodiscard]] std::string_view AtlasProposalStatusName(AtlasProposalStatus) noexcept;
struct AtlasProposal {
    AtlasProposalStatus status{AtlasProposalStatus::IneligibleHandoff};
    ProvisionalAtlasZone zone{};
    GeographicGapCandidate observation{};
    double fallbackXRaw{0}, fallbackYRaw{0};
    bool fallbackWasPrevious{false};
    std::uint64_t occurrences{1};
};
// No runtime resolver mutation. Validate against even disabled configured layers.
[[nodiscard]] AtlasProposalStatus ValidateAtlasProposal(const Config&, const ProvisionalAtlasZone&,
                                                       double fallbackXRaw, double fallbackYRaw) noexcept;
[[nodiscard]] AtlasProposal MakeAtlasProposal(const Config&, const GeographicGapCandidate&);

struct AtlasRegionalIssue {
    std::uint32_t worldspaceFormId{0};
    std::string region;
    std::uint64_t entries{0}, exits{0}, significant{0}, originTooFar{0}, drafts{0};
    double peakAbsDeltaC{0};
};
struct AtlasUseCount {
    std::uint32_t worldspaceFormId{0};
    std::string region, label;
    std::uint64_t observations{0};
};
struct AtlasProposalSummary {
    std::uint64_t fallbackHandoffs{0}, eligibleDrafts{0}, weakSupport{0}, unsafeFallback{0},
        wideBrackets{0}, invalidOrIneligible{0}, conflicts{0}, existingCoverage{0},
        capacityRejected{0}, mergedDrafts{0}, droppedDrafts{0}, droppedRegionalEvents{0};
    std::uint64_t effectiveAtlasObservations{0}, untrackedAtlasObservations{0};
    std::size_t draftCount{0}, regionCount{0}, atlasUseCount{0};
};
class AtlasProposalReview {
public:
    static constexpr std::size_t kMaxDrafts = 8;
    static constexpr std::size_t kMaxRegions = 16;
    static constexpr std::size_t kMaxAtlasUse = 32;
    static constexpr double kMaxBracketRawUnits = 4608.0;
    static constexpr double kPaddingRawUnits = 256.0;
    void Reset() noexcept { summary_ = {}; drafts_ = {}; regions_ = {}; atlasUse_ = {}; }
    void Observe(const Config&, const GeographicGapCandidate&);
    void RecordAtlasUse(const ClimateRegionResolution&);
    [[nodiscard]] const AtlasProposalSummary& Summary() const noexcept { return summary_; }
    [[nodiscard]] std::array<AtlasProposal,kMaxDrafts> RankedDrafts() const;
    [[nodiscard]] std::array<AtlasRegionalIssue,kMaxRegions> RankedRegions() const;
    [[nodiscard]] std::array<AtlasUseCount,kMaxAtlasUse> RankedAtlasUse() const;
private:
    AtlasProposalSummary summary_{};
    std::array<AtlasProposal,kMaxDrafts> drafts_{};
    std::array<AtlasRegionalIssue,kMaxRegions> regions_{};
    std::array<AtlasUseCount,kMaxAtlasUse> atlasUse_{};
};
} // namespace aetherclime
