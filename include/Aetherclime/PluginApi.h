#pragma once
// Third-party SKSE native C ABI. Calls are synchronous and MUST be made on
// Skyrim's game/main thread. Load exports dynamically; an older Aetherclime
// DLL may not export the Alpha-25 preview snapshot function.
// Original API version 1 exports retain their old meanings and signatures.
#include <cstdint>

#ifdef _WIN32
  #if defined(AETHERCLIME_BUILDING_DLL)
    #define AETHERCLIME_EXPORT __declspec(dllexport)
  #else
    #define AETHERCLIME_EXPORT __declspec(dllimport)
  #endif
#else
  #define AETHERCLIME_EXPORT
#endif

// Exactly-sized ABI for Alpha-25's read-only snapshot (schema version 1).
// Contains no owning pointers, std::string, bool, or STL types. Strings are
// null-terminated UTF-8, truncated if needed. Fields are sampled ON REQUEST.
// Only trust localAmbientC if LOCAL_AMBIENT_AVAILABLE is set; indoor values
// are currently MODELED estimates for explicitly configured heated inns.
// The old Aetherclime_GetAmbientC export is NOT the local ambient field.
enum AetherclimeEnvironmentFlagsV1 : std::uint32_t {
    AETHERCLIME_ENV_INTERIOR = 1u << 0,
    AETHERCLIME_ENV_TRUSTED_REGION = 1u << 1,
    AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER = 1u << 2,
    AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE = 1u << 3,
    AETHERCLIME_ENV_LOCAL_AMBIENT_MODELED_INDOOR = 1u << 4,
    AETHERCLIME_ENV_EXACT_INTERIOR_CELL_NAME = 1u << 5,
    // Additive Alpha-26 flags; V1 struct size and layout are unchanged.
    AETHERCLIME_ENV_PROVISIONAL_REGION = 1u << 6,
    AETHERCLIME_ENV_OUTDOOR_AIR_UNVERIFIED = 1u << 7
};
struct AetherclimeEnvironmentSnapshotV1 {
    std::uint32_t structSize;              // sizeof(AetherclimeEnvironmentSnapshotV1)
    std::uint32_t schemaVersion;           // 1
    std::uint32_t flags;                   // AetherclimeEnvironmentFlagsV1 bits
    std::uint32_t cellFormId;
    std::uint32_t namedLocationFormId;
    std::uint32_t verifiedOutdoorWeatherFormId; // zero when unverified
    std::uint32_t buildingKind;            // see docs/ENVIRONMENT_CONTEXT_API_ALPHA25.md
    std::uint32_t localNameSource;
    std::uint32_t localNameCertainty;
    std::uint32_t weatherProvenance;
    std::uint32_t weatherKind;
    std::int32_t year;
    std::int32_t month;                    // 0=Morning Star ... 11=Evening Star
    std::int32_t day;
    double hour;                           // [0,24)
    double outdoorAirC;                    // model baseline; only trust when TRUSTED_REGION flag set
    double outdoorFeelsLikeC;              // outdoor model estimate
    double localAmbientC;                  // only valid when flag is set
    char localName[128];
    char regionKey[48];
    char regionDisplayName[96];
};

// Alpha-42 additive ABI. The nested legacy record is exactly SnapshotV1.
// Test the flags before reading optional fields; zero is not an availability flag.
enum AetherclimeEnvironmentFlagsV2 : std::uint32_t {
    AETHERCLIME_ENV2_SEASON_AVAILABLE = 1u << 0,
    AETHERCLIME_ENV2_BIOME_AVAILABLE = 1u << 1,
    AETHERCLIME_ENV2_BIOME_NEARBY = 1u << 2,
    AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE = 1u << 3
};
struct AetherclimeEnvironmentSnapshotV2 {
    std::uint32_t structSize;
    std::uint32_t schemaVersion; // 2
    AetherclimeEnvironmentSnapshotV1 legacy;
    std::uint32_t flags; // AetherclimeEnvironmentFlagsV2
    std::uint32_t officialSeason; // 0 winter, 1 spring, 2 summer, 3 autumn
    std::uint32_t biomeEvidence; // 0 none, 1 unique, 2 overlap, 3 overlap-edge, 4 nearby
    std::uint32_t reserved; // zero
    double seasonWeights[4]; // winter, spring, summer, autumn; sum 1 when available
    double biomeWeightA;
    double biomeWeightB;
    double previewAirC; // experimental biome model; never replaces legacy.outdoorAirC
    char biomeA[48];
    char biomeB[48];
    char biomeStatus[64]; // reason/status, even when identity unavailable
};

