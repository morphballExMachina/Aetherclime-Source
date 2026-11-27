// Adversarial public-ABI fixtures; no linkage to AetherclimeCore or the parser.
#include "Aetherclime/ApiConsumerProbe.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string_view>
using namespace aetherclime::probe;
namespace {
enum class Mode { Good, Unavailable, WrongVersion, BadV2, Mismatch, BadQuery, WrongSizeWrite, Changing, DirtyUnavailable, BadWeights, ThrowVersion, Nearby, Untrusted, Interior, BadBatch, BadBatchBuffer, ModeledInterior, BadContextQuery, BadV3, BadV3Basis };
Mode mode=Mode::Good;unsigned v1Calls=0,v2Calls=0;
void Reset(Mode m){mode=m;v1Calls=v2Calls=0;}
int Version(){if(mode==Mode::ThrowVersion) throw std::runtime_error("fixture");return mode==Mode::WrongVersion?2:1;}
void Fill(AetherclimeEnvironmentSnapshotV1& s) {
    std::memset(&s,0,sizeof(s));s.structSize=sizeof(s);s.schemaVersion=1;s.cellFormId=42;
    s.flags=AETHERCLIME_ENV_TRUSTED_REGION|AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE;
    s.year=201;s.month=7;s.day=17;s.hour=9.5;s.outdoorAirC=2;s.outdoorFeelsLikeC=1;s.localAmbientC=2;
    std::strcpy(s.regionKey,"winterhold");
    if(mode==Mode::Untrusted) {s.flags=0;s.localAmbientC=0;}
    if(mode==Mode::Interior || mode==Mode::ModeledInterior) {
        s.flags|=AETHERCLIME_ENV_INTERIOR;
        if(mode==Mode::ModeledInterior){s.flags|=AETHERCLIME_ENV_LOCAL_AMBIENT_MODELED_INDOOR;s.localAmbientC=16;}
        else {s.flags&=~AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE;s.localAmbientC=0;}
    }
}
std::int32_t V1(AetherclimeEnvironmentSnapshotV1* s,std::uint32_t bytes) noexcept {
    if(!s || bytes!=sizeof(*s)) {if(s && mode==Mode::WrongSizeWrite) s->schemaVersion=0;return 0;}
    ++v1Calls;std::memset(s,0,sizeof(*s));
    if(mode==Mode::Unavailable || mode==Mode::DirtyUnavailable) {if(mode==Mode::DirtyUnavailable)s->cellFormId=55;return 0;}
    Fill(*s);return 1;
}
std::int32_t V2(AetherclimeEnvironmentSnapshotV2* s,std::uint32_t bytes) noexcept {
    if(!s || bytes!=sizeof(*s)) return 0;
    ++v2Calls;std::memset(s,0,sizeof(*s));
    if(mode==Mode::Unavailable || mode==Mode::DirtyUnavailable) return 0;
    s->structSize=sizeof(*s);s->schemaVersion=mode==Mode::BadV2?99:2;Fill(s->legacy);
    s->flags=11;s->officialSeason=2;s->seasonWeights[2]=1;
    s->biomeEvidence=1;s->biomeWeightA=1;std::strcpy(s->biomeA,"coastal");s->previewAirC=10;
    if(mode==Mode::Mismatch) s->legacy.outdoorAirC=99;
    if(mode==Mode::Changing && v2Calls>1) s->previewAirC=-10;
    if(mode==Mode::BadWeights) s->seasonWeights[2]=2;
    if(mode==Mode::Nearby) {s->flags|=AETHERCLIME_ENV2_BIOME_NEARBY;s->biomeEvidence=4;}
    if(mode==Mode::Interior || mode==Mode::ModeledInterior) {s->flags=1;s->biomeEvidence=0;s->biomeWeightA=0;s->biomeA[0]=0;s->previewAirC=0;}
    return 1;
}
std::int32_t V3(AetherclimeEnvironmentSnapshotV3* s,std::uint32_t bytes) noexcept {
    if(!s || bytes!=sizeof(*s)) {if(s && mode==Mode::WrongSizeWrite)s->schemaVersion=0;return 0;}
    *s={};
    if(mode==Mode::Unavailable || mode==Mode::DirtyUnavailable) return 0;
    s->structSize=sizeof(*s);s->schemaVersion=mode==Mode::BadV3?99:3;
    const auto saved=v2Calls;
    V2(&s->legacy,sizeof(s->legacy));
    v2Calls=saved;
    s->flags=AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE;
    s->outdoorEstimateBasis=mode==Mode::Untrusted?7:1; // Alpha-49.1 reference basis 7 MUST be accepted.
    s->outdoorEstimateC=2;
    if(mode==Mode::BadV3Basis)s->outdoorEstimateBasis=999;
    if(mode==Mode::Interior||mode==Mode::ModeledInterior) {
        s->flags|=AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE;
        s->indoorEstimateBasis=mode==Mode::ModeledInterior?1:2;
        s->indoorEstimateC=mode==Mode::ModeledInterior?16:18;
    }
    return 1;
}
std::int32_t Evaluate(const char* text,std::uint32_t bytes,AetherclimeConditionResultV1* r,std::uint32_t size) noexcept {
    if(!r || size!=sizeof(*r)) return 0;
    *r={sizeof(*r),1,0,0,0};
    if(!text){r->status=3;r->error=1;return 1;}
    if(bytes>1024){r->status=3;r->error=2;r->errorOffset=1024;return 1;}
    const std::string_view q(text,bytes);
    if(q=="true" || q=="true | false & false")r->status=1;
    else if(q=="false" || q=="(true | false) & false")r->status=0;
    else if(q=="false & unsupportedField=1"){r->status=3;r->error=4;r->errorOffset=8;}
    else if(q=="time=24:00"){r->status=3;r->error=5;r->errorOffset=5;}
    else if(q=="seasonWeight(bogus)>0"){r->status=3;r->error=5;}
    else if(q=="year=201 & month=7 & day=17")r->status=1;
    else if(q=="outdoorAirC<0")r->status=mode==Mode::Untrusted?2:mode==Mode::BadQuery?1:0;
    else if(q=="biomeWeight(coastal)>=0.5")r->status=mode==Mode::Nearby || mode==Mode::Interior || mode==Mode::ModeledInterior?2:1;
    else if(q=="previewAirC>0")r->status=mode==Mode::Interior || mode==Mode::ModeledInterior?2:1;
    else {r->status=3;r->error=4;}
    return 1;
}
std::int32_t Batch(const AetherclimeConditionRuleV1* rules,std::uint32_t count,std::uint32_t stride,
    AetherclimeConditionBatchV1* out,std::uint32_t bytes) noexcept {
    if(!rules || !out || !count || count>32 || stride!=sizeof(*rules) || bytes!=sizeof(*out)) {
        if(out && mode==Mode::BadBatchBuffer) out->schemaVersion=0;
        return 0;
    }
    std::memset(out,0,sizeof(*out));out->structSize=sizeof(*out);out->schemaVersion=1;out->ruleCount=count;
    // Fixture's batch capture is independent of calls to the single snapshot API.
    const auto savedCalls=v2Calls;
    out->snapshotAvailable=static_cast<unsigned>(V2(&out->snapshot,sizeof(out->snapshot)));
    v2Calls=savedCalls;
    for(unsigned i=0;i<count;++i) {
        auto& r=out->results[i];
        Evaluate(rules[i].text,rules[i].textBytes,&r,sizeof(r));
        if(rules[i].reserved) r={sizeof(r),1,3,9,0};
        const std::string_view q=rules[i].text && rules[i].textBytes<=1024 ? std::string_view(rules[i].text,rules[i].textBytes) : std::string_view{};
        if(q=="outdoorAirC<0") r.status=out->snapshotAvailable && (out->snapshot.legacy.flags&AETHERCLIME_ENV_TRUSTED_REGION) ? (out->snapshot.legacy.outdoorAirC<0?1u:0u) : 2u;
        if(q=="previewAirC>0") r.status=out->snapshotAvailable && (out->snapshot.flags&AETHERCLIME_ENV2_PREVIEW_AIR_AVAILABLE) ? (out->snapshot.previewAirC>0?1u:0u) : 2u;
        if(q=="localAmbientC<0") r={sizeof(r),1,out->snapshotAvailable && (out->snapshot.legacy.flags&AETHERCLIME_ENV_LOCAL_AMBIENT_AVAILABLE) ?
            (out->snapshot.legacy.localAmbientC<0?1u:0u) : 2u,0,0};
        if(q=="weather=clear") r={sizeof(r),1,out->snapshotAvailable && (out->snapshot.legacy.flags&AETHERCLIME_ENV_VERIFIED_OUTDOOR_WEATHER) && out->snapshot.legacy.weatherKind<6 ?
            (out->snapshot.legacy.weatherKind==0?1u:0u) : 2u,0,0};
        if(mode==Mode::BadContextQuery && q=="localAmbientC<0")r.status=1;
        if(q=="interior=true") r={sizeof(r),1,out->snapshotAvailable ? ((out->snapshot.legacy.flags&AETHERCLIME_ENV_INTERIOR)?1u:0u) : 2u,0,0};
    }
    if(mode==Mode::BadBatch) out->results[0].status=0;
    return 1;
}
ApiFunctions Functions(){return {Version,V1,V2,Evaluate,Batch,V3};}
void Totals(const ReportV1& r){assert(r.passed+r.failed+r.skipped==kCheckCount && r.structSize==sizeof(r) && r.schemaVersion==1);}
}
int main(){
    Reset(Mode::Good);auto r=RunChecks(Functions());Totals(r);
    assert(r.completed==1 && r.resolvedMask==63 && r.passed==26 && r.failed==0 && r.skipped==0 && r.contextAvailable==1);
    assert(std::strcmp(r.marker,kMarker)==0 && v1Calls==2 && v2Calls>=2);
    for(unsigned missing=0;missing<5;++missing){
        auto api=Functions();if(missing==0)api.version=nullptr;if(missing==1)api.v1=nullptr;if(missing==2)api.v2=nullptr;if(missing==3)api.evaluate=nullptr;if(missing==4)api.batch=nullptr;
        Reset(Mode::Good);r=RunChecks(api);Totals(r);assert(r.failed==1 && r.skipped==25 && v1Calls==0 && v2Calls==0);
    }
    Reset(Mode::Unavailable);r=RunChecks(Functions());Totals(r);assert(r.failed==0 && r.skipped>=6 && r.contextAvailable==0);
    Reset(Mode::BadContextQuery);r=RunChecks(Functions());assert(r.checks[20]==Failed && r.failed==1);
    Reset(Mode::BadBatch);r=RunChecks(Functions());assert(r.checks[16]==Failed);
    Reset(Mode::BadBatchBuffer);r=RunChecks(Functions());assert(r.checks[17]==Failed);
    Reset(Mode::WrongVersion);r=RunChecks(Functions());assert(r.checks[1]==Failed);
    Reset(Mode::BadV2);r=RunChecks(Functions());assert(r.checks[3]==Failed && r.contextAvailable==0);
    Reset(Mode::Mismatch);r=RunChecks(Functions());assert(r.checks[4]==Failed && r.checks[7]==Skipped);
    Reset(Mode::BadQuery);r=RunChecks(Functions());assert(r.checks[8]==Failed && r.failed==1);
    Reset(Mode::WrongSizeWrite);r=RunChecks(Functions());assert(r.checks[12]==Failed);
    Reset(Mode::Changing);r=RunChecks(Functions());assert(r.failed==0 && r.skipped==5 && r.checks[8]==Skipped && r.checks[23]==Skipped);
    Reset(Mode::DirtyUnavailable);r=RunChecks(Functions());assert(r.checks[2]==Failed);
    Reset(Mode::BadWeights);r=RunChecks(Functions());assert(r.checks[15]==Failed && r.checks[7]==Skipped && r.checks[22]==Failed);
    Reset(Mode::BadV3);r=RunChecks(Functions());assert(r.checks[22]==Failed && r.checks[25]==Passed);
    Reset(Mode::BadV3Basis);r=RunChecks(Functions());assert(r.checks[22]==Failed && r.checks[25]==Passed);
    Reset(Mode::Good);auto missingV3=Functions();missingV3.v3=nullptr;
    r=RunChecks(missingV3);assert(r.checks[0]==Passed && r.checks[21]==Failed && r.checks[22]==Skipped);
    Reset(Mode::ThrowVersion);r=RunChecks(Functions());Totals(r);assert(r.completed==0 && r.skipped==25);
    for(auto m:{Mode::Nearby,Mode::Untrusted,Mode::Interior,Mode::ModeledInterior}){Reset(m);r=RunChecks(Functions());Totals(r);assert(r.passed==26 && r.failed==0 && r.skipped==0);}
}
