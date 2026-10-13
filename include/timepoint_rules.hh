#ifndef TIMEPOINT_RULES_HH
#define TIMEPOINT_RULES_HH

#include "tendency_tone_rules.hh"
#include "measure_count_plan.hh"
#include <regex>

namespace TimepointRules {

inline int parse_tick(const nlohmann::json& value, int base, int horizon) {
    long long ticks = -1;
    if (value.is_number_integer()) ticks = TendencyToneRules::selector_integer(value);
    else if (value.is_string()) {
        static const std::regex pattern("^([0-9]{1,9})(?:/([1-9][0-9]{0,8})|\\.([0-9]{1,6}))?q$");
        std::smatch match;
        const std::string text = value.get<std::string>();
        if (!std::regex_match(text,match,pattern) || base < 4 || base%4 != 0)
            throw std::runtime_error("Timepoint must be an exactly representable quarter-note token such as 3/2q");
        const long long whole = std::stoll(match[1].str());
        const int quarter = base/4;
        if (match[2].matched) {
            const long long denominator = std::stoll(match[2].str());
            const long long numerator = whole*quarter;
            if (numerator%denominator != 0) throw std::runtime_error("Timepoint does not align with the tick grid");
            ticks=numerator/denominator;
        } else {
            ticks=whole*quarter;
            if (match[3].matched) {
                long long scale=1;
                for (size_t digit=0; digit<match[3].str().size(); ++digit) scale*=10;
                const long long numerator=std::stoll(match[3].str())*quarter;
                if (numerator%scale != 0) throw std::runtime_error("Timepoint does not align with the tick grid");
                ticks+=numerator/scale;
            }
        }
    }
    if (ticks<0 || ticks>=horizon) throw std::runtime_error("Timepoint lies outside the bar-plan horizon");
    return static_cast<int>(ticks);
}

inline std::pair<bool,std::vector<int>> interval_predicate(const nlohmann::json& rule) {
    std::string function=rule.value("function",std::string("interval_in_set"));
    const nlohmann::json* parameters=rule.contains("parameters") ? &rule["parameters"] : nullptr;
    if (rule.contains("constraint_function")) {
        function=rule["constraint_function"].value("function",function);
        if (!parameters && rule["constraint_function"].contains("parameters")) parameters=&rule["constraint_function"]["parameters"];
    }
    if (rule.contains("constraint")) {
        const std::string constraint=rule["constraint"].get<std::string>();
        if (constraint=="interval_in_set" || constraint=="interval_not_in_set") function=constraint;
        else {
            if (std::regex_search(constraint,std::regex("voice\\[[0-9]+\\]"))) {
                if (!rule.contains("target_voices") || !rule["target_voices"].is_array() || rule["target_voices"].size()!=2 ||
                    !rule["target_voices"][0].is_number_integer() || !rule["target_voices"][1].is_number_integer())
                    throw std::runtime_error("Literal voice references require the matching 0/1 pair; use v1/v2 for other pairs");
                std::set<int> targets{TendencyToneRules::selector_integer(rule["target_voices"][0]),TendencyToneRules::selector_integer(rule["target_voices"][1])};
                if (targets!=std::set<int>{0,1}) throw std::runtime_error("Literal voice references cannot be rebound to another target pair");
            }
            const std::string normalized=std::regex_replace(std::regex_replace(constraint,
                std::regex("voice\\[(v1|a)\\]"),"voice[0]"),std::regex("voice\\[(v2|b)\\]"),"voice[1]");
            auto ast=DynamicRules::RuleExpressionParser::parse_rule({{"id","timepoint-predicate"},{"type","basic_constraint"},{"mode","true_false"},{"expression",normalized}});
            const auto& root=*ast->root;
            if ((root.type!=DynamicRules::ASTNodeType::IN && root.type!=DynamicRules::ASTNodeType::NOT_IN) || root.children.size()!=2 ||
                root.children[0]->type!=DynamicRules::ASTNodeType::ABS || root.children[0]->children.size()!=1 ||
                root.children[1]->type!=DynamicRules::ASTNodeType::CONSTANT || !std::holds_alternative<std::vector<int>>(root.children[1]->value))
                throw std::runtime_error("Measure timepoint expressions currently support abs(pitch difference) in/not_in a literal interval set");
            const auto* function_node=dynamic_cast<const DynamicRules::FunctionNode*>(root.children[0].get());
            if (function_node && function_node->function_name!="abs") throw std::runtime_error("Timepoint expressions require abs, not an unknown function");
            const auto& difference=*root.children[0]->children[0];
            if (difference.type!=DynamicRules::ASTNodeType::MINUS || difference.children.size()!=2)
                throw std::runtime_error("Timepoint expression must compare a pair of sounding pitches");
            std::set<int> referenced;
            for (const auto& operand : difference.children) {
                if (operand->type!=DynamicRules::ASTNodeType::VOICE_ACCESS) throw std::runtime_error("Timepoint difference operands must be voice pitches");
                const auto& voice=static_cast<const DynamicRules::VoiceAccessNode&>(*operand);
                if (voice.property!="pitch" || !voice.position_expr || voice.position_expr->type!=DynamicRules::ASTNodeType::VARIABLE ||
                    static_cast<const DynamicRules::VariableNode&>(*voice.position_expr).variable_name!="i")
                    throw std::runtime_error("Timepoint expressions use pitch[i]; event offsets require generic rule migration");
                referenced.insert(voice.voice_index);
            }
            if (referenced!=std::set<int>{0,1}) throw std::runtime_error("Timepoint expression uses pair placeholders v1/v2 (or a/b)");
            auto values=root.children[1]->get_value<std::vector<int>>();
            if (values.empty() || std::any_of(values.begin(),values.end(),[](int value){return value<0;}))
                throw std::runtime_error("Timepoint interval set must contain nonnegative distances");
            return {root.type==DynamicRules::ASTNodeType::NOT_IN,values};
        }
    }
    if (function!="interval_in_set" && function!="interval_not_in_set") throw std::runtime_error("Unsupported measure timepoint function");
    if (!parameters || !parameters->is_array() || parameters->empty()) throw std::runtime_error("Timepoint interval parameters required");
    std::vector<int> intervals;
    for (const auto& value : *parameters) {
        intervals.push_back(TendencyToneRules::selector_integer(value));
    }
    return {function=="interval_not_in_set",intervals};
}

inline void validate_signatures(const nlohmann::json& rule, const MusicalConstraintSolver::SolverConfig& config) {
    const auto bars=MeasureCountPlan::expand_fixed_schedule(config.measure_count_plan,config.rhythm_base);
    if (rule.contains("heuristic") && rule["heuristic"]!=false && rule["heuristic"]!=0)
        throw std::runtime_error("Time-signature plan constraints support hard mode only");
    std::string function=rule.value("constraint",std::string("equal_values"));
    const nlohmann::json* parameters=rule.contains("parameters") ? &rule["parameters"] : nullptr;
    if (rule.contains("constraint_function")) {
        function=rule["constraint_function"].value("function",function);
        if (!parameters && rule["constraint_function"].contains("parameters")) parameters=&rule["constraint_function"]["parameters"];
    }
    if (function!="equal_values" && function!="equal") throw std::runtime_error("Unsupported time-signature function");
    if (rule.contains("indices") || rule.contains("stride") || rule.contains("offset"))
        throw std::runtime_error("Measure time-signature constraints currently use timepoints or bar patterns, not event indices");
    if (rule.contains("bar_pattern")) {
        if (rule.contains("timepoints") || parameters) throw std::runtime_error("Time-signature bar pattern cannot be combined with timepoint parameters");
        const auto& pattern=rule["bar_pattern"];
        if (!pattern.is_array() || pattern.empty()) throw std::runtime_error("Nonempty bar_pattern required");
        const std::string type=rule.value("bar_pattern_type",std::string("fixed"));
        if (type!="fixed" && type!="repeat" && type!="repeating")
            throw std::runtime_error("Measure time-signature currently validates fixed/repeating bar patterns only");
        int repetitions=type=="fixed" || !rule.contains("bar_pattern_repetitions") ? 1 :
            TendencyToneRules::selector_integer(rule["bar_pattern_repetitions"]);
        if (repetitions==0) repetitions=1;
        if (repetitions<1 || repetitions>config.measure_count_plan.measure_count ||
            pattern.size()*static_cast<size_t>(repetitions)!=bars.size()) throw std::runtime_error("Bar pattern count conflicts with measure_count");
        for (size_t index=0; index<bars.size(); ++index)
            if (!(MeasureCountPlan::parse_signature(pattern[index%pattern.size()],"time-signature bar_pattern")==bars[index].signature))
                throw std::runtime_error("Time-signature bar pattern conflicts with measure_plan");
    } else {
        if (!parameters || !parameters->is_array() || parameters->empty()) throw std::runtime_error("Time-signature parameters required");
        std::vector<MeasureCountPlan::TimeSignature> signatures;
        for (const auto& value : *parameters) signatures.push_back(MeasureCountPlan::parse_signature(value,"time-signature parameter"));
        std::vector<int> starts;
        if (rule.contains("timepoints")) {
            if (!rule["timepoints"].is_array()) throw std::runtime_error("Time-signature timepoints must be an array");
            for (const auto& point : rule["timepoints"]) starts.push_back(parse_tick(point,config.rhythm_base,config.measure_horizon_ticks));
        } else starts.push_back(0);
        if (starts.size()!=signatures.size() || starts.front()!=0) throw std::runtime_error("Time-signature timepoints and parameters must match and start at zero");
        for (size_t index=0; index<starts.size(); ++index) {
            if (index && starts[index]<=starts[index-1]) throw std::runtime_error("Time-signature timepoints must strictly increase");
            if (std::none_of(bars.begin(),bars.end(),[&](const MeasureCountPlan::Bar& bar){return bar.start_tick==starts[index];}))
                throw std::runtime_error("Time-signature changes must occur at bar boundaries");
        }
        size_t segment=0;
        for (const auto& bar : bars) {
            while (segment+1<starts.size() && starts[segment+1]<=bar.start_tick) ++segment;
            if (!(bar.signature==signatures[segment])) throw std::runtime_error("Time-signature parameters conflict with measure_plan");
        }
    }
}

inline std::unique_ptr<DynamicRules::CompiledConstraint> compile_signatures(
        const nlohmann::json& rule, const MusicalConstraintSolver::SolverConfig& config) {
    validate_signatures(rule,config);
    bool allow_cross=!rule.contains("bar_pattern");
    if (rule.contains("allow_cross_barline")) {
        const auto& value=rule["allow_cross_barline"];
        if (!value.is_boolean() && !value.is_number_integer()) throw std::runtime_error("allow_cross_barline must be boolean or integer");
        allow_cross=value.is_boolean() ? value.get<bool>() : value!=0;
    }
    const auto bars=MeasureCountPlan::expand_fixed_schedule(config.measure_count_plan,config.rhythm_base);
    auto compiled=std::make_unique<DynamicRules::CompiledConstraint>(rule.value("id",std::string("r-time-signature")),"fixed-plan meter and barline constraint");
    compiled->post_constraint=[bars,allow_cross](DynamicRules::ConstraintContext& ctx) {
        if (allow_cross) return;
        ctx.space->initialize_event_timing();
        for (int voice=0; voice<ctx.num_voices; ++voice) for (int event=0; event<ctx.sequence_length; ++event)
            for (size_t bar=0; bar+1<bars.size(); ++bar) {
                const auto onset=ctx.space->get_event_boundary(voice,event),end=ctx.space->get_event_boundary(voice,event+1);
                Gecode::rel(*ctx.space,(onset>=bars[bar].end_tick) || (end<=bars[bar].end_tick));
            }
    };
    return compiled;
}

inline std::unique_ptr<DynamicRules::CompiledConstraint> compile(
        const nlohmann::json& rule, const MusicalConstraintSolver::SolverConfig& config) {
    const auto predicate=interval_predicate(rule);
    if (rule.contains("indices") || rule.contains("stride") || rule.contains("offset"))
        throw std::runtime_error("Measure timepoint rules select score time, not event-index subsets");
    if (rule.contains("heuristic") && rule["heuristic"]!=false && rule["heuristic"]!=0)
        throw std::runtime_error("Measure timepoint interval rules currently support hard mode only");
    std::vector<int> voices;
    if (!rule.contains("target_voices") || !rule["target_voices"].is_array()) throw std::runtime_error("Timepoint target voices required");
    for (const auto& value : rule["target_voices"]) {
        const int voice=TendencyToneRules::selector_integer(value);
        if (voice>=config.num_voices)
            throw std::runtime_error("Timepoint target voice out of range");
        voices.push_back(voice);
    }
    std::sort(voices.begin(),voices.end()); voices.erase(std::unique(voices.begin(),voices.end()),voices.end());
    if (voices.size()<2) throw std::runtime_error("Timepoint relationships require at least two voices");
    const std::string pairing=rule.value("pair_mode",std::string("adjacent"));
    if (pairing!="adjacent" && pairing!="all_pairs") throw std::runtime_error("Timepoint pair_mode must be adjacent or all_pairs");
    const std::string voice_order=rule.value("voice_order",std::string("none"));
    if (voice_order!="none" && voice_order!="descending") throw std::runtime_error("Timepoint voice_order must be none or descending");
    const bool descending=voice_order=="descending";
    std::vector<std::pair<int,int>> pairs;
    for (size_t first=0; first<voices.size(); ++first)
        for (size_t second=first+1; second<voices.size(); ++second)
            if (pairing=="all_pairs" || second==first+1) pairs.push_back({voices[first],voices[second]});
    const std::string scope=rule.value("time_scope",rule.value("temporal_scope",std::string("all_timepoints")));
    if (rule.contains("time_scope") && rule.contains("temporal_scope") && rule["time_scope"]!=rule["temporal_scope"])
        throw std::runtime_error("time_scope and temporal_scope must agree");
    if ((scope!="at_timepoints" && rule.contains("timepoints")) || (scope!="on_beats" && rule.contains("beat_numbers")))
        throw std::runtime_error("Timepoint/beat filters must match the selected time_scope");
    std::vector<int> ticks;
    if (scope=="at_timepoints") {
        if (!rule.contains("timepoints") || !rule["timepoints"].is_array() || rule["timepoints"].empty()) throw std::runtime_error("Explicit timepoints required");
        for (const auto& point : rule["timepoints"]) ticks.push_back(parse_tick(point,config.rhythm_base,config.measure_horizon_ticks));
    } else if (scope=="on_beats") {
        std::set<int> beat_numbers;
        if (rule.contains("beat_numbers")) {
            if (!rule["beat_numbers"].is_array()) throw std::runtime_error("beat_numbers must be an array");
            for (const auto& number : rule["beat_numbers"]) {
                const int beat=TendencyToneRules::selector_integer(number);
                if (beat<1) throw std::runtime_error("Beat numbers must be positive integers");
                beat_numbers.insert(beat);
            }
        }
        for (const auto& bar : MeasureCountPlan::expand_fixed_schedule(config.measure_count_plan,config.rhythm_base)) {
            if (config.rhythm_base%bar.signature.denominator!=0) throw std::runtime_error("Metric beats require exact tick alignment");
            const int step=config.rhythm_base/bar.signature.denominator;
            for (int beat=1; beat<=bar.signature.numerator; ++beat)
                if (beat_numbers.empty() || beat_numbers.count(beat)) ticks.push_back(bar.start_tick+(beat-1)*step);
        }
            for (int number : beat_numbers)
                if (std::none_of(config.measure_count_plan.fixed_groups.begin(),config.measure_count_plan.fixed_groups.end(),
                    [number](const MeasureCountPlan::Group& group){return number<=group.signature.numerator;}))
                throw std::runtime_error("Beat number is absent from every bar in the plan");
        if (ticks.empty()) throw std::runtime_error("Beat selection contains no bar-plan beats");
    } else if (scope!="all_timepoints") throw std::runtime_error("Unsupported measure time_scope");
    std::sort(ticks.begin(),ticks.end()); ticks.erase(std::unique(ticks.begin(),ticks.end()),ticks.end());
    // next_tick[lo] = first sampled tick >= lo; horizon+1 when none, which no interval end can exceed.
    std::vector<int> next_tick;
    if (scope!="all_timepoints") {
        const int horizon=config.measure_horizon_ticks;
        next_tick.assign(horizon+2,horizon+1);
        std::set<int> sampled(ticks.begin(),ticks.end());
        for (int lo=horizon; lo>=0; --lo) next_tick[lo]=sampled.count(lo) ? lo : next_tick[lo+1];
    }
    auto compiled=std::make_unique<DynamicRules::CompiledConstraint>(rule.value("id",std::string("r-timepoint-relationship")),"score-time pitch relationship");
    compiled->post_constraint=[pairs,next_tick,scope,predicate,descending](DynamicRules::ConstraintContext& ctx) {
        if (!ctx.space || !ctx.space->is_measure_count_mode() || !ctx.pitch_vars || !ctx.rhythm_vars)
            throw std::runtime_error("Score-time relationship requires measure context");
        ctx.space->initialize_event_timing();
        auto post_interval=[&](Gecode::IntVar first,Gecode::IntVar second,Gecode::BoolVar guard) {
            Gecode::IntVar distance=Gecode::expr(*ctx.space,Gecode::abs(first-second));
            Gecode::IntArgs values;
            for (int interval : predicate.second) values << interval;
            Gecode::BoolVar membership(*ctx.space,0,1);
            Gecode::dom(*ctx.space,distance,Gecode::IntSet(values),Gecode::Reify(membership,Gecode::RM_EQV));
            Gecode::rel(*ctx.space,membership,Gecode::IRT_EQ,predicate.first ? 0 : 1,Gecode::Reify(guard,Gecode::RM_IMP));
        };
        for (const auto& pair : pairs) for (int first=0; first<ctx.sequence_length; ++first)
            for (int second=0; second<ctx.sequence_length; ++second) {
                const int first_index=pair.first*ctx.sequence_length+first,second_index=pair.second*ctx.sequence_length+second;
                Gecode::BoolExpr sounded=((*ctx.rhythm_vars)[first_index]>0) && ((*ctx.rhythm_vars)[second_index]>0);
                const auto first_start=ctx.space->get_event_boundary(pair.first,first),first_end=ctx.space->get_event_boundary(pair.first,first+1);
                const auto second_start=ctx.space->get_event_boundary(pair.second,second),second_end=ctx.space->get_event_boundary(pair.second,second+1);
                Gecode::BoolExpr selected=(first_start<second_end) && (second_start<first_end);
                if (scope!="all_timepoints") {
                    const Gecode::IntVar lo=Gecode::expr(*ctx.space,Gecode::max(first_start,second_start));
                    const Gecode::IntVar hi=Gecode::expr(*ctx.space,Gecode::min(first_end,second_end));
                    Gecode::IntVar next(*ctx.space,0,static_cast<int>(next_tick.size())-1);
                    Gecode::element(*ctx.space,Gecode::IntArgs(next_tick.begin(),next_tick.end()),lo,next);
                    selected=next<hi;
                }
                Gecode::BoolVar compare=Gecode::expr(*ctx.space,sounded && selected);
                post_interval((*ctx.pitch_vars)[first_index],(*ctx.pitch_vars)[second_index],compare);
                if (descending) Gecode::rel(*ctx.space,(!compare) || ((*ctx.pitch_vars)[first_index]>=(*ctx.pitch_vars)[second_index]));
            }
    };
    return compiled;
}

}
#endif