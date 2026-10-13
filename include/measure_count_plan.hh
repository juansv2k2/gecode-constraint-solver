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
        if (!rule.is_object()) throw std::runtime_error("Measure-mode rule entries must be objects");
        if (rule.contains("enabled")) {
            const auto& value = rule["enabled"];
            if (!value.is_boolean() && !value.is_number_integer())
                throw std::runtime_error("Rule enabled flag must be boolean or integer");
            if (!(value.is_boolean() ? value.get<bool>() : value.get<int>() != 0)) return;
        }
        const std::string type = rule.value("rule_type", std::string());
        if (rule.value("type", std::string()) == "index") {
            if (!rule.contains("voice") || !rule["voice"].is_number_integer() || rule["voice"].get<int>() < 0)
                throw std::runtime_error("Measure index rule requires a nonnegative integer voice");
            if (!rule.contains("events") || !rule["events"].is_array() || rule["events"].empty())
                throw std::runtime_error("Measure index rule requires a nonempty events array");
            for (const auto& event : rule["events"])
                if (!event.is_array() || event.size() != 3 || !event[0].is_number_integer() || event[0].get<int>() < 0 ||
                    !event[1].is_string() || !(event[2].is_null() || event[2].is_number_integer()))
                    throw std::runtime_error("Measure index events must be [position, rhythm string, pitch or null]");
            return;
        }
        const bool cadence_at_beats = type == "r-cadence" &&
            rule.contains("beat_positions") && rule["beat_positions"].is_array() &&
            rule["beat_positions"].size() == 2 && !rule.contains("positions");
        const bool melodic = type == "r-melodic-step" || type == "r-no-interval-repetition" ||
            type == "r-no-consecutive-equal" || type == "r-no-consecutive-direction";
        if (melodic && (rule.contains("indices") || rule.contains("stride") || rule.contains("offset")))
            throw std::runtime_error("measure-count local melodic rules currently apply to the whole active voice; indexed subsets are not supported");
        if (type == "r-no-interval-repetition" && rule.contains("interval_mode") &&
            rule["interval_mode"] != "up-down" && rule["interval_mode"] != "absolute")
            throw std::runtime_error("r-no-interval-repetition interval_mode must be up-down or absolute");
        const bool cross_voice = type == "r-rhythm-rhythm" || type == "r-pitch-pitch";
        const bool global = type == "r-one-voice" || type == "r-pitches-one-engine" ||
            type == "r-pitches-all-different" || type == "r-twelve-tone-voice1" ||
            type == "r-uniformity" || type == "r-rhythmic-uniformity";
        const bool reversal = type == "r-palindrome-voice" || type == "r-palindrome-voice2" || type == "r-cross-voice-retrograde-inversion";
        if (global || reversal) {
            std::string function = rule.value("constraint", std::string());
            if (rule.contains("constraint_function")) function = rule["constraint_function"].value("function", function);
            const bool uniformity = type == "r-uniformity" || type == "r-rhythmic-uniformity";
            const std::string expected = global ? (uniformity ? "equal_values" : "all_different") :
                (type == "r-cross-voice-retrograde-inversion" ? "retrograde_inversion_relationship" : "palindrome_of_engine");
            if (!function.empty() && function != expected && !(uniformity && function == "equal"))
                throw std::runtime_error("Unsupported global/transformation function in measure mode: " + function);
            if (rule.contains("indices")) {
                if (!rule["indices"].is_array()) throw std::runtime_error("Global indices must be an array");
                for (const auto& index : rule["indices"])
                    if (!index.is_number_integer() || index.get<int>() < 0)
                        throw std::runtime_error("Global indices must be nonnegative integers");
            }
        }
        const bool repetition = type == "r-repetition";
        const bool tendency = type == "r-tendency-tone";
        const bool timepoint = type == "r-timepoint-relationship";
        const bool signatures=type=="r-time-signature" || type=="r-metric-signature";
        const bool metric_hierarchy=type=="r-metric-hierarchy";
        const bool wildcard=type=="wildcard_constraint";
        if (tendency && rule.contains("heuristic") && rule["heuristic"] != false && rule["heuristic"] != 0)
            throw std::runtime_error("Tendency-tone rules support hard mode only");
        if (repetition) {
            for (const char* key : {"source_positions", "target_positions", "target_voices"}) {
                if (!rule.contains(key) || !rule[key].is_array() || rule[key].empty())
                    throw std::runtime_error(std::string("Measure repetition requires nonempty ") + key);
                for (const auto& index : rule[key])
                    if (!index.is_number_integer() || index.get<int>() < 0)
                        throw std::runtime_error("Measure repetition indices and voices must be nonnegative integers");
            }
            if (rule.contains("heuristic") && rule["heuristic"] != false && rule["heuristic"] != 0 &&
                (rule.value("relation", std::string()) == "rhythm_preserving" || rule.value("relation", std::string()) == "diatonic_sequence"))
                throw std::runtime_error("Rhythmic and diatonic repetition currently support hard mode only");
        }
        if ((global || reversal) && rule.contains("heuristic") && rule["heuristic"] != false && rule["heuristic"] != 0)
            throw std::runtime_error("Global all_different/uniformity rules currently support hard mode only");
        if (metric_hierarchy && rule.contains("heuristic") && rule["heuristic"] != false && rule["heuristic"] != 0)
            throw std::runtime_error("Measure metric-hierarchy rules currently support hard mode only");
        if (metric_hierarchy && (rule.contains("stride") || rule.contains("offset")))
            throw std::runtime_error("Measure metric-hierarchy rules support explicit event indices, not stride/offset");
        if (metric_hierarchy) {
            std::string function=rule.value("constraint",std::string());
            if (rule.contains("constraint_function"))
                function=rule["constraint_function"].value("function",function);
            auto is_supported=[](const std::string& value) {
                return value=="no_syncopation" || value=="no-syncopation" || value=="anti_syncopation" ||
                       value=="anti-syncopation" || value=="no_sync" || value=="no-sync" ||
                       value=="min_grid" || value=="min-grid" || value=="no_subdivision_below_grid" ||
                       value=="no-subdivision-below-grid" || value=="durations" || value=="equal" ||
                       value=="equal_values" || value=="tuplet_on_beat_start" || value=="tuplet-on-beat-start" ||
                       value=="hierarchical_voices" || value=="hierarchical-voices" ||
                       value=="voice_hierarchy" || value=="voice-hierarchy";
            };
            bool supported=function.empty() || is_supported(function);
            if (rule.contains("parameters") && rule["parameters"].is_array())
                for (const auto& parameter : rule["parameters"])
                    if (parameter.is_string() && is_supported(parameter.get<std::string>())) supported=true;
            if (!supported)
                throw std::runtime_error("Unsupported measure r-metric-hierarchy mode");
            bool tuplet_mode=function=="tuplet_on_beat_start" || function=="tuplet-on-beat-start";
            if (rule.contains("parameters") && rule["parameters"].is_array())
                for (const auto& parameter : rule["parameters"])
                    if (parameter.is_string() &&
                        (parameter=="tuplet_on_beat_start" || parameter=="tuplet-on-beat-start"))
                        tuplet_mode=true;
            if (rule.contains("indices") && tuplet_mode)
                throw std::runtime_error("Measure tuplet_on_beat_start applies to the full active prefix; indices are unsupported");
            if (rule.contains("indices")) {
                if (!rule["indices"].is_array()) throw std::runtime_error("Metric-hierarchy indices must be an array");
                for (const auto& index : rule["indices"])
                    if (!index.is_number_integer() || index.get<int>()<0)
                        throw std::runtime_error("Metric-hierarchy indices must be nonnegative integers");
            }
        }
        if (wildcard) {
            const std::string wildcard_type = rule.contains("wildcard_type") && rule["wildcard_type"].is_string() ?
                rule["wildcard_type"].get<std::string>() : (rule.contains("wildcard_type") ? "" : "for_all_positions");
            if (wildcard_type!="sliding_window" && wildcard_type!="for_all_positions" && wildcard_type!="for_all_voices")
                throw std::runtime_error("Measure wildcard rules support sliding_window, for_all_positions and for_all_voices only");
            if (wildcard_type!="sliding_window" && rule.contains("step_size") && rule["step_size"]!=1)
                throw std::runtime_error("Measure for_all_positions/for_all_voices wildcards use step 1; use sliding_window for step_size");
            if (!rule.contains("constraint") || !rule["constraint"].is_string())
                throw std::runtime_error("Measure wildcard requires a string constraint expression");
            if (rule.contains("pattern_offsets")) {
                if (!rule["pattern_offsets"].is_array() || rule["pattern_offsets"].empty())
                    throw std::runtime_error("Measure wildcard pattern_offsets must be a nonempty array");
                for (const auto& offset : rule["pattern_offsets"])
                    if (!offset.is_number_integer() || offset.get<int>()<0)
                        throw std::runtime_error("Measure wildcard offsets must be nonnegative integers");
            }
            if (rule.contains("step_size") && (!rule["step_size"].is_number_integer() || rule["step_size"].get<int>()<=0))
                throw std::runtime_error("Measure wildcard step_size must be a positive integer");
            if (rule.contains("indices")) {
                if (!rule["indices"].is_array()) throw std::runtime_error("Measure wildcard indices must be an array");
                for (const auto& index : rule["indices"])
                    if (!index.is_number_integer() || index.get<int>()<0)
                        throw std::runtime_error("Measure wildcard indices must be nonnegative integers");
            }
            if (rule.contains("target_voices")) {
                if (!rule["target_voices"].is_array()) throw std::runtime_error("Measure wildcard target_voices must be an array");
                for (const auto& voice : rule["target_voices"])
                    if (!voice.is_number_integer() || voice.get<int>()<0)
                        throw std::runtime_error("Measure wildcard target_voices must contain nonnegative integers");
            }
            if (rule.contains("heuristic") && rule["heuristic"]!=false && rule["heuristic"]!=0)
                throw std::runtime_error("Measure wildcard heuristics are not yet supported");
        }
        if (reversal && (rule.contains("indices") || rule.contains("stride") || rule.contains("offset")))
            throw std::runtime_error("Reversal rules operate on the full active prefix; subset selectors are unsupported");
        if (global && (rule.contains("stride") || rule.contains("offset")))
            throw std::runtime_error("Global all_different/uniformity rules use indices, not stride/offset");
        if (!cadence_at_beats && !melodic && !cross_voice && !global && !reversal && !repetition && !tendency && !timepoint && !signatures && !metric_hierarchy && !wildcard) {
            if (type == "r-cross-voice-no-unisons")
                throw std::runtime_error("r-cross-voice-no-unisons is not enforced by any solver path; use r-pitch-pitch with constraint no_unison");
            if (type == "r-perfect-fifth-intervals")
                throw std::runtime_error("r-perfect-fifth-intervals is a legacy fixed-interval chain with no measure-mode meaning; use r-pitch-pitch or dynamic_rules");
            if (type == "r-pitches-one-voice" || type == "r-index-pitches-one-voice")
                throw std::runtime_error(type + " is a legacy Lisp-era alias; use r-one-voice or an index rule");
        }
        if (!cadence_at_beats && !melodic && !cross_voice && !global && !reversal && !repetition && !tendency && !timepoint && !signatures && !metric_hierarchy && !wildcard)
            throw std::runtime_error("Rule '" +
                                    (type.empty() ? std::string("(missing rule_type)") : type) +
                                    "' is not yet active-aware in measure-count mode");
    };
    if (config.contains("rules")) {
        const auto& rules = config["rules"];
        if (rules.is_array()) for (const auto& rule : rules) validate_rule(rule);
        else if (rules.is_object()) for (auto it = rules.begin(); it != rules.end(); ++it) validate_rule(it.value());
    }
    if (config.contains("dynamic_rules") && !config["dynamic_rules"].is_null() && !config["dynamic_rules"].empty()) {
        const auto& rules=config["dynamic_rules"];
        if (!rules.is_array()) throw std::runtime_error("Measure dynamic_rules must be an array");
        for (const auto& rule : rules) {
            if (!rule.is_object()) throw std::runtime_error("Measure dynamic rule entries must be objects");
            if (rule.contains("enabled") && ((rule["enabled"].is_boolean() && !rule["enabled"].get<bool>()) ||
                (rule["enabled"].is_number_integer() && rule["enabled"].get<int>()==0) ||
                (rule["enabled"].is_string() && (rule["enabled"]=="false" || rule["enabled"]=="no" || rule["enabled"]=="off" || rule["enabled"]=="0"))))
                continue;
            if (!rule.contains("id") || !rule["id"].is_string() || !rule.contains("type") || !rule["type"].is_string() ||
                (!rule.contains("expression") && !rule.contains("constraint")) ||
                (rule.contains("expression") && !rule["expression"].is_string()) ||
                (rule.contains("constraint") && !rule["constraint"].is_string()))
                throw std::runtime_error("Measure dynamic rules require id, type and a string expression");
            if (rule["type"]!="basic_constraint")
                throw std::runtime_error("Measure dynamic rules currently support basic_constraint only");
            if (rule.contains("mode") && !rule["mode"].is_string())
                throw std::runtime_error("Measure dynamic rules require a string mode");
            if (rule.contains("mode") && rule["mode"]!="true_false" && rule["mode"]!="constraint")
                throw std::runtime_error("Measure dynamic rules support hard true_false/constraint mode only");
        }
    }
    if (config.contains("heuristic_rules") && !config["heuristic_rules"].is_null() && !config["heuristic_rules"].empty())
        throw std::runtime_error("measure-count mode does not yet support heuristic_rules");
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