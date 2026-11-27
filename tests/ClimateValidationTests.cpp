#include "Aetherclime/Climate.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
using namespace aetherclime;
namespace {
template<class F> void Reject(F f) {bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
}
int main(){
    const auto defaults=DefaultConfig();
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    for(double bad:{nan,inf,-inf,61.,-81.}) {
        auto c=defaults;c.regions.at("winterhold").seasonalMeanC[2]=bad;
        Reject([&]{Climate model(c);});
    }
    for(double bad:{nan,inf,-inf,-1.,21.}) {
        auto c=defaults;c.regions.at("temperate").diurnalAmplitudeC=bad;Reject([&]{Climate model(c);});
    }
    for(double bad:{nan,inf,-inf,-31.,31.}) {
        auto c=defaults;c.regions.at("temperate").permanentSnowBiasC=bad;Reject([&]{Climate model(c);});
    }
    for(double bad:{nan,inf,-inf,-31.,61.}) {
        auto c=defaults;c.regions.at("temperate").waterMeanC=bad;Reject([&]{Climate model(c);});
    }
    auto c=defaults;c.months[6]=static_cast<Season>(255);Reject([&]{Climate model(c);});
    // Boundary values are legal. Validation includes profiles unused by this sample.
    c=defaults;c.regions.at("temperate").seasonalMeanC={-80,60,-80,60};
    c.regions.at("temperate").diurnalAmplitudeC=20;c.regions.at("temperate").permanentSnowBiasC=-30;c.regions.at("temperate").waterMeanC=60;
    Climate edge(c);assert(std::isfinite(edge.Calculate(Sample{}).airC));
    c.regions.at("temperate").permanentSnowBiasC=30;c.regions.at("temperate").waterMeanC=-30;Climate other(c);
    Climate model;
    Sample s;s.date={6,15,15,201};
    for(double bad:{nan,inf,-inf}) {
        auto v=s;v.elevationAboveReferenceM=bad;Reject([&]{model.Calculate(v);});
        v=s;v.windKmh=bad;Reject([&]{model.Calculate(v);});
        v=s;v.waterDepthM=bad;Reject([&]{model.Calculate(v);});
    }
    auto huge=s;huge.elevationAboveReferenceM=std::numeric_limits<double>::max();Reject([&]{model.Calculate(huge);});
    auto negative=s;negative.date.year=-1;Reject([&]{model.Calculate(negative);});
    // Previously year*yearLength overflowed signed int before conversion to double.
    for(int year:{0,201,6000000,std::numeric_limits<int>::max()}) {
        s.date.year=year;const auto r=model.Calculate(s);
        assert(std::isfinite(r.airC) && std::abs(r.anomalyOffsetC)<=defaults.anomalyAmplitudeC);
        assert(r.airC==model.Calculate(s).airC);
    }
    // Preserve historical clamping for finite negative height/depth/wind samples.
    s.date.year=201;const auto base=model.Calculate(s);
    s.elevationAboveReferenceM=s.waterDepthM=s.windKmh=-1;const auto low=model.Calculate(s);
    assert(low.airC==base.airC && low.waterC==base.waterC && low.feelsLikeC==base.feelsLikeC);
    // Known defaults: separate snow bias, daytime term, weather and lapse remain additive.
    c=defaults;c.anomalyAmplitudeC=0;Climate deterministic(c);
    s=Sample{};s.region="winterhold";s.date={6,15,15,201};s.weather=Weather::Clear;
    const auto r=deterministic.Calculate(s);
    assert(r.seasonalBaselineC==2.5 && r.diurnalOffsetC==3.5 && r.weatherOffsetC==1.5 && r.airC==7.5);
}
