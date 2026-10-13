#ifndef CROSS_VOICE_RULES_HH
#define CROSS_VOICE_RULES_HH

#include "dynamic_rule_compiler.hh"
#include <cmath>
#include <numeric>
#include <set>

namespace CrossVoiceRules {

struct Params {
    std::string id;
    std::string mode;
    int first_voice = 0;
    int second_voice = 1;
    bool heuristic = false;
    std::vector<int> indices;
    int stride = 0;
    int offset = 0;
    std::vector<int> intervals;
    int ratio_numerator = 2;
    int ratio_denominator = 1;
};

inline Params resolve(const nlohmann::json& rule, int voices, int capacity, bool rhythm) {
    Params result;
    result.id = rule.value("id", rhythm ? std::string("r-rhythm-rhythm") : std::string("r-pitch-pitch"));
    result.mode = rule.value("constraint", rhythm ? std::string("isorhythm") : std::string("no_unison"));
    const nlohmann::json* parameters = rule.contains("parameters") ? &rule["parameters"] : nullptr;
    if (rule.contains("constraint_function")) {
        const auto& function = rule["constraint_function"];
        if (!function.is_object()) throw DynamicRules::RuleCompileException(result.id + ": constraint_function must be an object");
        if (!rule.contains("constraint")) result.mode = function.value("function", result.mode);
        if (!parameters && function.contains("parameters")) parameters = &function["parameters"];
    }
    const auto& targets = rule.at("target_voices");
    if (!targets.is_array() || targets.size() != 2 || !targets[0].is_number_integer() || !targets[1].is_number_integer())
        throw DynamicRules::RuleCompileException(result.id + ": exactly two integer target_voices required");
    result.first_voice = targets[0].get<int>();
    result.second_voice = targets[1].get<int>();
    if (result.first_voice < 0 || result.first_voice >= voices || result.second_voice < 0 || result.second_voice >= voices)
        throw DynamicRules::RuleCompileException(result.id + ": voice out of range");
    if (rule.contains("heuristic")) {
        const auto& value = rule["heuristic"];
        if (!value.is_boolean() && !value.is_number_integer())
            throw DynamicRules::RuleCompileException(result.id + ": heuristic must be boolean or integer");
        result.heuristic = value.is_boolean() ? value.get<bool>() : value.get<int>() != 0;
    }
    if (rule.contains("indices")) {
        if (!rule["indices"].is_array()) throw DynamicRules::RuleCompileException(result.id + ": indices must be an array");
        for (const auto& value : rule["indices"]) {
            if (!value.is_number_integer()) throw DynamicRules::RuleCompileException(result.id + ": indices must be integers");
            const int index = value.get<int>();
            if (index < 0 || index >= capacity) throw DynamicRules::RuleCompileException(result.id + ": index out of range");
            result.indices.push_back(index);
        }
    }
    if (rule.contains("stride")) {
        if (rhythm || !rule["stride"].is_number_integer() || rule["stride"].get<int>() < 1)
            throw DynamicRules::RuleCompileException(result.id + ": positive integer stride is supported for pitch rules only");
        result.stride = rule["stride"].get<int>();
    }
    if (rule.contains("offset")) {
        if (rhythm || !rule["offset"].is_number_integer() || rule["offset"].get<int>() < 0 || result.stride == 0)
            throw DynamicRules::RuleCompileException(result.id + ": nonnegative offset requires pitch stride");
        result.offset = rule["offset"].get<int>();
    }
    if (rhythm) {
        static const std::set<std::string> modes{
            "isorhythm", "abs_isorhythm", "augmentation", "diminution", "no_simultaneous_rests", "rest_complement"};
        if (!modes.count(result.mode)) throw DynamicRules::RuleCompileException(result.id + ": unknown rhythm mode " + result.mode);
        if ((result.mode == "augmentation" || result.mode == "diminution") && parameters) {
            if (!parameters->is_array())
                throw DynamicRules::RuleCompileException(result.id + ": parameters must contain a positive numeric ratio");
            if (parameters->empty()) return result;
            if (!(*parameters)[0].is_number())
                throw DynamicRules::RuleCompileException(result.id + ": parameters must contain a positive numeric ratio");
            const double ratio = (*parameters)[0].get<double>();
            if (!std::isfinite(ratio) || ratio <= 0 || ratio > 1000000)
                throw DynamicRules::RuleCompileException(result.id + ": unsupported ratio");
            bool represented = false;
            for (int denominator = 1; denominator <= 64; ++denominator) {
                const double scaled = ratio * denominator;
                if (scaled >= 1 && std::abs(scaled - std::round(scaled)) < 1e-9) {
                    const int numerator = static_cast<int>(std::round(scaled));
                    const int divisor = std::gcd(numerator, denominator);
                    result.ratio_numerator = numerator / divisor;
                    result.ratio_denominator = denominator / divisor;
                    represented = true;
                    break;
                }
            }
            if (!represented) throw DynamicRules::RuleCompileException(result.id + ": ratio denominator must be representable within 64");
        }
    } else {
        static const std::set<std::string> modes{
            "no_unison", "same_pitch", "voice_above", "voice_below", "exact_interval", "min_interval",
            "max_interval", "interval_range", "interval_class", "no_consecutive_fifths", "no_consecutive_octaves",
            "no_parallel_motion", "contrary_motion"};
        if (!modes.count(result.mode)) throw DynamicRules::RuleCompileException(result.id + ": unknown pitch mode " + result.mode);
        if (parameters) {
            if (!parameters->is_array()) throw DynamicRules::RuleCompileException(result.id + ": parameters must be an array");
            for (const auto& value : *parameters) {
                if (!value.is_number()) throw DynamicRules::RuleCompileException(result.id + ": interval parameters must be integers");
                const double number = value.get<double>();
                if (!std::isfinite(number) || number != std::round(number) || std::abs(number) > Gecode::Int::Limits::max)
                    throw DynamicRules::RuleCompileException(result.id + ": interval parameter out of range");
                result.intervals.push_back(static_cast<int>(number));
            }
        }
        if ((result.mode == "exact_interval" || result.mode == "min_interval" || result.mode == "max_interval" ||
             result.mode == "interval_class") && result.intervals.empty())
            throw DynamicRules::RuleCompileException(result.id + ": interval parameters required");
        if (result.mode == "interval_range" && (result.intervals.size() != 2 || result.intervals[0] < 0 ||
            result.intervals[1] < result.intervals[0]))
            throw DynamicRules::RuleCompileException(result.id + ": interval_range requires nonnegative ordered bounds");
        if ((result.mode == "min_interval" || result.mode == "max_interval") && result.intervals[0] < 0)
            throw DynamicRules::RuleCompileException(result.id + ": interval distance must be nonnegative");
        if (result.mode == "interval_class")
            for (int interval : result.intervals)
                if (interval < 0 || interval > 11) throw DynamicRules::RuleCompileException(result.id + ": interval classes must be 0..11");
    }
    return result;
}

inline bool selected_index(const Params& params, int index) {
    if (params.stride > 0) return index >= params.offset && (index - params.offset) % params.stride == 0;
    return params.indices.empty() || std::find(params.indices.begin(), params.indices.end(), index) != params.indices.end();
}

inline Gecode::BoolVar active_window(DynamicRules::ConstraintContext& ctx, const Params& params,
                                    int first_event, int width, bool sounded_only) {
    Gecode::BoolVar guard(*ctx.space, 1, 1);
    for (int voice : {params.first_voice, params.second_voice}) {
        for (int offset = 0; offset < width; ++offset) {
            const int index = voice * ctx.sequence_length + first_event + offset;
            if (!ctx.rhythm_vars || index < 0 || index >= ctx.rhythm_vars->size())
                throw DynamicRules::RuleCompileException(params.id + ": rhythm variables out of range");
            if (params.stride == 0 && !params.indices.empty())
                Gecode::rel(*ctx.space, ctx.space->get_active_vars()[index], Gecode::IRT_EQ, 1);
            guard = Gecode::expr(*ctx.space, guard && (sounded_only
                ? Gecode::BoolExpr((*ctx.rhythm_vars)[index] > 0) : Gecode::BoolExpr((*ctx.rhythm_vars)[index] != 0)));
        }
    }
    return guard;
}

inline bool rhythm_matches(const Params& params, int first, int second) {
    if (params.mode == "isorhythm") return first == second;
    if (params.mode == "abs_isorhythm") return std::abs(first) == std::abs(second);
    if (params.mode == "augmentation")
        return static_cast<long long>(params.ratio_denominator) * first == static_cast<long long>(params.ratio_numerator) * second;
    if (params.mode == "diminution")
        return static_cast<long long>(params.ratio_numerator) * first == static_cast<long long>(params.ratio_denominator) * second;
    if (params.mode == "no_simultaneous_rests") return first > 0 || second > 0;
    return (first > 0) != (second > 0);
}

inline std::unique_ptr<DynamicRules::CompiledConstraint> compile_rhythm(
        const nlohmann::json& rule, int voices, int capacity) {
    const auto params = resolve(rule, voices, capacity, true);
    auto compiled = std::make_unique<DynamicRules::CompiledConstraint>(params.id, "r-rhythm-rhythm: " + params.mode);
    if (params.heuristic) {
        compiled->is_heuristic = true;
        compiled->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
        compiled->heuristic_variable_type = "rhythm";
        compiled->applies_to_voices = {params.first_voice, params.second_voice};
        compiled->score_candidate = [params](const DynamicRules::ConstraintContext& ctx,
                                             const DynamicRules::HeuristicCandidateContext& candidate) {
            if (!ctx.rhythm_vars || candidate.position < 0 || candidate.position >= ctx.sequence_length ||
                !selected_index(params, candidate.position) || candidate.candidate_value == 0 ||
                (candidate.voice != params.first_voice && candidate.voice != params.second_voice)) return 0.0;
            const int other_voice = candidate.voice == params.first_voice ? params.second_voice : params.first_voice;
            const int index = other_voice * ctx.sequence_length + candidate.position;
            if (index >= ctx.rhythm_vars->size()) return 0.0;
            const auto& other = (*ctx.rhythm_vars)[index];
            if (!other.assigned() || other.val() == 0) return 0.0;
            const int first = candidate.voice == params.first_voice ? candidate.candidate_value : other.val();
            const int second = candidate.voice == params.second_voice ? candidate.candidate_value : other.val();
            return rhythm_matches(params, first, second) ? 1.0 : -1.0;
        };
    } else {
        compiled->post_constraint = [params](DynamicRules::ConstraintContext& ctx) {
            if (!ctx.space || !ctx.space->is_measure_count_mode() || !ctx.rhythm_vars)
                throw DynamicRules::RuleCompileException(params.id + ": measure-mode rhythm context required");
            for (int event = 0; event < ctx.sequence_length; ++event) {
                if (!selected_index(params, event)) continue;
                const auto guard = active_window(ctx, params, event, 1, false);
                Gecode::IntVar first = (*ctx.rhythm_vars)[params.first_voice * ctx.sequence_length + event];
                Gecode::IntVar second = (*ctx.rhythm_vars)[params.second_voice * ctx.sequence_length + event];
                if (params.mode == "isorhythm") {
                    Gecode::rel(*ctx.space, first, Gecode::IRT_EQ, second, Gecode::Reify(guard, Gecode::RM_IMP));
                } else if (params.mode == "abs_isorhythm") {
                    Gecode::IntVar first_abs = Gecode::expr(*ctx.space, Gecode::abs(first));
                    Gecode::IntVar second_abs = Gecode::expr(*ctx.space, Gecode::abs(second));
                    Gecode::rel(*ctx.space, first_abs, Gecode::IRT_EQ, second_abs, Gecode::Reify(guard, Gecode::RM_IMP));
                } else if (params.mode == "augmentation" || params.mode == "diminution") {
                    const int first_coefficient = params.mode == "augmentation" ? params.ratio_denominator : params.ratio_numerator;
                    const int second_coefficient = params.mode == "augmentation" ? params.ratio_numerator : params.ratio_denominator;
                    Gecode::linear(*ctx.space, Gecode::IntArgs({first_coefficient, -second_coefficient}),
                        Gecode::IntVarArgs({first, second}), Gecode::IRT_EQ, 0, Gecode::Reify(guard, Gecode::RM_IMP));
                } else {
                    Gecode::BoolVar first_note = Gecode::expr(*ctx.space, first > 0);
                    Gecode::BoolVar second_note = Gecode::expr(*ctx.space, second > 0);
                    Gecode::BoolVar match = Gecode::expr(*ctx.space, params.mode == "rest_complement"
                        ? Gecode::BoolExpr(first_note != second_note) : Gecode::BoolExpr(first_note || second_note));
                    Gecode::rel(*ctx.space, match, Gecode::IRT_EQ, 1, Gecode::Reify(guard, Gecode::RM_IMP));
                }
            }
        };
    }
    return compiled;
}

inline bool consecutive_pitch_mode(const std::string& mode) {
    return mode == "no_consecutive_fifths" || mode == "no_consecutive_octaves" ||
        mode == "no_parallel_motion" || mode == "contrary_motion";
}

inline int interval_class(int first, int second) {
    return ((first - second) % 12 + 12) % 12;
}

inline bool pitch_matches(const Params& params, int first, int second, int next_first, int next_second) {
    const int difference = first - second;
    const int distance = std::abs(difference);
    if (params.mode == "no_unison") return first != second;
    if (params.mode == "same_pitch") return first == second;
    if (params.mode == "voice_above") return first > second;
    if (params.mode == "voice_below") return first < second;
    if (params.mode == "exact_interval") return difference == params.intervals[0];
    if (params.mode == "min_interval") return distance >= params.intervals[0];
    if (params.mode == "max_interval") return distance <= params.intervals[0];
    if (params.mode == "interval_range") return distance >= params.intervals[0] && distance <= params.intervals[1];
    if (params.mode == "interval_class")
        return std::find(params.intervals.begin(), params.intervals.end(), interval_class(first, second)) != params.intervals.end();
    if (params.mode == "no_consecutive_fifths" || params.mode == "no_consecutive_octaves") {
        const int target = params.mode == "no_consecutive_fifths" ? 7 : 0;
        return interval_class(first, second) != target || interval_class(next_first, next_second) != target;
    }
    const int first_direction = next_first - first, second_direction = next_second - second;
    if (params.mode == "contrary_motion") return first_direction * second_direction < 0;
    return !((first_direction > 0 && second_direction > 0) || (first_direction < 0 && second_direction < 0));
}

inline Gecode::IntVar pitch_class_var(DynamicRules::ConstraintContext& ctx, Gecode::IntVar first, Gecode::IntVar second) {
    Gecode::IntVar shifted = Gecode::expr(*ctx.space, first - second + 132);
    Gecode::IntVar twelve(*ctx.space, 12, 12), pitch_class(*ctx.space, 0, 11);
    Gecode::mod(*ctx.space, shifted, twelve, pitch_class);
    return pitch_class;
}

inline Gecode::BoolExpr pitch_predicate(DynamicRules::ConstraintContext& ctx, const Params& params,
        Gecode::IntVar first, Gecode::IntVar second, Gecode::IntVar next_first, Gecode::IntVar next_second) {
    if (params.mode == "no_unison") return first != second;
    if (params.mode == "same_pitch") return first == second;
    if (params.mode == "voice_above") return first > second;
    if (params.mode == "voice_below") return first < second;
    if (params.mode == "exact_interval") return (first - second) == params.intervals[0];
    if (params.mode == "min_interval") return Gecode::abs(first - second) >= params.intervals[0];
    if (params.mode == "max_interval") return Gecode::abs(first - second) <= params.intervals[0];
    if (params.mode == "interval_range")
        return (Gecode::abs(first - second) >= params.intervals[0]) && (Gecode::abs(first - second) <= params.intervals[1]);
    if (params.mode == "interval_class") {
        Gecode::IntVar pitch_class = pitch_class_var(ctx, first, second);
        Gecode::BoolExpr allowed = pitch_class == params.intervals[0];
        for (size_t index = 1; index < params.intervals.size(); ++index) allowed = allowed || (pitch_class == params.intervals[index]);
        return allowed;
    }
    if (params.mode == "no_consecutive_fifths" || params.mode == "no_consecutive_octaves") {
        const int target = params.mode == "no_consecutive_fifths" ? 7 : 0;
        return (pitch_class_var(ctx, first, second) != target) || (pitch_class_var(ctx, next_first, next_second) != target);
    }
    Gecode::IntVar first_direction = Gecode::expr(*ctx.space, next_first - first);
    Gecode::IntVar second_direction = Gecode::expr(*ctx.space, next_second - second);
    if (params.mode == "contrary_motion")
        return ((first_direction > 0) && (second_direction < 0)) || ((first_direction < 0) && (second_direction > 0));
    return ((first_direction <= 0) || (second_direction <= 0)) && ((first_direction >= 0) || (second_direction >= 0));
}

inline std::unique_ptr<DynamicRules::CompiledConstraint> compile_pitch(
        const nlohmann::json& rule, int voices, int capacity) {
    const auto params = resolve(rule, voices, capacity, false);
    const int width = consecutive_pitch_mode(params.mode) ? 2 : 1;
    if (params.stride == 0)
        for (int index : params.indices)
            if (index + width > capacity) throw DynamicRules::RuleCompileException(params.id + ": explicit comparison window out of range");
    auto compiled = std::make_unique<DynamicRules::CompiledConstraint>(params.id, "r-pitch-pitch: " + params.mode);
    if (params.heuristic) {
        compiled->is_heuristic = true;
        compiled->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
        compiled->heuristic_variable_type = "pitch";
        compiled->applies_to_voices = {params.first_voice, params.second_voice};
        compiled->score_candidate = [params, width](const DynamicRules::ConstraintContext& ctx,
                                                  const DynamicRules::HeuristicCandidateContext& candidate) {
            const int start = candidate.position - width + 1;
            if (!ctx.pitch_vars || !ctx.rhythm_vars || start < 0 || candidate.position >= ctx.sequence_length ||
                candidate.candidate_value < 0 || !selected_index(params, start) ||
                (candidate.voice != params.first_voice && candidate.voice != params.second_voice)) return 0.0;
            int pitches[2][2]{};
            for (int selected_voice = 0; selected_voice < 2; ++selected_voice) {
                const int voice = selected_voice == 0 ? params.first_voice : params.second_voice;
                for (int offset = 0; offset < width; ++offset) {
                    const int event = start + offset, index = voice * ctx.sequence_length + event;
                    if (index >= ctx.pitch_vars->size() || index >= ctx.rhythm_vars->size()) return 0.0;
                    const auto& rhythm = (*ctx.rhythm_vars)[index];
                    if (!rhythm.assigned() || rhythm.val() <= 0) return 0.0;
                    if (voice == candidate.voice && event == candidate.position) pitches[selected_voice][offset] = candidate.candidate_value;
                    else {
                        const auto& pitch = (*ctx.pitch_vars)[index];
                        if (!pitch.assigned()) return 0.0;
                        pitches[selected_voice][offset] = pitch.val();
                    }
                }
            }
            return pitch_matches(params, pitches[0][0], pitches[1][0], pitches[0][width - 1], pitches[1][width - 1]) ? 1.0 : -1.0;
        };
    } else {
        compiled->post_constraint = [params, width](DynamicRules::ConstraintContext& ctx) {
            if (!ctx.space || !ctx.space->is_measure_count_mode() || !ctx.pitch_vars || !ctx.rhythm_vars)
                throw DynamicRules::RuleCompileException(params.id + ": measure-mode pitch and rhythm context required");
            for (int event = 0; event + width <= ctx.sequence_length; ++event) {
                if (!selected_index(params, event)) continue;
                const auto guard = active_window(ctx, params, event, width, true);
                Gecode::IntVar first = (*ctx.pitch_vars)[params.first_voice * ctx.sequence_length + event];
                Gecode::IntVar second = (*ctx.pitch_vars)[params.second_voice * ctx.sequence_length + event];
                Gecode::IntVar next_first = (*ctx.pitch_vars)[params.first_voice * ctx.sequence_length + event + width - 1];
                Gecode::IntVar next_second = (*ctx.pitch_vars)[params.second_voice * ctx.sequence_length + event + width - 1];
                Gecode::BoolVar matches = Gecode::expr(*ctx.space, pitch_predicate(ctx, params, first, second, next_first, next_second));
                Gecode::rel(*ctx.space, matches, Gecode::IRT_EQ, 1, Gecode::Reify(guard, Gecode::RM_IMP));
            }
        };
    }
    return compiled;
}

}
#endif