#include "Aetherclime/TravelDiscontinuity.h"
#include <cassert>
#include <limits>
using namespace aetherclime;
int main() {
    TravelDiscontinuity d;Date now;now.month=7;now.day=17;now.hour=14;
    assert(d.Observe({},now)==TravelEvent::NoExterior);
    assert(d.Observe(ClimateWorldPoint{0x3C,500,300,0},now)==TravelEvent::InitialExterior);
    assert(d.Observe(ClimateWorldPoint{0x3C,1000,300,0},now)==TravelEvent::Normal);
    assert(d.Observe(ClimateWorldPoint{0x3C,100000,300,0},now)==TravelEvent::FarJump);
    assert(d.Observe(ClimateWorldPoint{0x3C,100000,300,0},now)==TravelEvent::Normal);
    assert(d.Observe(ClimateWorldPoint{0x99,0,0,0},now)==TravelEvent::WorldspaceChanged);
    assert(d.Observe({},now)==TravelEvent::NoExterior);
    now.hour=13;
    assert(d.Observe(ClimateWorldPoint{0x99,0,0,0},now)==TravelEvent::CalendarRolledBack);
    assert(d.Observe(ClimateWorldPoint{0x99,std::numeric_limits<double>::infinity(),0,0},now)==TravelEvent::NoExterior);
    d.Reset();assert(d.Observe(ClimateWorldPoint{0x3C,0,0,0},now)==TravelEvent::InitialExterior);
    return 0;
}
