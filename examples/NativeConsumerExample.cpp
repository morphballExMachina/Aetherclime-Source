// C++17 compile-checked example; no Core/SKSE or provider import library.
#include "Aetherclime/NativeClient.h"
namespace aetherclime::examples {
struct ClothingAdvice {
    client::CallStatus callStatus{client::CallStatus::MissingProvider};
    std::optional<bool> coldOutside; // absent = unknown, invalid, or failed call
    std::optional<bool> nighttime;
    AetherclimeEnvironmentSnapshotV2 context{};
    bool contextAvailable{false};
};
ClothingAdvice ReadClothingAdvice(const client::Client& api) noexcept {
    // Illustrative consumer policy; Aetherclime does not change any clothing.
    const std::string_view rules[]{"outdoorAirC<5", "time>=18:30 | time<06:00"};
    const auto batch=api.QueryBatch(rules);
    ClothingAdvice advice;advice.callStatus=batch.status;
    if(batch.status!=client::CallStatus::Ok) return advice;
    const auto asBool=[](std::optional<client::RuleState> state)->std::optional<bool> {
        if(state==client::RuleState::True)return true;
        if(state==client::RuleState::False)return false;
        return std::nullopt;
    };
    advice.coldOutside=asBool(batch.State(0));advice.nighttime=asBool(batch.State(1));
    advice.contextAvailable=batch.value.snapshotAvailable==1;
    advice.context=batch.value.snapshot;
    return advice;
}
} // namespace aetherclime::examples
