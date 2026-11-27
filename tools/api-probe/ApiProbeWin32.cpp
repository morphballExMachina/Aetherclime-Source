// Separately linked DLL: no AetherclimeCore, SKSE, CommonLib or engine pointers.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include "Aetherclime/ApiConsumerProbe.h"
using namespace aetherclime::probe;
extern "C" __declspec(dllexport) std::int32_t Aetherclime_RunApiProbeV1(
    ReportV1* output,std::uint32_t bytes) noexcept {
    if(!output || bytes!=sizeof(*output)) return 0;
    ApiFunctions api;
    // Borrow a handle to the already-loaded provider; never load/unload it.
    if(auto module=GetModuleHandleW(L"Aetherclime.dll")) {
        api=aetherclime::client::ResolveFunctions([module](const char* name) {
            return GetProcAddress(module,name);
        });
    }
    *output=RunChecks(api);
    return 1;
}

extern "C" __declspec(dllexport) const char* Aetherclime_ApiProbeBuildMarker() noexcept {
    return kMarker;
}
