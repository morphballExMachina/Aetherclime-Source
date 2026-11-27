#include "Aetherclime/NativeClient.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace aetherclime::client;
namespace {
enum class Mode {Good, Unavailable, Reject, BadReturn, BadSchema, DirtyUnavailable,
    BadState, BadError, BadOffset, BadCount, BadAvailability, DirtyUnused, BadNested, BadString,
    WrongVersion, ThrowVersion, ReferenceBasis, InvalidEstimateBasis};
Mode mode=Mode::Good;unsigned calls=0,versionCalls=0;
int Version(){++versionCalls;if(mode==Mode::ThrowVersion)throw std::runtime_error("fixture");return mode==Mode::WrongVersion?2:1;}
void Fill(AetherclimeEnvironmentSnapshotV1& s){s={};s.structSize=sizeof(s);s.schemaVersion=1;s.cellFormId=42;s.month=7;s.day=17;s.year=201;}
void Fill(AetherclimeEnvironmentSnapshotV2& s){s={};s.structSize=sizeof(s);s.schemaVersion=2;Fill(s.legacy);}
std::int32_t V1(AetherclimeEnvironmentSnapshotV1* out,std::uint32_t bytes) noexcept {
    ++calls;assert(out && bytes==sizeof(*out));*out={};
    if(mode==Mode::Unavailable)return 0;
    if(mode==Mode::DirtyUnavailable){out->schemaVersion=5;return 0;}
    Fill(*out);if(mode==Mode::BadSchema)out->schemaVersion=9;
    if(mode==Mode::BadString)std::memset(out->regionKey,'x',sizeof(out->regionKey));
    return mode==Mode::BadReturn?2:1;
}
std::int32_t V2(AetherclimeEnvironmentSnapshotV2* out,std::uint32_t bytes) noexcept {
    ++calls;assert(out && bytes==sizeof(*out));*out={};
    if(mode==Mode::Unavailable)return 0;
    if(mode==Mode::DirtyUnavailable){out->schemaVersion=5;return 0;}
    Fill(*out);if(mode==Mode::BadSchema)out->schemaVersion=9;
    if(mode==Mode::BadNested)out->legacy.schemaVersion=9;
    if(mode==Mode::BadString)std::memset(out->biomeA,'x',sizeof(out->biomeA));
    return mode==Mode::BadReturn?2:1;
}
void Fill(AetherclimeEnvironmentSnapshotV3& s){s={};s.structSize=sizeof(s);s.schemaVersion=3;Fill(s.legacy);}
std::int32_t V3(AetherclimeEnvironmentSnapshotV3* out,std::uint32_t bytes) noexcept {
    ++calls;assert(out && bytes==sizeof(*out));*out={};
    if(mode==Mode::Unavailable)return 0;
    if(mode==Mode::DirtyUnavailable){out->schemaVersion=5;return 0;}
    Fill(*out);if(mode==Mode::ReferenceBasis || mode==Mode::InvalidEstimateBasis){
        out->flags=AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE;
        out->outdoorEstimateC=3.0;
        out->outdoorEstimateBasis=mode==Mode::ReferenceBasis?7:99;
    }
    if(mode==Mode::BadSchema)out->schemaVersion=9;
    if(mode==Mode::BadNested)out->legacy.legacy.schemaVersion=9;
    if(mode==Mode::BadString)std::memset(out->legacy.biomeA,'x',sizeof(out->legacy.biomeA));
    return mode==Mode::BadReturn?2:1;
}
void Result(std::string_view text,AetherclimeConditionResultV1& out) {
    out={sizeof(out),1,0,0,0};
    if(text=="true")out.status=1;
    else if(text=="false")out.status=0;
    else if(text=="outdoorAirC<0")out.status=2;
    else if(text.empty()){out.status=3;out.error=1;}
    else {out.status=3;out.error=4;}
    if(mode==Mode::BadSchema)out.schemaVersion=2;
    if(mode==Mode::BadState)out.status=99;
    if(mode==Mode::BadError)out.error=99;
    if(mode==Mode::BadOffset)out.errorOffset=999;
}
std::int32_t Evaluate(const char* text,std::uint32_t bytes,AetherclimeConditionResultV1* out,std::uint32_t size) noexcept {
    ++calls;assert(out && size==sizeof(*out) && bytes<=1024);
    Result(text?std::string_view(text,bytes):std::string_view{},*out);
    return mode==Mode::Reject?0:mode==Mode::BadReturn?-1:1;
}
std::int32_t Batch(const AetherclimeConditionRuleV1* rules,std::uint32_t count,std::uint32_t stride,
    AetherclimeConditionBatchV1* out,std::uint32_t bytes) noexcept {
    ++calls;assert(rules && count>0 && count<=32 && stride==sizeof(*rules) && out && bytes==sizeof(*out));
    *out={};out->structSize=sizeof(*out);out->schemaVersion=1;out->ruleCount=count;
    if(mode!=Mode::Unavailable){out->snapshotAvailable=1;Fill(out->snapshot);}
    for(unsigned i=0;i<count;++i){assert(rules[i].reserved==0);Result(rules[i].text?std::string_view(rules[i].text,rules[i].textBytes):std::string_view{},out->results[i]);}
    if(mode==Mode::BadCount)out->ruleCount=33;
    if(mode==Mode::BadAvailability)out->snapshotAvailable=2;
    if(mode==Mode::BadNested)out->snapshot.legacy.schemaVersion=7;
    if(mode==Mode::DirtyUnused)out->results[31].status=1;
    if(mode==Mode::DirtyUnavailable){out->snapshotAvailable=0;out->snapshot.legacy.cellFormId=42;}
    return mode==Mode::Reject?0:mode==Mode::BadReturn?-1:1;
}
Functions All(){return {Version,V1,V2,Evaluate,Batch,V3};}
using Symbol=void(*)();
Symbol Resolve(const char* name) {
    const std::string_view n(name);
    if(n=="Aetherclime_ApiVersion")return reinterpret_cast<Symbol>(Version);
    if(n=="Aetherclime_GetEnvironmentSnapshotV1")return reinterpret_cast<Symbol>(V1);
    if(n=="Aetherclime_GetEnvironmentSnapshotV2")return reinterpret_cast<Symbol>(V2);
    if(n=="Aetherclime_GetEnvironmentSnapshotV3")return reinterpret_cast<Symbol>(V3);
    if(n=="Aetherclime_EvaluateConditionV1")return reinterpret_cast<Symbol>(Evaluate);
    if(n=="Aetherclime_EvaluateConditionsV1")return reinterpret_cast<Symbol>(Batch);
    assert(false);return nullptr;
}
}
int main(){
    static_assert(!std::is_convertible_v<QueryReply,bool> && !std::is_convertible_v<BatchReply,bool>);
    Client none;assert(none.Status()==CallStatus::MissingProvider && none.ExportMask()==0);
    assert(none.Query("true").State()==std::nullopt && !none.Query("true").Matches());
    auto c=Client::Discover(Resolve);assert(c.Status()==CallStatus::Ok && c.ExportMask()==63 && versionCalls==1 && calls==0);
    assert(c.SnapshotV1().value.cellFormId==42 && c.SnapshotV2().value.legacy.cellFormId==42 &&
        c.SnapshotV3().value.legacy.legacy.cellFormId==42);
    const auto yes=c.Query("true");assert(yes.status==CallStatus::Ok && yes.State()==RuleState::True && yes.Matches());
    assert(c.Query("false").State()==RuleState::False && !c.Query("false").Matches());
    assert(c.Query("outdoorAirC<0").State()==RuleState::Unknown && !c.Query("outdoorAirC<0").Matches());
    assert(c.Query("unsupported=1").State()==RuleState::Invalid);
    assert(c.Query({}).State()==RuleState::Invalid && c.Query({}).value.error==1);
    const std::string embedded("true\0false",10);assert(c.Query(embedded).State()==RuleState::Invalid);
    const std::string_view rules[]{"true","false","outdoorAirC<0","unsupported=1"};
    calls=0;const auto b=c.QueryBatch(rules);assert(calls==1 && b.status==CallStatus::Ok && b.value.ruleCount==4);
    assert(b.Matches(0) && !b.Matches(1) && b.State(2)==RuleState::Unknown && b.State(3)==RuleState::Invalid);
    assert(!b.Matches(4) && !b.State(32) && !b.State((std::numeric_limits<std::size_t>::max)()));
    // A geographically inferred V3 basis 7 is valid, but unknown future IDs
    // must not be silently accepted by this SDK version.
    mode=Mode::ReferenceBasis;assert(c.SnapshotV3().status==CallStatus::Ok);
    mode=Mode::InvalidEstimateBasis;assert(c.SnapshotV3().status==CallStatus::MalformedReply);
    mode=Mode::Unavailable;auto missing=c.QueryBatch(rules);assert(missing.status==CallStatus::Ok && !missing.value.snapshotAvailable && missing.Matches(0));
    assert(c.SnapshotV1().status==CallStatus::Unavailable && c.SnapshotV2().status==CallStatus::Unavailable && c.SnapshotV3().status==CallStatus::Unavailable);
    assert(b.value.snapshotAvailable==1); // retained value survives later provider changes
    mode=Mode::Good;
    std::string limit(1024,' ');assert(c.Query(limit).status==CallStatus::Ok);
    limit.push_back(' ');calls=0;assert(c.Query(limit).status==CallStatus::InvalidArgument && calls==0);
    std::array<std::string_view,32> maximum;maximum.fill("true");
    assert(c.QueryBatch(maximum).Matches(31));
    calls=0;assert(c.QueryBatch(nullptr,1).status==CallStatus::InvalidArgument);
    assert(c.QueryBatch(rules,0).status==CallStatus::InvalidArgument);
    assert(c.QueryBatch(rules,33).status==CallStatus::InvalidArgument);
    assert(c.QueryBatch(rules,(std::numeric_limits<std::size_t>::max)()).status==CallStatus::InvalidArgument);
    maximum[31]=limit;assert(c.QueryBatch(maximum).status==CallStatus::InvalidArgument && calls==0);
    for(auto bad:{Mode::BadReturn,Mode::BadSchema,Mode::BadState,Mode::BadError,Mode::BadOffset}){
        mode=bad;const auto q=c.Query("true");assert(q.status==CallStatus::MalformedReply && !q.Matches() && q.value.structSize==0);
        const auto batch=c.QueryBatch(rules);assert(batch.status==CallStatus::MalformedReply && !batch.Matches(0));
    }
    for(auto bad:{Mode::BadCount,Mode::BadAvailability,Mode::DirtyUnused,Mode::BadNested,Mode::DirtyUnavailable}){
        mode=bad;const auto q=c.QueryBatch(rules);assert(q.status==CallStatus::MalformedReply && q.value.ruleCount==0);
    }
    for(auto bad:{Mode::BadSchema,Mode::BadString,Mode::DirtyUnavailable,Mode::BadReturn}){
        mode=bad;assert(c.SnapshotV1().status==CallStatus::MalformedReply && c.SnapshotV2().status==CallStatus::MalformedReply &&
            c.SnapshotV3().status==CallStatus::MalformedReply);
    }
    mode=Mode::BadNested;assert(c.SnapshotV2().status==CallStatus::MalformedReply &&
        c.SnapshotV3().status==CallStatus::MalformedReply);
    mode=Mode::Reject;assert(c.Query("true").status==CallStatus::ProviderRejected && c.QueryBatch(rules).status==CallStatus::ProviderRejected);
    // Old providers: existing features work, missing features never silently fall back.
    mode=Mode::Good;auto f=All();f.batch=nullptr;Client old(f);calls=0;
    assert(old.QueryBatch(rules).status==CallStatus::MissingExport && calls==0);
    assert(old.Query("true").Matches());
    f.v2=nullptr;f.v3=nullptr;f.evaluate=nullptr;Client v1only(f);assert(v1only.ExportMask()==3);
    assert(v1only.SnapshotV1().status==CallStatus::Ok);
    calls=0;assert(v1only.Query("true").status==CallStatus::MissingExport && v1only.SnapshotV2().status==CallStatus::MissingExport &&
        v1only.SnapshotV3().status==CallStatus::MissingExport && calls==0);
    auto absent=Client::Discover([](const char*)->Symbol{return nullptr;});assert(absent.Status()==CallStatus::MissingProvider);
    auto broken=Client::Discover([](const char*)->Symbol{throw std::runtime_error("resolver");});assert(broken.Status()==CallStatus::ProviderError && broken.ExportMask()==0);
    for(auto bad:{Mode::WrongVersion,Mode::ThrowVersion}){
        mode=bad;Client fail(All());calls=0;
        assert(fail.Status()==(bad==Mode::WrongVersion?CallStatus::IncompatibleProvider:CallStatus::ProviderError));
        assert(!fail.Query("true").Matches() && !fail.QueryBatch(rules).Matches(0));
        assert(fail.SnapshotV1().status==fail.Status() && fail.SnapshotV2().status==fail.Status() && fail.SnapshotV3().status==fail.Status() && calls==0);
    }
    mode=Mode::Good;f=All();f.version=nullptr;Client noVersion(f);calls=0;
    assert(noVersion.Query("true").status==CallStatus::MissingProvider && calls==0);
}
