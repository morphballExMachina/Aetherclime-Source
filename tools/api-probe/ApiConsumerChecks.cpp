#include "Aetherclime/ApiConsumerProbe.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>

namespace aetherclime::probe {
namespace {
template<class T> bool Zero(const T& value) noexcept {
    const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
    for(std::size_t i=0;i<sizeof(value);++i) if(bytes[i]) return false;
    return true;
}
template<std::size_t N> bool Text(const char (&s)[N]) noexcept {return std::memchr(s,0,N)!=nullptr;}
bool V1Shape(const AetherclimeEnvironmentSnapshotV1& s) noexcept {
    return s.structSize==sizeof(s) && s.schemaVersion==1 && s.cellFormId!=0 &&
        Text(s.localName) && Text(s.regionKey) && Text(s.regionDisplayName) &&
        s.month>=0 && s.month<=11 && s.day>=1 && s.day<=40 &&
        std::isfinite(s.hour) && s.hour>=0 && s.hour<24 &&
        std::isfinite(s.outdoorAirC) && std::isfinite(s.outdoorFeelsLikeC) &&
        (!(s.flags&AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE) || std::isfinite(s.localAmbientC));
}
bool V2Shape(const AetherclimeEnvironmentSnapshotV2& s) noexcept {
    return s.structSize==sizeof(s) && s.schemaVersion==2 && s.reserved==0 && V1Shape(s.legacy) &&
        Text(s.biomeA) && Text(s.biomeB) && Text(s.biomeStatus);
}
bool Optional(const AetherclimeEnvironmentSnapshotV2& s) noexcept {
    if(s.flags&AETHERCLIME_ENV2_SEASON_AVAILABLE) {
        double sum=0;if(s.officialSeason>3) return false;
        for(double w:s.seasonWeights) {if(!std::isfinite(w)||w<0||w>1) return false;sum+=w;}
        if(std::abs(sum-1)>1e-8) return false;
    } else {
        if(s.officialSeason!=0) return false;
        for(double w:s.seasonWeights) if(w!=0) return false;
    }
    if(s.flags&AETHERCLIME_ENV2_BIOME_AVAILABLE) {
        if((s.legacy.flags&AETHERCLIME_ENV_INTERIOR) || !s.biomeA[0] || s.biomeEvidence<1 || s.biomeEvidence>4) return false;
        if(!std::isfinite(s.biomeWeightA)||!std::isfinite(s.biomeWeightB)||s.biomeWeightA<=0||s.biomeWeightB<0||
           s.biomeWeightA>1||s.biomeWeightB>1||std::abs(s.biomeWeightA+s.biomeWeightB-1)>1e-8) return false;
        if((s.biomeEvidence==4)!=((s.flags&AETHERCLIME_ENV2_BIOME_NEARBY)!=0)) return false;
        if(s.biomeEvidence==2) {
            if(!s.biomeB[0] || std::strcmp(s.biomeA,s.biomeB)==0 || s.biomeWeightB<=0) return false;
        } else if(s.biomeB[0] || s.biomeWeightA!=1 || s.biomeWeightB!=0) return false;
    } else if(s.biomeA[0]||s.biomeB[0]||s.biomeWeightA!=0||s.biomeWeightB!=0||s.biomeEvidence!=0||
              (s.flags&AETHERCLIME_ENV2_BIOME_NEARBY)) return false;
    if(s.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE) {
        if(!(s.flags&AETHERCLIME_ENV2_BIOME_AVAILABLE) || !std::isfinite(s.previewAirC)) return false;
    } else if(s.previewAirC!=0) return false;
    return true;
}
bool V3Shape(const AetherclimeEnvironmentSnapshotV3& s) noexcept {
    if(s.structSize!=sizeof(s)||s.schemaVersion!=3||s.reserved!=0||!V2Shape(s.legacy)||!Optional(s.legacy)||
       (s.flags&~3u))return false;
    const bool outdoor=(s.flags&AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE)!=0;
    const bool indoor=(s.flags&AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE)!=0;
    if(outdoor ? (s.outdoorEstimateBasis<1||s.outdoorEstimateBasis>7 ||
        !std::isfinite(s.outdoorEstimateC)||std::abs(s.outdoorEstimateC)>100) :
        (s.outdoorEstimateBasis!=0||s.outdoorEstimateC!=0))return false;
    if(indoor ? (s.indoorEstimateBasis<1||s.indoorEstimateBasis>3||
        !(s.legacy.legacy.flags&AETHERCLIME_ENV_INTERIOR)||
        !std::isfinite(s.indoorEstimateC)||std::abs(s.indoorEstimateC)>100) :
        (s.indoorEstimateBasis!=0||s.indoorEstimateC!=0))return false;
    return true;
}
bool Query(const ApiFunctions& api,const char* text,std::uint32_t expected,std::uint32_t error=0,std::uint32_t offset=0) noexcept {
    AetherclimeConditionResultV1 r{};
    return api.evaluate(text,static_cast<std::uint32_t>(std::strlen(text)),&r,sizeof(r))==1 &&
        r.structSize==sizeof(r) && r.schemaVersion==1 && r.status==expected && r.error==error && r.errorOffset==offset;
}
void Execute(const ApiFunctions& api,ReportV1& r) {
    const auto check=[&](unsigned index,bool pass){r.checks[index]=pass?Passed:Failed;};
    r.resolvedMask=(api.version?1u:0u)|(api.v1?2u:0u)|(api.v2?4u:0u)|(api.evaluate?8u:0u)|(api.batch?16u:0u)|(api.v3?32u:0u);
    check(0,(r.resolvedMask&31u)==31u);
    if((r.resolvedMask&31u)!=31u) return;
    check(1,api.version()==1);
    AetherclimeEnvironmentSnapshotV1 before{},after{};
    const auto got1=api.v1(&before,sizeof(before));
    const auto got2=api.v2(&r.observed,sizeof(r.observed));
    const auto gotAfter=api.v1(&after,sizeof(after));
    check(2,got1==1 ? V1Shape(before) : got1==0 && Zero(before));
    check(3,got2==1 ? V2Shape(r.observed) : got2==0 && Zero(r.observed));
    r.contextAvailable=got2==1 && r.checks[3]==Passed;
    if(r.contextAvailable) check(15,Optional(r.observed));
    // Bracket separate on-demand reads. A change is a SKIP, never a false failure
    // or an assumption that separate calls sampled one instant.
    bool stable=got1==1 && got2==1 && gotAfter==1 && r.checks[2]==Passed && r.checks[3]==Passed &&
        V1Shape(after) && std::memcmp(&before,&after,sizeof(before))==0;
    if(stable) check(4,std::memcmp(&before,&r.observed.legacy,sizeof(before))==0);
    check(5,Query(api,"true",1) && Query(api,"false",0));
    check(6,Query(api,"false & unsupportedField=1",3,4,8));
    check(11,Query(api,"true | false & false",1) && Query(api,"(true | false) & false",0));
    // Full-size allocated buffers with wrong declared sizes; never invalid memory.
    AetherclimeEnvironmentSnapshotV1 bad1{};bad1.schemaVersion=991;
    AetherclimeEnvironmentSnapshotV2 bad2{};bad2.schemaVersion=992;
    AetherclimeConditionResultV1 badq{};badq.schemaVersion=993;
    const auto keep1=bad1;const auto keep2=bad2;const auto keepq=badq;
    check(12,api.v1(&bad1,sizeof(bad1)-1)==0 && api.v2(&bad2,sizeof(bad2)-1)==0 &&
        api.evaluate("true",4,&badq,sizeof(badq)-1)==0 &&
        std::memcmp(&bad1,&keep1,sizeof(bad1))==0 && std::memcmp(&bad2,&keep2,sizeof(bad2))==0 && std::memcmp(&badq,&keepq,sizeof(badq))==0 &&
        api.v1(nullptr,sizeof(bad1))==0 && api.v2(nullptr,sizeof(bad2))==0 && api.evaluate("true",4,nullptr,sizeof(badq))==0);
    AetherclimeConditionResultV1 len{};
    char oversized[1025]{};std::memset(oversized,' ',sizeof(oversized));
    const bool longOK=api.evaluate(oversized,sizeof(oversized),&len,sizeof(len))==1 && len.status==3 && len.error==2 && len.errorOffset==1024;
    const bool nullOK=api.evaluate(nullptr,0,&len,sizeof(len))==1 && len.status==3 && len.error==1;
    check(13,longOK && nullOK);
    check(14,Query(api,"time=24:00",3,5,5) && Query(api,"seasonWeight(bogus)>0",3,5,0));
    // Batch expectations come from the returned snapshot, never a separate read.
    const AetherclimeConditionRuleV1 rules[]{
        {"true",4,0},{"false",5,0},{"outdoorAirC<0",13,0},
        {"previewAirC>0",13,0},{"false & unsupportedField=1",26,0}};
    AetherclimeConditionBatchV1 batch{};
    const auto resultOK=[](const AetherclimeConditionResultV1& q,unsigned status,unsigned error=0,unsigned offset=0) {
        return q.structSize==sizeof(q) && q.schemaVersion==1 && q.status==status && q.error==error && q.errorOffset==offset;
    };
    bool batchOK=api.batch(rules,5,sizeof(rules[0]),&batch,sizeof(batch))==1 &&
        batch.structSize==sizeof(batch) && batch.schemaVersion==1 && batch.ruleCount==5 && batch.snapshotAvailable<=1;
    const auto& bs=batch.snapshot;
    batchOK=batchOK && (batch.snapshotAvailable ? V2Shape(bs)&&Optional(bs) : Zero(bs));
    const auto air=batch.snapshotAvailable && (bs.legacy.flags&AETHERCLIME_ENV_TRUSTED_REGION) ? (bs.legacy.outdoorAirC<0?1u:0u) : 2u;
    const auto preview=batch.snapshotAvailable && (bs.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE) ? (bs.previewAirC>0?1u:0u) : 2u;
    batchOK=batchOK && resultOK(batch.results[0],1) && resultOK(batch.results[1],0) &&
        resultOK(batch.results[2],air) && resultOK(batch.results[3],preview) && resultOK(batch.results[4],3,4,8);
    for(unsigned i=5;i<AETHERCLIME_MAX_BATCH_RULES_V1;++i) batchOK=batchOK && Zero(batch.results[i]);
    check(16,batchOK);
    AetherclimeConditionBatchV1 badBatch{};badBatch.schemaVersion=994;const auto keepBatch=badBatch;
    bool contract=api.batch(rules,0,sizeof(rules[0]),&badBatch,sizeof(badBatch))==0 &&
        api.batch(rules,33,sizeof(rules[0]),&badBatch,sizeof(badBatch))==0 &&
        api.batch(rules,5,sizeof(rules[0])-1,&badBatch,sizeof(badBatch))==0 &&
        api.batch(rules,5,sizeof(rules[0]),&badBatch,sizeof(badBatch)-1)==0 &&
        api.batch(nullptr,1,sizeof(rules[0]),&badBatch,sizeof(badBatch))==0 &&
        api.batch(rules,5,sizeof(rules[0]),nullptr,sizeof(badBatch))==0 &&
        std::memcmp(&badBatch,&keepBatch,sizeof(badBatch))==0;
    const AetherclimeConditionRuleV1 malformed[]{{"true",4,1},{nullptr,0,0},{oversized,1025,0}};
    contract=contract && api.batch(malformed,3,sizeof(malformed[0]),&badBatch,sizeof(badBatch))==1 &&
        resultOK(badBatch.results[0],3,9) && resultOK(badBatch.results[1],3,1) && resultOK(badBatch.results[2],3,2,1024);
    check(17,contract);
    if(stable && r.checks[4]==Passed && r.checks[15]==Passed) {
        const auto& s=r.observed;char rule[160]{};
        std::snprintf(rule,sizeof(rule),"year=%d & month=%d & day=%d",s.legacy.year,s.legacy.month,s.legacy.day);
        check(7,Query(api,rule,1));
        check(8,Query(api,"outdoorAirC<0",(s.legacy.flags&AETHERCLIME_ENV_TRUSTED_REGION)?(s.legacy.outdoorAirC<0?1u:0u):2u));
        std::uint32_t biomeExpected=2;
        if((s.flags&AETHERCLIME_ENV2_BIOME_AVAILABLE) && !(s.flags&AETHERCLIME_ENV2_BIOME_NEARBY)) {
            const double share=std::strcmp(s.biomeA,"coastal")==0?s.biomeWeightA:std::strcmp(s.biomeB,"coastal")==0?s.biomeWeightB:0;
            biomeExpected=share>=.5?1:0;
        }
        check(9,Query(api,"biomeWeight(coastal)>=0.5",biomeExpected));
        check(10,Query(api,"previewAirC>0",(s.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE)?(s.previewAirC>0?1u:0u):2u));
        AetherclimeEnvironmentSnapshotV2 end{};
        if(api.v2(&end,sizeof(end))!=1 || std::memcmp(&s,&end,sizeof(s))!=0) {
            // State changed during live queries: context-dependent comparisons
            // become inconclusive, but unrelated ABI/grammar failures remain.
            for(unsigned i=7;i<=10;++i) r.checks[i]=Skipped;
        }
    }
    // Exercise the shipped header-only client across the real DLL boundary too.
    // The older raw ABI checks above remain independent of this wrapper.
    const aetherclime::client::Client client(api);
    using aetherclime::client::CallStatus;
    using aetherclime::client::RuleState;
    const auto yes=client.Query("true"), no=client.Query("false"), invalid=client.Query("false & unsupportedField=1");
    check(18,client.Status()==CallStatus::Ok && client.ExportMask()==63 && yes.Matches() &&
        no.State()==RuleState::False && !no.Matches() && invalid.State()==RuleState::Invalid &&
        invalid.value.error==4 && invalid.value.errorOffset==8 && !invalid.Matches());
    const std::string_view clientRules[]{"true","false","outdoorAirC<0","previewAirC>0"};
    const auto reply=client.QueryBatch(clientRules);
    const auto& context=reply.value.snapshot;
    const auto expectedAir=reply.value.snapshotAvailable && (context.legacy.flags&AETHERCLIME_ENV_TRUSTED_REGION) ?
        (context.legacy.outdoorAirC<0?RuleState::True:RuleState::False) : RuleState::Unknown;
    const auto expectedPreview=reply.value.snapshotAvailable && (context.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE) ?
        (context.previewAirC>0?RuleState::True:RuleState::False) : RuleState::Unknown;
    check(19,reply.status==CallStatus::Ok && reply.value.ruleCount==4 && reply.Matches(0) &&
        reply.State(1)==RuleState::False && reply.State(2)==expectedAir && reply.State(3)==expectedPreview &&
        !reply.State(4).has_value() && !reply.Matches(4));
    const std::string_view contextRules[]{"localAmbientC<0","weather=clear","interior=true"};
    const auto contextReply=client.QueryBatch(contextRules);
    const auto& observed=contextReply.value.snapshot.legacy;
    const bool available=contextReply.value.snapshotAvailable==1;
    const auto flags=observed.flags;
    const bool interior=(flags&AETHERCLIME_ENV_INTERIOR)!=0;
    const bool trusted=(flags&AETHERCLIME_ENV_TRUSTED_REGION)!=0;
    const bool local=(flags&AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE)!=0;
    const bool modeled=(flags&AETHERCLIME_ENV_LOCAL_AMBIENT_MODELED_INDOOR)!=0;
    const auto ambientExpected=available&&local ? (observed.localAmbientC<0?RuleState::True:RuleState::False) : RuleState::Unknown;
    const auto weatherExpected=available&&(flags&AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER)&&observed.weatherKind<6 ?
        (observed.weatherKind==0?RuleState::True:RuleState::False) : RuleState::Unknown;
    const auto interiorExpected=available ? (interior?RuleState::True:RuleState::False) : RuleState::Unknown;
    const bool contextFlags=!available || (!(trusted&&(flags&AETHERCLIME_ENV_PROVISIONAL_REGION)) &&
        (!modeled||(interior&&trusted&&local)) && (!local||trusted) && (!interior||!local||modeled));
    check(20,contextReply.status==CallStatus::Ok && contextFlags &&
        contextReply.State(0)==ambientExpected && contextReply.State(1)==weatherExpected && contextReply.State(2)==interiorExpected);
    // Alpha-50: independently verify V3 against the real DLL boundary.
    // V3 is required for this milestone, but retains its OPTIONAL public ABI
    // contract for older consumer plugins that only discover V1/V2.
    check(21,api.v3!=nullptr);
    if(api.v3) {
        const auto got3=api.v3(&r.observedV3,sizeof(r.observedV3));
        check(22,got3==1 ? V3Shape(r.observedV3) : got3==0 && Zero(r.observedV3));
        if(got2==0 && got3==0) {
            // Both absent; independent API reads cannot assert a shared moment.
            r.checks[23]=Skipped;
        } else if(got2==1 && got3==1 && r.checks[3]==Passed && r.checks[22]==Passed) {
            if(std::memcmp(&r.observed,&r.observedV3.legacy,sizeof(r.observed))==0)check(23,true);
            else {
                AetherclimeEnvironmentSnapshotV2 afterV3{};
                const bool unchanged=api.v2(&afterV3,sizeof(afterV3))==1 &&
                    std::memcmp(&afterV3,&r.observed,sizeof(r.observed))==0;
                if(unchanged)check(23,false);
                else r.checks[23]=Skipped; // changing game context between calls
            }
        }
        AetherclimeEnvironmentSnapshotV3 bad3{};
        bad3.schemaVersion=993;const auto keep3=bad3;
        check(24,api.v3(&bad3,sizeof(bad3)-1)==0 &&
            std::memcmp(&bad3,&keep3,sizeof(bad3))==0 &&
            api.v3(nullptr,sizeof(bad3))==0);
        const auto value=client.SnapshotV3();
        check(25,value.status==(r.checks[22]==Passed && got3==1 ?
            aetherclime::client::CallStatus::Ok :
            got3==0 ? aetherclime::client::CallStatus::Unavailable :
            aetherclime::client::CallStatus::MalformedReply) &&
            (value.status==aetherclime::client::CallStatus::Ok ?
                V3Shape(value.value) : Zero(value.value)));
    }
}
} // namespace
static_assert(std::is_standard_layout_v<ReportV1> && std::is_trivially_copyable_v<ReportV1>);
ReportV1 RunChecks(const ApiFunctions& api) noexcept {
    ReportV1 r{};r.structSize=sizeof(r);r.schemaVersion=1;
    std::memcpy(r.marker,kMarker,sizeof(kMarker));
    try {Execute(api,r);r.completed=1;} catch(...) {r.completed=0;}
    for(auto s:r.checks) {if(s==Passed) ++r.passed;else if(s==Failed) ++r.failed;else ++r.skipped;}
    return r;
}
} // namespace aetherclime::probe
