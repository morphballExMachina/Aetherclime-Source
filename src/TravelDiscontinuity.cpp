#include "Aetherclime/TravelDiscontinuity.h"
#include <cmath>
namespace aetherclime {
std::string_view TravelEventName(TravelEvent e) noexcept {
    switch(e) {case TravelEvent::NoExterior:return "no-exterior-position";
    case TravelEvent::InitialExterior:return "initial-exterior";
    case TravelEvent::Normal:return "normal";
    case TravelEvent::FarJump:return "far-jump";
    case TravelEvent::WorldspaceChanged:return "worldspace-changed";
    case TravelEvent::CalendarRolledBack:return "calendar-rolled-back";}
    return "unknown";
}
TravelEvent TravelDiscontinuity::Observe(const std::optional<ClimateWorldPoint>& p,const Date& d) noexcept {
    if(!p || !p->worldspaceFormId || !std::isfinite(p->xRaw) || !std::isfinite(p->yRaw) ||
       std::abs(p->xRaw)>10000000 || std::abs(p->yRaw)>10000000 ||
       d.month<0 || d.month>11 || d.day<1 || d.day>40 || !std::isfinite(d.hour))
        return TravelEvent::NoExterior;
    TravelEvent result=TravelEvent::InitialExterior;
    if(last_) {
        if(last_->worldspaceFormId!=p->worldspaceFormId)result=TravelEvent::WorldspaceChanged;
        else if(lastDate_ && (d.year<lastDate_->year ||
          (d.year==lastDate_->year && (d.month<lastDate_->month ||
          (d.month==lastDate_->month && (d.day<lastDate_->day ||
          (d.day==lastDate_->day && d.hour+1e-6<lastDate_->hour))))))) result=TravelEvent::CalendarRolledBack;
        else if(std::hypot(last_->xRaw-p->xRaw,last_->yRaw-p->yRaw)>kJumpDistanceRaw)result=TravelEvent::FarJump;
        else result=TravelEvent::Normal;
    }
    last_=*p;lastDate_=d;return result;
}
} // namespace aetherclime
