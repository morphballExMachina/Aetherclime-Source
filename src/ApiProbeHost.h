#pragma once
#include "Aetherclime/ApiConsumerProbe.h"
namespace aetherclime::probe {
enum class HostStatus { Loaded, MissingProvider, ProviderPathFailed, LoadFailed, MissingProbeExport, InvalidReport, Exception };
struct HostResult {HostStatus status{HostStatus::Exception};std::uint32_t windowsError{};ReportV1 report{};};
HostResult RunExternalApiProbe() noexcept;
inline const char* HostStatusName(HostStatus s) noexcept {
    switch(s) {
    case HostStatus::Loaded:return "loaded";
    case HostStatus::MissingProvider:return "provider-not-loaded";
    case HostStatus::ProviderPathFailed:return "provider-path-failed";
    case HostStatus::LoadFailed:return "probe-load-failed";
    case HostStatus::MissingProbeExport:return "probe-export-missing";
    case HostStatus::InvalidReport:return "invalid-probe-report";
    default:return "host-exception";
    }
}
}
