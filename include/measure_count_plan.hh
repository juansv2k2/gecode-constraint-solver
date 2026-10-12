#ifndef MEASURE_COUNT_PLAN_HH
#define MEASURE_COUNT_PLAN_HH

#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace MeasureCountPlan {

struct TimeSignature {
    int numerator = 4;
    int denominator = 4;

    bool operator==(const TimeSignature& other) const {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

struct Group {
    int bars = 0;
    TimeSignature signature;
};

struct Bar {
    int index = 0;
    int start_tick = 0;
    int end_tick = 0;
    TimeSignature signature;
};

struct Spec {
    int measure_count = 0;
    bool solver_selects_meter = false;
    bool exact_fill = false;
    std::vector<TimeSignature> meter_domain;
    std::vector<Group> fixed_groups;
};

inline bool enabled(const nlohmann::json& config) {
    return config.contains("solution_length") && config["solution_length"].is_string() &&
           config["solution_length"] == "measure_count";
}

inline void validate_rule_support(const nlohmann::json& config) {
    if (!enabled(config)) return;
    auto validate_rule = [](const nlohmann::json& rule) {
        if (!rule.is_object()) return;
        const std::string type = rule.value("rule_type", std::string());
        const bool cadence_at_beats = type == "r-cadence" &&
            rule.contains("beat_positions") && rule["beat_positions"].is_array() &&
            rule["beat_positions"].size() == 2 && !rule.contains("positions");
        if (!cadence_at_beats)
            throw std::runtime_error("measure-count mode currently supports only r-cadence with beat_positions; rule '" +
                                    (type.empty() ? std::string("(missing rule_type)") : type) +
                                    "' still uses fixed event-index semantics");
    };
    if (config.contains("rules")) {
        const auto& rules = config["rules"];
        if (rules.is_array()) for (const auto& rule : rules) validate_rule(rule);
        else if (rules.is_object()) for (auto it = rules.begin(); it != rules.end(); ++it) validate_rule(it.value());
    }
    for (const char* key : {"dynamic_rules", "heuristic_rules"})
        if (config.contains(key) && !config[key].is_null() && !config[key].empty())
            throw std::runtime_error(std::string("measure-count mode does not yet support ") + key +
                                     "; dynamic and heuristic rules require active-event semantics");
}

inline TimeSignature parse_signature(const nlohmann::json& value, const std::string& context) {
    if (!value.is_string()) throw std::runtime_error(context + " must be a time-signature string such as '4/4'");
    const std::string text = value.get<std::string>();
    const auto slash = text.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= text.size() ||
        text.find('/', slash + 1) != std::string::npos)
        throw std::runtime_error(context + " is not a valid time signature: " + text);
    size_t consumed_numerator = 0;
    size_t consumed_denominator = 0;
    const int numerator = std::stoi(text.substr(0, slash), &consumed_numerator);
    const int denominator = std::stoi(text.substr(slash + 1), &consumed_denominator);
    if (consumed_numerator != slash || consumed_denominator != text.size() - slash - 1 ||
        numerator <= 0 || denominator <= 0 || numerator > 1000 || denominator > 1000)
        throw std::runtime_error(context + " is not a positive time signature: " + text);
    return {numerator, denominator};
}

inline Spec parse(const nlohmann::json& config) {
    if (!enabled(config)) throw std::runtime_error("solution_length must be 'measure_count' for measure-count mode");
    if (!config.contains("measure_count") || !config["measure_count"].is_number_integer())
        throw std::runtime_error("measure_count mode requires a positive integer measure_count");

    Spec spec;
    spec.measure_count = config["measure_count"].get<int>();
    if (spec.measure_count <= 0 || spec.measure_count > 1000)
        throw std::runtime_error("measure_count must be between 1 and 1000");

    if (config.contains("measure_fill_policy")) {
        if (!config["measure_fill_policy"].is_string())
            throw std::runtime_error("measure_fill_policy must be 'pad' or 'exact'");
        const auto policy = config["measure_fill_policy"].get<std::string>();
        if (policy != "pad" && policy != "exact")
            throw std::runtime_error("measure_fill_policy must be 'pad' or 'exact'");
        spec.exact_fill = policy == "exact";
    }

    if (config.contains("meter")) {
        const auto& meter = config["meter"];
        if (!meter.is_object() || !meter.contains("time_signatures") ||
            !meter["time_signatures"].is_array() || meter["time_signatures"].empty())
            throw std::runtime_error("measure-count meter requires a nonempty meter.time_signatures array");
        for (size_t index = 0; index < meter["time_signatures"].size(); ++index) {
            const auto signature = parse_signature(meter["time_signatures"][index],
                "meter.time_signatures[" + std::to_string(index) + "]");
            if (std::find(spec.meter_domain.begin(), spec.meter_domain.end(), signature) == spec.meter_domain.end())
                spec.meter_domain.push_back(signature);
        }
    }

    if (config.contains("measure_plan")) {
        const auto& plan = config["measure_plan"];
        if (!plan.is_array() || plan.empty()) throw std::runtime_error("measure_plan must be a nonempty array");
        int covered_bars = 0;
        for (size_t index = 0; index < plan.size(); ++index) {
            const auto& item = plan[index];
            if (!item.is_object() || !item.contains("bars") || !item["bars"].is_number_integer())
                throw std::runtime_error("measure_plan[" + std::to_string(index) + "] requires integer bars and time_signature");
            Group group;
            group.bars = item["bars"].get<int>();
            if (group.bars <= 0 || covered_bars > spec.measure_count - group.bars)
                throw std::runtime_error("measure_plan bar counts must be positive and sum to measure_count");
            if (!item.contains("time_signature"))
                throw std::runtime_error("measure_plan[" + std::to_string(index) + "] requires time_signature");
            group.signature = parse_signature(item["time_signature"],
                "measure_plan[" + std::to_string(index) + "].time_signature");
            if (!spec.meter_domain.empty() &&
                std::find(spec.meter_domain.begin(), spec.meter_domain.end(), group.signature) == spec.meter_domain.end())
                throw std::runtime_error("measure_plan signature is not present in meter.time_signatures");
            covered_bars += group.bars;
            spec.fixed_groups.push_back(group);
        }
        if (covered_bars != spec.measure_count)
            throw std::runtime_error("measure_plan bar counts must sum to measure_count");
    } else {
        if (spec.meter_domain.empty())
            throw std::runtime_error("measure-count mode requires measure_plan or meter.time_signatures");
        spec.solver_selects_meter = true;
    }
    return spec;
}

inline int required_rhythm_base(const Spec& spec, int current_base) {
    auto checked_lcm = [](int left, int right) {
        if (left <= 0 || right <= 0)
            throw std::runtime_error("measure-count rhythm base overflow");
        const long long result = static_cast<long long>(left / std::gcd(left, right)) * right;
        if (result > std::numeric_limits<int>::max())
            throw std::runtime_error("measure-count rhythm base overflow");
        return static_cast<int>(result);
    };
    int base = checked_lcm(std::max(1, current_base), 4);
    auto include_denominator = [&](int denominator) {
        base = checked_lcm(base, denominator);
    };
    for (const auto& signature : spec.meter_domain) include_denominator(signature.denominator);
    for (const auto& group : spec.fixed_groups) include_denominator(group.signature.denominator);
    return base;
}

inline int fixed_horizon_ticks(const Spec& spec, int rhythm_base) {
    if (spec.solver_selects_meter)
        throw std::runtime_error("fixed horizon is unavailable when the solver selects each bar's meter");
    long long ticks = 0;
    for (const auto& group : spec.fixed_groups) {
        const long long numerator = static_cast<long long>(group.bars) * group.signature.numerator * rhythm_base;
        if (numerator % group.signature.denominator != 0)
            throw std::runtime_error("measure plan is not exactly representable in the rhythm tick grid");
        ticks += numerator / group.signature.denominator;
        if (ticks > std::numeric_limits<int>::max())
            throw std::runtime_error("measure plan exceeds the supported tick horizon");
    }
    return static_cast<int>(ticks);
}

inline std::vector<Bar> expand_fixed_schedule(const Spec& spec, int rhythm_base) {
    if (spec.solver_selects_meter)
        throw std::runtime_error("fixed schedule is unavailable when the solver selects each bar's meter");
    std::vector<Bar> bars;
    int current_tick = 0;
    for (const auto& group : spec.fixed_groups) {
        const long long numerator = static_cast<long long>(group.signature.numerator) * rhythm_base;
        if (numerator % group.signature.denominator != 0)
            throw std::runtime_error("measure plan is not exactly representable in the rhythm tick grid");
        const int duration = static_cast<int>(numerator / group.signature.denominator);
        for (int count = 0; count < group.bars; ++count) {
            bars.push_back({static_cast<int>(bars.size()), current_tick, current_tick + duration, group.signature});
            current_tick += duration;
        }
    }
    if (static_cast<int>(bars.size()) != spec.measure_count || current_tick != fixed_horizon_ticks(spec, rhythm_base))
        throw std::runtime_error("expanded measure plan does not match measure_count or horizon");
    return bars;
}

}

#endif