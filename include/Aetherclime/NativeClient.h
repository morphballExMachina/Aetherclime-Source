#pragma once
// Optional C++17 consumer helper; header-only, no provider/Core/SKSE linkage.
// Call discovery and all provider functions on Skyrim's main/game thread.
#include "Aetherclime/PluginApi.h"
#include <array>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <optional>
#include <string_view>

namespace aetherclime::client {
enum class CallStatus {
    Ok, MissingProvider, IncompatibleProvider, MissingExport, InvalidArgument,
    Unavailable, ProviderRejected, MalformedReply, ProviderError
};
enum class RuleState : std::uint32_t { False=0, True=1, Unknown=2, Invalid=3 };
struct Functions {
    using Batch = std::int32_t (*)(const AetherclimeConditionRuleV1*,std::uint32_t,std::uint32_t,
        AetherclimeConditionBatchV1*,std::uint32_t) noexcept;
    int (*version)(){};
    std::int32_t (*v1)(AetherclimeEnvironmentSnapshotV1*,std::uint32_t) noexcept {};
    std::int32_t (*v2)(AetherclimeEnvironmentSnapshotV2*,std::uint32_t) noexcept {};
    std::int32_t (*evaluate)(const char*,std::uint32_t,AetherclimeConditionResultV1*,std::uint32_t) noexcept {};
    Batch batch{};
    std::int32_t (*v3)(AetherclimeEnvironmentSnapshotV3*,std::uint32_t) noexcept {};
};
// Resolver returns a native function pointer (e.g. GetProcAddress/FARPROC).
// Borrow an already-loaded module; this helper never loads or owns a DLL.
// All optional symbols are resolved independently. No fallback emulates a batch.
template<class Resolver> Functions ResolveFunctions(Resolver&& resolve) {
    Functions f;
    f.version=reinterpret_cast<decltype(f.version)>(resolve("Aetherclime_ApiVersion"));
    f.v1=reinterpret_cast<decltype(f.v1)>(resolve("Aetherclime_GetEnvironmentSnapshotV1"));
    f.v2=reinterpret_cast<decltype(f.v2)>(resolve("Aetherclime_GetEnvironmentSnapshotV2"));
    f.v3=reinterpret_cast<decltype(f.v3)>(resolve("Aetherclime_GetEnvironmentSnapshotV3"));
    f.evaluate=reinterpret_cast<decltype(f.evaluate)>(resolve("Aetherclime_EvaluateConditionV1"));
    f.batch=reinterpret_cast<decltype(f.batch)>(resolve("Aetherclime_EvaluateConditionsV1"));
    return f;
}
template<class T> struct Reply {
    CallStatus status{CallStatus::MissingProvider};
    T value{}; // owned value; zero on transport/contract failure
    // Deliberately no implicit bool conversion: call success is not rule truth.
};
struct QueryReply : Reply<AetherclimeConditionResultV1> {
    [[nodiscard]] std::optional<RuleState> State() const noexcept {
        if(status!=CallStatus::Ok) return std::nullopt;
        return static_cast<RuleState>(value.status);
    }
    [[nodiscard]] bool Matches() const noexcept {return State()==RuleState::True;}
};
struct BatchReply : Reply<AetherclimeConditionBatchV1> {
    [[nodiscard]] std::optional<RuleState> State(std::size_t index) const noexcept {
        if(status!=CallStatus::Ok || index>=value.ruleCount || index>=AETHERCLIME_MAX_BATCH_RULES_V1) return std::nullopt;
        return static_cast<RuleState>(value.results[index].status);
    }
    [[nodiscard]] bool Matches(std::size_t index) const noexcept {return State(index)==RuleState::True;}
};
namespace detail {
template<class T> bool Zero(const T& value) noexcept {
    const auto* p=reinterpret_cast<const unsigned char*>(&value);
    for(std::size_t i=0;i<sizeof(value);++i) if(p[i]!=0) return false;
    return true;
}
template<std::size_t N> bool Terminated(const char (&s)[N]) noexcept {return std::memchr(s,0,N)!=nullptr;}
inline bool Shape(const AetherclimeEnvironmentSnapshotV1& s) noexcept {
    return s.structSize==sizeof(s) && s.schemaVersion==1 &&
        Terminated(s.localName) && Terminated(s.regionKey) && Terminated(s.regionDisplayName);
}
inline bool Shape(const AetherclimeEnvironmentSnapshotV2& s) noexcept {
    return s.structSize==sizeof(s) && s.schemaVersion==2 && s.reserved==0 && Shape(s.legacy) &&
        Terminated(s.biomeA) && Terminated(s.biomeB) && Terminated(s.biomeStatus);
}
inline bool Shape(const AetherclimeEnvironmentSnapshotV3& s) noexcept {
    if(s.structSize!=sizeof(s)||s.schemaVersion!=3||s.reserved!=0||!Shape(s.legacy)||
       (s.flags & ~3u)) return false;
    const bool outdoor=(s.flags&AETHERCLIME_ENV3_OUTDOOR_ESTIMATE_AVAILABLE)!=0;
    const bool indoor=(s.flags&AETHERCLIME_ENV3_INDOOR_ESTIMATE_AVAILABLE)!=0;
    if(outdoor ? (s.outdoorEstimateBasis<1||s.outdoorEstimateBasis>7||
        !std::isfinite(s.outdoorEstimateC)||std::abs(s.outdoorEstimateC)>100) :
        (s.outdoorEstimateBasis!=0||s.outdoorEstimateC!=0)) return false;
    if(indoor ? (s.indoorEstimateBasis<1||s.indoorEstimateBasis>3||
        !std::isfinite(s.indoorEstimateC)||std::abs(s.indoorEstimateC)>100||
        !(s.legacy.legacy.flags&AETHERCLIME_ENV_INTERIOR)) :
        (s.indoorEstimateBasis!=0||s.indoorEstimateC!=0)) return false;
    return true;
}
inline bool Shape(const AetherclimeConditionResultV1& r,std::size_t length) noexcept {
    if(r.structSize!=sizeof(r) || r.schemaVersion!=1 || r.status>3) return false;
    if(r.status!=3) return r.error==0 && r.errorOffset==0;
    return r.error>=1 && r.error<=9 && r.errorOffset<=length;
}
template<class T> void Fail(T& out,CallStatus status) noexcept {out.value={};out.status=status;}
} // namespace detail

class Client {
public:
    Client() noexcept = default;
    explicit Client(Functions functions) noexcept : functions_(functions) {
        if(!functions_.version) return;
        try {status_=functions_.version()==1 ? CallStatus::Ok : CallStatus::IncompatibleProvider;}
        catch(...) {status_=CallStatus::ProviderError;}
    }
    template<class Resolver> static Client Discover(Resolver&& resolver) noexcept {
        try {return Client(ResolveFunctions(resolver));}
        catch(...) {Client failed;failed.status_=CallStatus::ProviderError;return failed;}
    }
    [[nodiscard]] CallStatus Status() const noexcept {return status_;}
    // Bits: version=1, V1=2, V2=4, single query=8, batch query=16, V3=32.
    // Discovery only; Status must also be Ok before using a feature.
    [[nodiscard]] std::uint32_t ExportMask() const noexcept {
        return (functions_.version?1u:0u)|(functions_.v1?2u:0u)|(functions_.v2?4u:0u)|
            (functions_.evaluate?8u:0u)|(functions_.batch?16u:0u)|(functions_.v3?32u:0u);
    }
    [[nodiscard]] Reply<AetherclimeEnvironmentSnapshotV1> SnapshotV1() const noexcept {
        return Snapshot<AetherclimeEnvironmentSnapshotV1>(functions_.v1);
    }
    [[nodiscard]] Reply<AetherclimeEnvironmentSnapshotV2> SnapshotV2() const noexcept {
        return Snapshot<AetherclimeEnvironmentSnapshotV2>(functions_.v2);
    }
    [[nodiscard]] Reply<AetherclimeEnvironmentSnapshotV3> SnapshotV3() const noexcept {
        return Snapshot<AetherclimeEnvironmentSnapshotV3>(functions_.v3);
    }
    [[nodiscard]] QueryReply Query(std::string_view rule) const noexcept {
        QueryReply out;out.status=Gate(functions_.evaluate!=nullptr);
        if(out.status!=CallStatus::Ok) return out;
        if(rule.size()>1024 || (!rule.data() && !rule.empty())) {
            out.status=CallStatus::InvalidArgument;return out;
        }
        const auto result=functions_.evaluate(rule.data(),static_cast<std::uint32_t>(rule.size()),&out.value,sizeof(out.value));
        if(result!=1) detail::Fail(out,result==0?CallStatus::ProviderRejected:CallStatus::MalformedReply);
        else if(!detail::Shape(out.value,rule.size())) detail::Fail(out,CallStatus::MalformedReply);
        return out;
    }
    [[nodiscard]] BatchReply QueryBatch(const std::string_view* rules,std::size_t count) const noexcept {
        BatchReply out;out.status=Gate(functions_.batch!=nullptr);
        if(out.status!=CallStatus::Ok) return out;
        if(!rules || count==0 || count>AETHERCLIME_MAX_BATCH_RULES_V1) {
            out.status=CallStatus::InvalidArgument;return out;
        }
        std::array<AetherclimeConditionRuleV1,AETHERCLIME_MAX_BATCH_RULES_V1> descriptors{};
        for(std::size_t i=0;i<count;++i) {
            if(rules[i].size()>1024 || (!rules[i].data() && !rules[i].empty())) {
                out.status=CallStatus::InvalidArgument;return out;
            }
            descriptors[i]={rules[i].data(),static_cast<std::uint32_t>(rules[i].size()),0};
        }
        const auto result=functions_.batch(descriptors.data(),static_cast<std::uint32_t>(count),
            sizeof(descriptors[0]),&out.value,sizeof(out.value));
        if(result!=1) {detail::Fail(out,result==0?CallStatus::ProviderRejected:CallStatus::MalformedReply);return out;}
        const auto& b=out.value;
        bool valid=b.structSize==sizeof(b) && b.schemaVersion==1 && b.ruleCount==count && b.snapshotAvailable<=1;
        valid=valid && (b.snapshotAvailable ? detail::Shape(b.snapshot) : detail::Zero(b.snapshot));
        for(std::size_t i=0;i<count;++i) valid=valid && detail::Shape(b.results[i],rules[i].size());
        for(std::size_t i=count;i<AETHERCLIME_MAX_BATCH_RULES_V1;++i) valid=valid && detail::Zero(b.results[i]);
        if(!valid) detail::Fail(out,CallStatus::MalformedReply);
        return out;
    }
    template<std::size_t N> [[nodiscard]] BatchReply QueryBatch(const std::string_view (&rules)[N]) const noexcept {
        return QueryBatch(rules,N);
    }
    template<std::size_t N> [[nodiscard]] BatchReply QueryBatch(const std::array<std::string_view,N>& rules) const noexcept {
        return QueryBatch(rules.data(),N);
    }
private:
    Functions functions_{};
    CallStatus status_{CallStatus::MissingProvider};
    CallStatus Gate(bool present) const noexcept {
        return status_!=CallStatus::Ok ? status_ : present ? CallStatus::Ok : CallStatus::MissingExport;
    }
    template<class T,class Fn> Reply<T> Snapshot(Fn fn) const noexcept {
        Reply<T> out;out.status=Gate(fn!=nullptr);
        if(out.status!=CallStatus::Ok) return out;
        const auto result=fn(&out.value,sizeof(out.value));
        if(result==0) detail::Fail(out,detail::Zero(out.value)?CallStatus::Unavailable:CallStatus::MalformedReply);
        else if(result!=1 || !detail::Shape(out.value)) detail::Fail(out,CallStatus::MalformedReply);
        return out;
    }
};
} // namespace aetherclime::client
