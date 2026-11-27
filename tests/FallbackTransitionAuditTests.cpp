#include "Aetherclime/FallbackTransitionAudit.h"
#include <cassert>
#include <limits>
using namespace aetherclime;
int main() {
    FallbackTransitionAudit audit;
    FallbackTransitionSample trusted{false,OutdoorEstimateBasis::TrustedRegional,2.0,0x3C,1,1000.0,1000.0};
    auto cold=trusted;cold.basis=OutdoorEstimateBasis::NearbyClimateReference;cold.estimateC=3.0;
    auto broad=cold;broad.basis=OutdoorEstimateBasis::BroadTamriel;broad.estimateC=16.0;
    assert(!audit.Observe(trusted).Report());
    auto r=audit.Observe(cold);assert(r.Report() && r.reason==FallbackReportReason::BasisChanged && r.reportNumber==1);
    assert(!audit.Observe(cold).Report());
    r=audit.Observe(broad);assert(r.Report() && r.reason==FallbackReportReason::BasisChanged);
    broad.xRaw=1200.0;broad.estimateC=3.0;
    r=audit.Observe(broad);assert(r.Report() && r.reason==FallbackReportReason::NearbyTemperatureStep);
    auto far=broad;far.xRaw=15000;far.estimateC=30.0;
    assert(!audit.Observe(far).Report()); // never treat distant travel as nearby step
    assert(!audit.Observe(trusted).Report());
    r=audit.Observe(cold);assert(r.Report() && r.reason==FallbackReportReason::BasisChanged);
    auto interior=cold;interior.interior=true;
    assert(!audit.Observe(interior).Report());
    r=audit.Observe(cold);assert(r.Report() && r.reason==FallbackReportReason::Entered);
    FallbackTransitionSample invalid=cold;invalid.estimateC=std::numeric_limits<double>::quiet_NaN();
    assert(!audit.Observe(invalid).Report());
    assert(audit.Observe(cold).reason==FallbackReportReason::Entered);
    audit.Reset();assert(audit.Reports()==0);
    // Excessive transitions remain bounded to 12 reports per loaded session.
    for(unsigned i=0;i<30;++i){
        assert(!audit.Observe(trusted).Report());
        auto decision=audit.Observe(cold);
        assert(decision.Report()==(i<FallbackTransitionAudit::kMaxReports));
    }
    assert(audit.Reports()==FallbackTransitionAudit::kMaxReports);
}
