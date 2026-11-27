#include "Aetherclime/ConditionQuery.h"
#include <cstring>
#include <type_traits>
namespace aetherclime {
static_assert(std::is_standard_layout_v<AetherclimeConditionBatchV1> &&
              std::is_trivially_copyable_v<AetherclimeConditionBatchV1>);
bool ValidConditionBatchEnvelope(const AetherclimeConditionRuleV1* rules, std::uint32_t count,
    std::uint32_t descriptorBytes, AetherclimeConditionBatchV1* out, std::uint32_t bytes) noexcept {
    return rules && out && count>0 && count<=AETHERCLIME_MAX_BATCH_RULES_V1 &&
        descriptorBytes==sizeof(*rules) && bytes==sizeof(*out);
}
std::int32_t WriteConditionBatchV1(const AetherclimeConditionRuleV1* rules, std::uint32_t count,
    std::uint32_t descriptorBytes, const EnvironmentSnapshot& state,
    AetherclimeConditionBatchV1* out, std::uint32_t bytes, bool captureFailed) noexcept {
    if(!ValidConditionBatchEnvelope(rules,count,descriptorBytes,out,bytes)) return 0;
    std::memset(out,0,sizeof(*out));
    out->structSize=sizeof(*out);out->schemaVersion=1;out->ruleCount=count;
    if(!captureFailed) out->snapshotAvailable=ToPublicSnapshotV2(state,out->snapshot)?1u:0u;
    for(std::uint32_t i=0;i<count;++i) {
        auto& result=out->results[i];
        if(captureFailed || rules[i].reserved) {
            result={sizeof(result),1,static_cast<std::uint32_t>(ConditionStatus::Invalid),
                static_cast<std::uint32_t>(captureFailed?ConditionError::InternalError:ConditionError::InvalidArgument),0};
        } else {
            WriteConditionResultV1(rules[i].text,rules[i].textBytes,state,std::nullopt,&result,sizeof(result));
        }
    }
    return 1;
}
} // namespace aetherclime