// Alpha-49 opt-in additive contract: temperature estimates are available even
// when no trusted named region is resolved. V1/V2 remain bit-for-bit unchanged.
// Only read temperature values if the corresponding flag is set. In particular
// an estimate is NOT a verified weather measurement or a proven hold identity.
enum AetherclimeEnvironmentFlagsV3 : std::uint32_t {
    AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE = 1u << 0,
    AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE = 1u << 1
};
// outdoorEstimateBasis: 0 unavailable,1 trusted regional,2 provisional regional,
// 3 native biome,4 nearby biome,5 broad Tamriel,6 generic worldspace,
// 7 nearby surveyed climate reference (low-confidence, not a hold boundary).
// indoorEstimateBasis: 0 unavailable,1 explicit inn,2 building class,3 generic.
struct AetherclimeEnvironmentSnapshotV3 {
    std::uint32_t structSize;
    std::uint32_t schemaVersion; // 3
    AetherclimeEnvironmentSnapshotV2 legacy;
    std::uint32_t flags;
    std::uint32_t outdoorEstimateBasis;
    std::uint32_t indoorEstimateBasis;
    std::uint32_t reserved; // zero
    double outdoorEstimateC;
    double indoorEstimateC;
};

// Alpha-41 additive optional export. Resolve with GetProcAddress; ApiVersion()
// stays 1 for legacy compatibility. No changes to EnvironmentSnapshotV1.
// status: 0=false, 1=true, 2=unknown/unavailable, 3=invalid rule/internal error.
// error: 0=none, 1=empty, 2=too long, 3=unexpected token, 4=unknown field,
// 5=invalid literal, 6=invalid operator, 7=resource limit, 8=internal error.
struct AetherclimeConditionResultV1 {
    std::uint32_t structSize;
    std::uint32_t schemaVersion;
    std::uint32_t status;
    std::uint32_t error;
    std::uint32_t errorOffset; // zero-based byte offset for invalid rules
};

// Alpha-44: one captured snapshot for up to 32 independent rules. Descriptors
// and text are borrowed only for this call; reserved must be zero. Default packing.
inline constexpr std::uint32_t AETHERCLIME_MAX_BATCH_RULES_V1 = 32;
struct AetherclimeConditionRuleV1 {
    const char* text;
    std::uint32_t textBytes;
    std::uint32_t reserved;
};
struct AetherclimeConditionBatchV1 {
    std::uint32_t structSize, schemaVersion, ruleCount, snapshotAvailable;
    AetherclimeEnvironmentSnapshotV2 snapshot;
    AetherclimeConditionResultV1 results[AETHERCLIME_MAX_BATCH_RULES_V1];
};

extern "C" {
    AETHERCLIME_EXPORT std::int32_t Aetherclime_GetEnvironmentSnapshotV3(
        AetherclimeEnvironmentSnapshotV3* output, std::uint32_t outputBytes) noexcept;
    // Main thread only; count 1..32 and both sizes exact. Invalid envelope
    // returns 0 without writing output. Valid envelope returns 1, including
    // unavailable context and individual invalid rules. Input and output must
    // not overlap; caller owns all readable inputs and writable output.
    // Each result uses snapshot below; unused result slots are zero.
    // Per-rule reserved != 0 gives status=invalid, error=9 (invalid argument).
    AETHERCLIME_EXPORT std::int32_t Aetherclime_EvaluateConditionsV1(
        const AetherclimeConditionRuleV1* rules, std::uint32_t ruleCount,
        std::uint32_t ruleDescriptorBytes, AetherclimeConditionBatchV1* output,
        std::uint32_t outputBytes) noexcept;
    // Main thread only. ruleBytes excludes NUL, 1..1024; embedded NUL invalid.
    // Caller owns readable rule bytes and a writable, exactly-sized output.
    // Returns 1 when a result is written (including false/unknown/invalid),
    // 0 for null/wrong-sized output. Apply an action ONLY for status == 1.
    AETHERCLIME_EXPORT std::int32_t Aetherclime_EvaluateConditionV1(
        const char* rule, std::uint32_t ruleBytes,
        AetherclimeConditionResultV1* output, std::uint32_t outputBytes) noexcept;
    // Same caller/main-thread/size rules as V1; optional new export.
    AETHERCLIME_EXPORT std::int32_t Aetherclime_GetEnvironmentSnapshotV2(
        AetherclimeEnvironmentSnapshotV2* output, std::uint32_t outputBytes) noexcept;
    AETHERCLIME_EXPORT int Aetherclime_ApiVersion();
    AETHERCLIME_EXPORT int Aetherclime_GetSeason();
    AETHERCLIME_EXPORT double Aetherclime_GetSeasonProgress();
    AETHERCLIME_EXPORT double Aetherclime_GetAmbientC();
    AETHERCLIME_EXPORT double Aetherclime_GetFeelsLikeC();
    // Returns 1 on success, 0 on no settled player cell, wrong struct size,
    // null out, or handled error. Output cleared when pointer/size valid.
    // Caller must pass sizeof(AetherclimeEnvironmentSnapshotV1) in bytes.
    AETHERCLIME_EXPORT std::int32_t Aetherclime_GetEnvironmentSnapshotV1(
        AetherclimeEnvironmentSnapshotV1* output, std::uint32_t outputBytes) noexcept;
}
