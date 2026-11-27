#include "Aetherclime/ConditionQuery.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <string>
using namespace aetherclime;
using S = ConditionStatus;
using E = ConditionError;
int main() {
    EnvironmentSnapshot s;
    s.available = true; s.trustedRegion = true; s.regionKey = "winterhold";
    s.date = {6,15,18.5,201}; s.outdoorAirC = -2.5; s.outdoorFeelsLikeC = -4;
    s.localAmbientC = 12; s.weather = Weather::Snow;
    s.weatherProvenance = OutdoorWeatherProvenance::DirectExteriorSky;
    s.verifiedOutdoorWeatherFormId = 42;
    const auto check = [&](std::string_view rule, S expected) {
        const auto r = EvaluateCondition(rule, s, Season::Summer);
        if (r.status != expected) std::cerr << rule << " status=" << static_cast<int>(r.status) << " error=" << static_cast<int>(r.error) << '\n';
        assert(r.status == expected);
        if (expected != S::Invalid) assert(r.error == E::None && r.errorOffset == 0);
        else assert(r.error != E::None && r.errorOffset <= rule.size());
    };
    check("season=summer & month=6 & day=15 & year=201 & time=18:30:00",S::True);
    check("month==6 & hour=18.5 & time==18:30",S::True);
    check("time>=18:30 | time<06:00",S::True);
    check("!(interior=true) & region=winterhold & weather=snow & outdoorAirC<0",S::True);
    check("outdoorAirC<=-2.5 & outdoorAirC>=-2.5 & outdoorAirC!=-2 & outdoorFeelsLikeC<outdoorAirC",S::Invalid);
    check("outdoorAirC<=-2.5 & outdoorAirC>=-2.5 & outdoorAirC!=-2 & outdoorFeelsLikeC<-3",S::True);
    check("localAmbientC>10 & trustedRegion=true & provisionalRegion=false",S::True);
    check("true | false & false", S::True);
    check("(true | false) & false", S::False);
    check("!false & !!true",S::True);
    check("\t month = 6\r\n & season != winter ",S::True);
    // Clock compares integer seconds by flooring the snapshot, hour retains precision.
    s.date.hour = 18.5 + 0.75/3600;
    check("time=18:30:00 & hour>18.5",S::True);
    s.date.hour=23+59.0/60+59.5/3600;
    check("time=23:59:59 & time>23:59:58",S::True);
    s.date.hour=0;
    check("time=00:00 & (time>=18:30 | time<06:00)",S::True);
    s.date.hour=12;
    check("time>=18:30 | time<06:00",S::False);
    s.date.hour=6;
    check("time>=18:30 | time<06:00",S::False);
    s.date.month=11; s.date.day=40;
    check("month=11 & day=40",S::True); // configurable calendar length, not a hardcoded Earth calendar
    s.date.month=12;
    check("month=11",S::Unknown);
    s.date={6,15,18.5,201};
    const std::array bad{
        "", "   ", "()", "true &", "|true", "(true", "true)", "true false", "true && false", "true || false",
        "month", "month=", "month==", "month=12", "month=-1", "month=6.5", "day=0", "day=41", "year=-1",
        "year=2.5", "hour=24.1", "time=24:00", "time=18:60", "time=18:30:60", "time=6:00", "time=18.5",
        "time=18:30:00.5", "time=18:30x", "hour=18:30", "outdoorAirC=nan", "outdoorAirC=inf", "outdoorAirC=1e2",
        "outdoorAirC=0x10", "outdoorAirC=1..2", "outdoorAirC=+2", "month=6=6", "season>summer", "season=banana",
        "weather=unknown", "weather=snwo", "interior=1", "interior>true", "region='winterhold'", "region=winterhold;",
        "Month=6", "hold=winterhold", "unsupportedBiomeField=snow", "month>=6 AND day=15", "false & missing=1", "true | missing=1"
    };
    for (auto rule : bad) check(rule, S::Invalid);
    auto error = EvaluateCondition("false & missing=1", s);
    assert(error.error == E::UnknownField && error.errorOffset == 8);
    std::string embedded("month=6\0",8); check(embedded,S::Invalid);
    // Three-valued logic, including both operand orders. Invalid always dominates.
    s.trustedRegion=false;
    const std::array<std::string,3> terms{"false","true","outdoorAirC<0"};
    const S andTable[3][3]{{S::False,S::False,S::False},{S::False,S::True,S::Unknown},{S::False,S::Unknown,S::Unknown}};
    const S orTable[3][3]{{S::False,S::True,S::Unknown},{S::True,S::True,S::True},{S::Unknown,S::True,S::Unknown}};
    for (int a=0;a<3;++a) for(int b=0;b<3;++b) {
        check("("+terms[a]+") & ("+terms[b]+")",andTable[a][b]);
        check("("+terms[a]+") | ("+terms[b]+")",orTable[a][b]);
    }
    check("!(outdoorAirC<0)",S::Unknown);
    check("trustedRegion=false & region=winterhold",S::True);
    check("outdoorEstimateC<0",S::Unknown);
    s.estimatedOutdoorAirC=-9.5;s.outdoorEstimateBasis=OutdoorEstimateBasis::BroadTamriel;
    check("outdoorEstimateC<-9 & outdoorAirC<0",S::Unknown);
    check("outdoorEstimateC<-9",S::True);
    s.interior=true;s.estimatedIndoorAmbientC=12.5;s.indoorEstimateBasis=IndoorEstimateBasis::Generic;
    check("indoorEstimateC>=12 & indoorEstimateC<13",S::True);
    s.interior=false;check("indoorEstimateC>0",S::Unknown);
    s.estimatedOutdoorAirC.reset();s.estimatedIndoorAmbientC.reset();
    s.localAmbientC.reset(); check("localAmbientC>0",S::Unknown);
    s.verifiedOutdoorWeatherFormId=0; check("weather=snow",S::Unknown);
    s.verifiedOutdoorWeatherFormId=42; s.weatherProvenance=OutdoorWeatherProvenance::InteriorSkyUnverified;
    check("weather=snow",S::Unknown);
    s.trustedRegion=true; s.outdoorAirC=std::numeric_limits<double>::quiet_NaN();
    check("outdoorAirC!=0",S::Unknown);
    s.available=false;
    check("month=6 & interior=false",S::Unknown);
    check("true | month=6",S::True);
    check("false & month=6",S::False);
    check("true | month=12",S::Invalid);
    check("true | " + std::string(17,'!') + "false",S::Invalid);
    check(std::string(16,'(')+"true"+std::string(16,')'),S::True);
    check(std::string(17,'(')+"true"+std::string(17,')'),S::Invalid);
    check(std::string(16,'!')+"true",S::True);
    check(std::string(17,'!')+"true",S::Invalid);
    check("true"+std::string(1020,' '),S::True);
    check("true"+std::string(1021,' '),S::Invalid);
    std::string chain="true";
    for (int i=1;i<128;++i) chain += "|true";
    check(chain,S::True); check(chain+"|true",S::Invalid);
    // Active calendar determines official season, not a hardcoded month mapping.
    s.available=true; s.date={6,15,12,201};
    auto cfg=DefaultConfig(); cfg.months[6]=Season::Winter;
    Climate climate(cfg);
    assert(EvaluateCondition("season=winter & month=6",s,climate.GetSeason(s.date).official).status==S::True);
    assert(EvaluateCondition("season=winter",s).status==S::Unknown);
    // ABI buffer/size/null handling; success means result written, not match.
    AetherclimeConditionResultV1 out{};
    assert(WriteConditionResultV1("true",4,s,Season::Summer,&out,sizeof(out))==1);
    assert(out.structSize==20 && out.schemaVersion==1 && out.status==1);
    assert(WriteConditionResultV1("false",5,s,{},&out,sizeof(out))==1 && out.status==0);
    out.status=77;
    assert(WriteConditionResultV1("true",4,s,{},&out,sizeof(out)-1)==0 && out.status==77);
    assert(WriteConditionResultV1("true",4,s,{},nullptr,sizeof(out))==0);
    assert(WriteConditionResultV1(nullptr,4,s,{},&out,sizeof(out))==1 && out.status==3);
    assert(WriteConditionResultV1("true",1025,s,{},&out,sizeof(out))==1 && out.error==2); // do not read oversized buffer
    assert(WriteConditionResultV1("true",0,s,{},&out,sizeof(out))==1 && out.error==1);
    // Queries cannot mutate the existing V1 public data.
    s.outdoorAirC=-2;
    AetherclimeEnvironmentSnapshotV1 before{},after{};
    assert(ToPublicSnapshotV1(s,before));
    check("month=6 & outdoorAirC<0",S::True);
    assert(ToPublicSnapshotV1(s,after)); assert(std::memcmp(&before,&after,sizeof(before))==0);
    // Deterministic bounded malformed-input stress, also run under ASan/UBSan.
    std::mt19937 random(41);
    constexpr std::string_view alphabet="abcmonthtime0123456789!&|()=<>:-. \0";
    for (int i=0;i<10000;++i) {
        std::string input;
        const auto len=random()%1100;
        for (unsigned j=0;j<len;++j) input += alphabet[random()%alphabet.size()];
        const auto a=EvaluateCondition(input,s), b=EvaluateCondition(input,s);
        assert(a.status==b.status && a.error==b.error && a.errorOffset==b.errorOffset);
        assert(a.status!=S::Invalid || a.errorOffset<=input.size());
    }
    std::cout << "Condition queries: grammar, calendar/time, truth tables, bounds, provenance and ABI passed\n";
}
