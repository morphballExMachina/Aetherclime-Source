// Compile without CommonLib's PCH: this adapter uses only the Win32 loader and
// a fixed report ABI. No global DLL search-path changes or game-engine access.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "ApiProbeHost.h"
#include <array>
#include <cstring>
#include <filesystem>

namespace aetherclime::probe {
HostResult RunExternalApiProbe() noexcept {
    HostResult result;
    try {
        const auto provider=GetModuleHandleW(L"Aetherclime.dll");
        if(!provider) {result.status=HostStatus::MissingProvider;result.windowsError=GetLastError();return result;}
        std::array<wchar_t,32768> name{};
        const auto size=GetModuleFileNameW(provider,name.data(),static_cast<DWORD>(name.size()));
        if(!size || size>=name.size()) {result.status=HostStatus::ProviderPathFailed;result.windowsError=GetLastError();return result;}
        const auto path=std::filesystem::path(name.data()).parent_path()/L"Aetherclime"/L"Diagnostics"/L"AetherclimeApiProbe.dll";
        const auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!module) {result.status=HostStatus::LoadFailed;result.windowsError=GetLastError();return result;}
        struct Release {HMODULE handle;~Release(){FreeLibrary(handle);}} release{module};
        const auto run=reinterpret_cast<RunProbe>(GetProcAddress(module,"Aetherclime_RunApiProbeV1"));
        if(!run) {result.status=HostStatus::MissingProbeExport;result.windowsError=GetLastError();return result;}
        if(run(&result.report,sizeof(result.report))!=1) {result.status=HostStatus::InvalidReport;return result;}
        const auto& r=result.report;
        std::uint32_t passed=0,failed=0,skipped=0;
        for(auto s:r.checks) {if(s==Passed) ++passed;else if(s==Failed) ++failed;else if(s==Skipped) ++skipped;}
        if(r.structSize!=sizeof(r) || r.schemaVersion!=1 || r.completed>1 || r.resolvedMask>63 ||
           std::memcmp(r.marker,kMarker,sizeof(kMarker))!=0 || passed+failed+skipped!=kCheckCount ||
           r.passed!=passed || r.failed!=failed || r.skipped!=skipped) {result.status=HostStatus::InvalidReport;return result;}
        result.status=HostStatus::Loaded;
    } catch(...) {result.status=HostStatus::Exception;}
    return result;
}
}
