#include "Aetherclime/ConditionQuery.h"

#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

namespace aetherclime {
namespace {
using S = ConditionStatus;
using E = ConditionError;
S And(S a, S b) noexcept {
    if (a == S::False || b == S::False) return S::False;
    return a == S::True && b == S::True ? S::True : S::Unknown;
}
S Or(S a, S b) noexcept {
    if (a == S::True || b == S::True) return S::True;
    return a == S::False && b == S::False ? S::False : S::Unknown;
}
S Not(S a) noexcept { return a == S::Unknown ? a : a == S::True ? S::False : S::True; }
enum class Kind { End, Atom, And, Or, Not, Left, Right, Eq, Ne, Lt, Le, Gt, Ge };
enum class Type { Number, Text, Boolean, Clock, Biome };
struct Token { Kind kind{Kind::End}; std::string_view text; std::size_t offset{0}; };
struct Value {
    Type type{Type::Number}; bool available{false}; double number{0}; std::string_view text;
};
bool Space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool Separator(char c) noexcept {
    return Space(c) || c == '&' || c == '|' || c == '!' || c == '(' || c == ')' || c == '=' || c == '<' || c == '>';
}
bool Key(std::string_view s) noexcept {
    if (s.empty()) return false;
    for (char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
bool Number(std::string_view s, double& out) noexcept {
    // Decimal only: no exponents, hexadecimal, NaN, infinity, or locale syntax.
    if (s.empty()) return false;
    std::size_t i = s[0] == '-' ? 1 : 0;
    bool digit = false, point = false;
    for (; i < s.size(); ++i) {
        if (s[i] >= '0' && s[i] <= '9') digit = true;
        else if (s[i] == '.' && !point) point = true;
        else return false;
    }
    if (!digit) return false;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size() && std::isfinite(out);
}
bool Clock(std::string_view s, double& out) noexcept {
    if ((s.size() != 5 && s.size() != 8) || s[2] != ':' || (s.size() == 8 && s[5] != ':')) return false;
    for (std::size_t i = 0; i < s.size(); ++i) if (i != 2 && i != 5 && (s[i] < '0' || s[i] > '9')) return false;
    const int h = (s[0]-'0')*10 + s[1]-'0';
    const int m = (s[3]-'0')*10 + s[4]-'0';
    const int sec = s.size() == 8 ? (s[6]-'0')*10 + s[7]-'0' : 0;
    if (h > 23 || m > 59 || sec > 59) return false;
    out = h*3600 + m*60 + sec;
    return true;
}
std::string_view WeatherKey(Weather w) noexcept {
    switch (w) {
    case Weather::Clear: return "clear";
    case Weather::Cloudy: return "cloudy";
    case Weather::Rain: return "rain";
    case Weather::Storm: return "storm";
    case Weather::Snow: return "snow";
    case Weather::Blizzard: return "blizzard";
    default: return {};
    }
}
std::string_view SeasonKey(Season s) noexcept {
    switch (s) {
    case Season::Winter: return "winter";
    case Season::Spring: return "spring";
    case Season::Summer: return "summer";
    case Season::Autumn: return "autumn";
    }
    return {};
}
class Parser {
public:
    Parser(std::string_view input, const EnvironmentSnapshot& s, std::optional<Season> season) noexcept
        : input_(input), state_(s), season_(season) {}
    ConditionResult Run() noexcept {
        if (input_.empty()) return {S::Invalid, E::Empty, 0};
        if (input_.size() > kConditionMaxBytes) return {S::Invalid, E::TooLong, static_cast<std::uint32_t>(kConditionMaxBytes)};
        Next();
        if (!failed_ && token_.kind == Kind::End) Fail(E::Empty, 0);
        const auto result = Expression(0);
        if (!failed_ && token_.kind != Kind::End) Fail(E::UnexpectedToken, token_.offset);
        return failed_ ? error_ : ConditionResult{result, E::None, 0};
    }
private:
    std::string_view input_; const EnvironmentSnapshot& state_; std::optional<Season> season_;
    std::size_t position_{0}, tokens_{0}; Token token_{}; bool failed_{false}; ConditionResult error_{};
    void Fail(E e, std::size_t at) noexcept {
        if (!failed_) error_ = {S::Invalid, e, static_cast<std::uint32_t>(at)};
        failed_ = true;
    }
    void Next() noexcept {
        while (position_ < input_.size() && Space(input_[position_])) ++position_;
        token_ = {Kind::End, {}, position_};
        if (failed_ || position_ == input_.size()) return;
        if (++tokens_ > kConditionMaxTokens) { Fail(E::LimitExceeded, position_); return; }
        const auto start = position_++;
        const char c = input_[start];
        switch (c) {
        case '&': token_.kind = Kind::And; break;
        case '|': token_.kind = Kind::Or; break;
        case '(': token_.kind = Kind::Left; break;
        case ')': token_.kind = Kind::Right; break;
        case '!': token_.kind = Kind::Not; break;
        case '=': token_.kind = Kind::Eq; break;
        case '<': token_.kind = Kind::Lt; break;
        case '>': token_.kind = Kind::Gt; break;
        default:
            token_.kind = Kind::Atom;
            while (position_ < input_.size() && !Separator(input_[position_])) ++position_;
            break;
        }
        if (position_ < input_.size() && input_[position_] == '=' && (c == '!' || c == '<' || c == '>' || c == '=')) {
            ++position_;
            token_.kind = c == '!' ? Kind::Ne : c == '<' ? Kind::Le : c == '>' ? Kind::Ge : Kind::Eq;
        }
        token_.text = input_.substr(start, position_ - start);
    }
    S Expression(unsigned depth) noexcept {
        auto a = Conjunction(depth);
        while (!failed_ && token_.kind == Kind::Or) {
            Next(); const auto b = Conjunction(depth); a = Or(a,b);
        }
        return a;
    }
    S Conjunction(unsigned depth) noexcept {
        auto a = Unary(depth);
        while (!failed_ && token_.kind == Kind::And) {
            Next(); const auto b = Unary(depth); a = And(a,b);
        }
        return a;
    }
    S Unary(unsigned depth) noexcept {
        if (failed_) return S::Unknown;
        if (token_.kind == Kind::Not || token_.kind == Kind::Left) {
            if (depth >= kConditionMaxDepth) { Fail(E::LimitExceeded, token_.offset); return S::Unknown; }
            const auto kind = token_.kind;
            Next();
            if (kind == Kind::Not) return Not(Unary(depth+1));
            const auto a = Expression(depth+1);
            if (!failed_ && token_.kind != Kind::Right) Fail(E::UnexpectedToken, token_.offset);
            if (!failed_) Next();
            return a;
        }
        return Comparison();
    }
    Value Field(std::string_view name, std::size_t at, std::string_view argument = {}) noexcept {
        const auto& s = state_;
        const bool dateValid = s.date.month >= 0 && s.date.month <= 11 && s.date.day >= 1 && s.date.day <= 40 &&
            std::isfinite(s.date.hour) && s.date.hour >= 0 && s.date.hour < 24;
        const auto numeric = [&](double n, bool ok = true) { return Value{Type::Number, s.available && ok && std::isfinite(n), n, {}}; };
        if (name == "year") return numeric(s.date.year, dateValid);
        if (name == "month") return numeric(s.date.month, dateValid);
        if (name == "day") return numeric(s.date.day, dateValid);
        if (name == "hour") return numeric(s.date.hour, dateValid);
        if (name == "time") return {Type::Clock, s.available && dateValid, dateValid ? std::floor(s.date.hour*3600) : 0, {}};
        const bool biomeValid = !s.interior && ValidBiomeIdentity(s.biome);
        if (name == "biome") return {Type::Biome, s.available && biomeValid && s.biome.evidence!=BiomeEvidence::Nearby, 0, {}};
        if (name == "biomeAvailable") return {Type::Boolean, s.available, biomeValid ? 1.0 : 0.0, {}};
        if (name == "biomeEvidence") return {Type::Text, s.available, 0, biomeValid ? BiomeEvidenceName(s.biome.evidence) : "none"};
        if (name == "biomeWeight") {
            const auto share = biomeValid ? BiomeShare(s.biome,argument) : std::nullopt;
            return numeric(share.value_or(0), share.has_value());
        }
        if (name == "seasonWeight") {
            const auto season=ParseSeason(argument);
            if (!season || (argument!="winter" && argument!="spring" && argument!="summer" && argument!="autumn")) {
                Fail(E::InvalidLiteral, at); return {};
            }
            const bool valid=s.season && ValidSeasonState(*s.season) && dateValid;
            return numeric(valid ? s.season->weights[static_cast<unsigned>(*season)] : 0,valid);
        }
        if (name == "previewAirC") return numeric(s.previewAirC.value_or(0),biomeValid && s.previewAirC.has_value());
        // Alpha-49 explicit lower-confidence fields; trusted legacy fields
        // retain their original tri-state semantics and are not promoted.
        if (name == "outdoorEstimateC") return numeric(s.estimatedOutdoorAirC.value_or(0),s.estimatedOutdoorAirC.has_value());
        if (name == "indoorEstimateC") return numeric(s.estimatedIndoorAmbientC.value_or(0),s.interior && s.estimatedIndoorAmbientC.has_value());
        if (name == "outdoorAirC") return numeric(s.outdoorAirC, s.trustedRegion);
        if (name == "outdoorFeelsLikeC") return numeric(s.outdoorFeelsLikeC, s.trustedRegion);
        if (name == "localAmbientC") return numeric(s.localAmbientC.value_or(0), s.localAmbientC.has_value());
        if (name == "interior") return {Type::Boolean, s.available, s.interior ? 1.0 : 0.0, {}};
        if (name == "trustedRegion") return {Type::Boolean, s.available, s.trustedRegion ? 1.0 : 0.0, {}};
        if (name == "provisionalRegion") return {Type::Boolean, s.available, s.provisionalRegion ? 1.0 : 0.0, {}};
        if (name == "region") return {Type::Text, s.available && !s.regionKey.empty(), 0, s.regionKey};
        if (name == "season") {
            const auto key = season_ ? SeasonKey(*season_) : s.season && ValidSeasonState(*s.season) ? SeasonKey(s.season->official) : std::string_view{};
            return {Type::Text, s.available && dateValid && !key.empty(), 0, key};
        }
        if (name == "weather") {
            const auto key = WeatherKey(s.weather);
            const bool verified = s.verifiedOutdoorWeatherFormId != 0 &&
                (s.weatherProvenance == OutdoorWeatherProvenance::DirectExteriorSky ||
                 s.weatherProvenance == OutdoorWeatherProvenance::CachedVerifiedExterior);
            return {Type::Text, s.available && verified && !key.empty(), 0, key};
        }
        Fail(E::UnknownField, at); return {};
    }
    S Comparison() noexcept {
        if (failed_) return S::Unknown;
        if (token_.kind != Kind::Atom) { Fail(E::UnexpectedToken, token_.offset); return S::Unknown; }
        const auto field = token_;
        Next();
        if (field.text == "true" || field.text == "false") return field.text == "true" ? S::True : S::False;
        std::string_view argument;
        if (field.text == "biomeWeight" || field.text == "seasonWeight") {
            if (token_.kind != Kind::Left) {Fail(E::UnexpectedToken,token_.offset);return S::Unknown;}
            Next();
            if (token_.kind != Kind::Atom || !Key(token_.text)) {Fail(E::InvalidLiteral,token_.offset);return S::Unknown;}
            argument=token_.text;
            Next();
            if (token_.kind != Kind::Right) {Fail(E::UnexpectedToken,token_.offset);return S::Unknown;}
            Next();
        }
        const auto value = Field(field.text, field.offset, argument);
        if (failed_) return S::Unknown;
        const auto op = token_;
        if (op.kind != Kind::Eq && op.kind != Kind::Ne && op.kind != Kind::Lt && op.kind != Kind::Le && op.kind != Kind::Gt && op.kind != Kind::Ge) {
            Fail(E::InvalidOperator, op.offset); return S::Unknown;
        }
        if ((value.type == Type::Text || value.type == Type::Boolean || value.type == Type::Biome) && op.kind != Kind::Eq && op.kind != Kind::Ne) {
            Fail(E::InvalidOperator, op.offset); return S::Unknown;
        }
        Next(); const auto literal = token_;
        if (literal.kind != Kind::Atom) { Fail(E::InvalidLiteral, literal.offset); return S::Unknown; }
        double n = 0; bool valid = true;
        if (value.type == Type::Text || value.type == Type::Biome) {
            valid = Key(literal.text);
            if (field.text == "biomeEvidence") valid = literal.text == "none" || literal.text == "unique" || literal.text == "overlap" || literal.text == "overlap-edge" || literal.text == "nearby";
            if (field.text == "season") valid = literal.text == "winter" || literal.text == "spring" || literal.text == "summer" || literal.text == "autumn";
            if (field.text == "weather") valid = literal.text == "clear" || literal.text == "cloudy" || literal.text == "rain" || literal.text == "storm" || literal.text == "snow" || literal.text == "blizzard";
        } else if (value.type == Type::Boolean) {
            valid = literal.text == "true" || literal.text == "false"; n = literal.text == "true" ? 1 : 0;
        } else if (value.type == Type::Clock) valid = Clock(literal.text, n);
        else {
            valid = Number(literal.text, n);
            if (field.text == "biomeWeight" || field.text == "seasonWeight") valid = valid && n >= 0 && n <= 1;
            if (field.text == "month") valid = valid && n >= 0 && n <= 11 && n == std::floor(n);
            if (field.text == "day") valid = valid && n >= 1 && n <= 40 && n == std::floor(n);
            if (field.text == "year") valid = valid && n >= 0 && n <= std::numeric_limits<int>::max() && n == std::floor(n);
            if (field.text == "hour") valid = valid && n >= 0 && n <= 24;
        }
        if (!valid) { Fail(E::InvalidLiteral, literal.offset); return S::Unknown; }
        Next();
        if (!value.available) return S::Unknown;
        bool match = false;
        if (value.type == Type::Biome) {
            const bool present=BiomeShare(state_.biome,literal.text).value_or(0)>=0.5;
            match=op.kind==Kind::Eq ? present : !present;
        }
        else if (value.type == Type::Text) match = op.kind == Kind::Eq ? value.text == literal.text : value.text != literal.text;
        else switch (op.kind) {
        case Kind::Eq: match = value.number == n; break;
        case Kind::Ne: match = value.number != n; break;
        case Kind::Lt: match = value.number < n; break;
        case Kind::Le: match = value.number <= n; break;
        case Kind::Gt: match = value.number > n; break;
        case Kind::Ge: match = value.number >= n; break;
        default: break;
        }
        return match ? S::True : S::False;
    }
};
} // namespace
std::string_view ConditionStatusName(ConditionStatus s) noexcept {
    switch (s) {
    case S::False: return "false";
    case S::True: return "true";
    case S::Unknown: return "unknown";
    case S::Invalid: return "invalid";
    }
    return "invalid";
}
ConditionResult EvaluateCondition(std::string_view rule, const EnvironmentSnapshot& state, std::optional<Season> season) noexcept {
    return Parser(rule, state, season).Run();
}
static_assert(std::is_standard_layout_v<AetherclimeConditionResultV1>);
static_assert(std::is_trivially_copyable_v<AetherclimeConditionResultV1>);
static_assert(sizeof(AetherclimeConditionResultV1) == 20);
std::int32_t WriteConditionResultV1(const char* rule, std::uint32_t ruleBytes,
    const EnvironmentSnapshot& state, std::optional<Season> season,
    AetherclimeConditionResultV1* output, std::uint32_t outputBytes) noexcept {
    if (!output || outputBytes != sizeof(*output)) return 0;
    const auto result = !rule ? ConditionResult{S::Invalid, E::Empty, 0} :
        ruleBytes > kConditionMaxBytes ? ConditionResult{S::Invalid, E::TooLong, static_cast<std::uint32_t>(kConditionMaxBytes)} :
        EvaluateCondition(std::string_view(rule, ruleBytes), state, season);
    *output = {sizeof(*output), 1, static_cast<std::uint32_t>(result.status),
        static_cast<std::uint32_t>(result.error), result.errorOffset};
    return 1;
}
} // namespace aetherclime
