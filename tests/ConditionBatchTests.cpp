#include "Aetherclime/ConditionQuery.h"
#include <array>
#include <cassert>
#include <cstring>
#include <string>
using namespace aetherclime;
int main() {
    EnvironmentSnapshot state;
    state.available=true;state.trustedRegion=true;state.local.cellFormId=42;
    state.date={7,17,9.5,201};state.outdoorAirC=-2;state.outdoorFeelsLikeC=-3;
    const AetherclimeConditionRuleV1 rules[]{
        {"true",4,0},{"false",5,0},{"outdoorAirC<0",13,0},
        {"previewAirC>0",13,0},{"false & unsupportedField=1",26,0},
        {"month=7 & time=09:30",20,0},{nullptr,0,0},{"true",4,1}};
    AetherclimeConditionBatchV1 out{};
    auto run=[&](const EnvironmentSnapshot& s,bool failed=false) {
        assert(WriteConditionBatchV1(rules,8,sizeof(rules[0]),s,&out,sizeof(out),failed)==1);
        assert(out.structSize==sizeof(out) && out.schemaVersion==1 && out.ruleCount==8);
    };
    run(state);
    assert(out.snapshotAvailable==1 && out.snapshot.legacy.outdoorAirC==-2);
    const unsigned expected[]{1,0,1,2,3,1,3,3};
    for(unsigned i=0;i<8;++i) assert(out.results[i].status==expected[i]);
    assert(out.results[4].error==4 && out.results[4].errorOffset==8);
    assert(out.results[6].error==1 && out.results[7].error==9);
    for(unsigned i=8;i<32;++i) {const AetherclimeConditionResultV1 zero{};assert(std::memcmp(&out.results[i],&zero,sizeof(zero))==0);}
    // Ownership: subsequent state changes cannot alter the returned value record.
    state.outdoorAirC=4;assert(out.snapshot.legacy.outdoorAirC==-2 && out.results[2].status==1);
    run(state);assert(out.snapshot.legacy.outdoorAirC==4 && out.results[2].status==0);
    state.trustedRegion=false;run(state);assert(out.results[2].status==2);
    run(EnvironmentSnapshot{});assert(out.snapshotAvailable==0 && out.results[0].status==1 && out.results[1].status==0);
    assert(out.results[2].status==2 && out.results[5].status==2 && out.results[4].status==3);
    const AetherclimeEnvironmentSnapshotV2 zero{};assert(std::memcmp(&out.snapshot,&zero,sizeof(zero))==0);
    run(state,true);assert(out.snapshotAvailable==0);
    for(unsigned i=0;i<8;++i) assert(out.results[i].status==3 && out.results[i].error==8);
    const auto keep=out;
    for(auto n:{0u,33u,0xffffffffu}) assert(WriteConditionBatchV1(rules,n,sizeof(rules[0]),state,&out,sizeof(out))==0);
    assert(WriteConditionBatchV1(nullptr,1,sizeof(rules[0]),state,&out,sizeof(out))==0);
    assert(WriteConditionBatchV1(rules,1,sizeof(rules[0])-1,state,&out,sizeof(out))==0);
    assert(WriteConditionBatchV1(rules,1,sizeof(rules[0]),state,&out,sizeof(out)-1)==0);
    assert(WriteConditionBatchV1(rules,1,sizeof(rules[0]),state,nullptr,sizeof(out))==0);
    assert(std::memcmp(&out,&keep,sizeof(out))==0);
    // Season availability must agree with V2, including malformed optional state.
    AetherclimeConditionRuleV1 seasonal{"season=summer",13,0};
    state.season=SeasonState{};state.season->official=Season::Summer;
    state.season->weights={0,0,1,0};
    assert(WriteConditionBatchV1(&seasonal,1,sizeof(seasonal),state,&out,sizeof(out))==1);
    assert(out.results[0].status==1 && (out.snapshot.flags&AETHERCLIME_ENV2_SEASON_AVAILABLE));
    state.season->weights={0,0,2,0};
    assert(WriteConditionBatchV1(&seasonal,1,sizeof(seasonal),state,&out,sizeof(out))==1);
    assert(out.results[0].status==2 && !(out.snapshot.flags&AETHERCLIME_ENV2_SEASON_AVAILABLE));
    state.season.reset();
    // Limits: maximum batch, maximum valid length, overlength and embedded NUL.
    std::array<AetherclimeConditionRuleV1,32> many{};
    std::string limit="true";limit.resize(1024,' ');
    for(auto& rule:many) rule={limit.data(),1024,0};
    assert(WriteConditionBatchV1(many.data(),32,sizeof(many[0]),state,&out,sizeof(out))==1);
    for(const auto& r:out.results) assert(r.status==1);
    limit.push_back(' ');many[0]={limit.data(),1025,0};
    const char embedded[]{'t','r','u','e',0};many[1]={embedded,5,0};
    assert(WriteConditionBatchV1(many.data(),2,sizeof(many[0]),state,&out,sizeof(out))==1);
    assert(out.results[0].status==3 && out.results[0].error==2 && out.results[0].errorOffset==1024);
    assert(out.results[1].status==3);
    // Mixed order and repeated calls must not leak old result slots or error state.
    for(unsigned n=1;n<=32;++n) {
        for(unsigned i=0;i<n;++i) many[i]=rules[i%8];
        assert(WriteConditionBatchV1(many.data(),n,sizeof(many[0]),state,&out,sizeof(out))==1);
        for(unsigned i=0;i<n;++i) {
            AetherclimeConditionResultV1 single{};
            if(many[i].reserved) assert(out.results[i].error==9);
            else {WriteConditionResultV1(many[i].text,many[i].textBytes,state,{},&single,sizeof(single));assert(std::memcmp(&single,&out.results[i],sizeof(single))==0);}
        }
        for(unsigned i=n;i<32;++i) assert(out.results[i].structSize==0);
    }
}
