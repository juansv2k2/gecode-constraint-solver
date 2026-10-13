/**
 * @file dynamic_rule_compiler.hh
 * @brief Compiler for converting rule AST to Gecode propagators
 * 
 * This compiler transforms parsed rule ASTs into executable Gecode
 * constraint propagators for musical constraint solving.
 */

#ifndef DYNAMIC_RULE_COMPILER_HH
#define DYNAMIC_RULE_COMPILER_HH

#include "rule_ast.hh"
#include "rule_expression_parser.hh"
#include "gecode_cluster_integration.hh"
#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <algorithm>

using namespace Gecode;
using namespace GecodeClusterIntegration;

namespace DynamicRules {

/**
 * @brief Exception for rule compilation errors
 */
class RuleCompileException : public std::exception {
private:
    std::string message_;
    
public:
    RuleCompileException(const std::string& msg) : message_("Rule Compile Error: " + msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

/**
 * @brief Context for constraint posting to Gecode space
 */
struct ConstraintContext {
    IntegratedMusicalSpace* space = nullptr;
    IntVarArray* pitch_vars = nullptr;
    IntVarArray* rhythm_vars = nullptr;
    IntVarArray* metric_vars = nullptr;
    int num_voices = 0;
    int sequence_length = 0;
    int rhythm_base = 4;
    int current_voice = 0;
    
    ConstraintContext(IntegratedMusicalSpace* s, 
                     IntVarArray* p_vars,
                     IntVarArray* r_vars,
                                         int voices, int length,
                                         IntVarArray* m_vars = nullptr, int base = 4)
        : space(s), pitch_vars(p_vars), rhythm_vars(r_vars), 
                    metric_vars(m_vars), num_voices(voices), sequence_length(length), rhythm_base(base) {}

    // Returns true if the rhythm domain at (voice, pos) allows a rest (negative tick).
    bool position_can_be_rest(int voice, int pos) const {
        if (!rhythm_vars) return false;
        int idx = voice * sequence_length + pos;
        if (idx < 0 || idx >= (int)rhythm_vars->size()) return false;
        return (*rhythm_vars)[idx].min() < 0;
    }
};

enum class HeuristicMode {
    NONE = 0,
    HEUR_SWITCH,
    REAL_HEURISTIC
};

struct HeuristicCandidateContext {
    int voice = 0;
    int position = 0;
    int candidate_value = 0;
};

/**
 * @brief Compiled constraint that can be posted to Gecode space
 */
class CompiledConstraint {
public:
    std::string rule_id;
    std::string description;
    bool is_heuristic = false;
    int weight = 0;
    int priority = 0;
    HeuristicMode heuristic_mode = HeuristicMode::NONE;
    int applies_to_position = -1;
    int applies_to_voice = -1;
    std::vector<int> applies_to_voices;
    // "pitch" (default) or "rhythm" — controls which brancher scorer uses this heuristic
    std::string heuristic_variable_type = "pitch";
    
    // Function to post constraint to Gecode space
    std::function<void(ConstraintContext& ctx)> post_constraint;

    // Unified numeric score callback for heuristic modes.
    std::function<double(const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx)> score_candidate;
    
    CompiledConstraint(const std::string& id, const std::string& desc)
        : rule_id(id), description(desc) {}
    
    void execute(ConstraintContext& ctx) {
        if (post_constraint) {
            post_constraint(ctx);
        }
    }

    bool has_score_callback() const {
        return static_cast<bool>(score_candidate);
    }

    bool applies_to_candidate(const HeuristicCandidateContext& candidate_ctx) const {
        if (applies_to_position >= 0 && candidate_ctx.position != applies_to_position) {
            return false;
        }
        if (applies_to_voice >= 0 && candidate_ctx.voice != applies_to_voice) {
            return false;
        }
        if (!applies_to_voices.empty() &&
            std::find(applies_to_voices.begin(), applies_to_voices.end(), candidate_ctx.voice) == applies_to_voices.end()) {
            return false;
        }
        return true;
    }

    double evaluate_score(const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx) const {
        if (!score_candidate || !applies_to_candidate(candidate_ctx)) {
            return 0.0;
        }
        return score_candidate(ctx, candidate_ctx);
    }
};

/**
 * @brief Compiler for converting AST to Gecode constraints
 */
inline std::unique_ptr<CompiledConstraint> compile_melodic_step(
        const std::vector<int>& voices, int max_step, bool heuristic, const std::string& id) {
    auto compiled = std::make_unique<CompiledConstraint>(
        id, "r-melodic-step (max_step=" + std::to_string(max_step) + ")");
    if (heuristic) {
        compiled->is_heuristic = true;
        compiled->heuristic_mode = HeuristicMode::REAL_HEURISTIC;
        compiled->heuristic_variable_type = "pitch";
        compiled->applies_to_voices = voices;
        compiled->score_candidate = [voices, max_step](const ConstraintContext& ctx,
                                                      const HeuristicCandidateContext& candidate) {
            if (!ctx.pitch_vars || candidate.voice < 0 || candidate.voice >= ctx.num_voices ||
                candidate.position <= 0 || candidate.position >= ctx.sequence_length ||
                std::find(voices.begin(), voices.end(), candidate.voice) == voices.end()) return 0.0;
            const int index = candidate.voice * ctx.sequence_length + candidate.position;
            if (index >= ctx.pitch_vars->size()) return 0.0;
            if (ctx.space && ctx.space->is_measure_count_mode()) {
                if (!ctx.rhythm_vars || index >= ctx.rhythm_vars->size() || candidate.candidate_value < 0) return 0.0;
                for (int offset : {-1, 0}) {
                    const auto& rhythm = (*ctx.rhythm_vars)[index + offset];
                    if (!rhythm.assigned() || rhythm.val() <= 0) return 0.0;
                }
            }
            const auto& previous = (*ctx.pitch_vars)[index - 1];
            if (!previous.assigned()) return 0.0;
            const int interval = std::abs(candidate.candidate_value - previous.val());
            if (interval <= max_step) return 1.0;
            if (static_cast<long long>(interval) <= static_cast<long long>(max_step) + 4) return 0.3;
            return 0.0;
        };
    } else {
        compiled->post_constraint = [voices, max_step, id](ConstraintContext& ctx) {
            if (!ctx.pitch_vars || !ctx.space) return;
            const bool measure_mode = ctx.space->is_measure_count_mode();
            for (int voice : voices) {
                if (voice < 0 || voice >= ctx.num_voices) {
                    if (measure_mode) throw RuleCompileException(id + ": target voice out of range");
                    continue;
                }
                for (int event = 0; event + 1 < ctx.sequence_length; ++event) {
                    const int index = voice * ctx.sequence_length + event;
                    if (index + 1 >= ctx.pitch_vars->size()) throw RuleCompileException(id + ": pitch variables out of range");
                    IntVar forward = expr(*ctx.space, (*ctx.pitch_vars)[index + 1] - (*ctx.pitch_vars)[index]);
                    IntVar backward = expr(*ctx.space, (*ctx.pitch_vars)[index] - (*ctx.pitch_vars)[index + 1]);
                    if (measure_mode) {
                        if (!ctx.rhythm_vars || index + 1 >= ctx.rhythm_vars->size())
                            throw RuleCompileException(id + ": rhythm variables required in measure mode");
                        BoolVar notes = expr(*ctx.space, ((*ctx.rhythm_vars)[index] > 0) && ((*ctx.rhythm_vars)[index + 1] > 0));
                        rel(*ctx.space, forward, IRT_LQ, max_step, Reify(notes, RM_IMP));
                        rel(*ctx.space, backward, IRT_LQ, max_step, Reify(notes, RM_IMP));
                    } else {
                        rel(*ctx.space, forward, IRT_LQ, max_step);
                        rel(*ctx.space, backward, IRT_LQ, max_step);
                    }
                }
            }
        };
    }
    return compiled;
}

inline std::unique_ptr<CompiledConstraint> compile_measure_melodic_pattern(
        const std::vector<int>& voices, const std::string& type, const std::string& id,
        bool heuristic, int maximum = 2, bool absolute_intervals = false,
        const std::string& direction = "both") {
    const bool intervals = type == "r-no-interval-repetition";
    const bool equal_notes = type == "r-no-consecutive-equal";
    if (!intervals && !equal_notes && type != "r-no-consecutive-direction")
        throw RuleCompileException(id + ": unsupported melodic pattern");
    if (maximum < 1 || (direction != "both" && direction != "ascending" && direction != "descending"))
        throw RuleCompileException(id + ": invalid melodic pattern parameters");
    const long long window = intervals ? 3 : static_cast<long long>(maximum) + 1;
    auto compiled = std::make_unique<CompiledConstraint>(id, type);
    if (heuristic) {
        compiled->is_heuristic = true;
        compiled->heuristic_mode = HeuristicMode::REAL_HEURISTIC;
        compiled->heuristic_variable_type = "pitch";
        compiled->applies_to_voices = voices;
        compiled->score_candidate = [voices, intervals, equal_notes, window, absolute_intervals, direction](
                const ConstraintContext& ctx, const HeuristicCandidateContext& candidate) {
            if (!ctx.pitch_vars || !ctx.rhythm_vars || candidate.voice < 0 || candidate.voice >= ctx.num_voices ||
                candidate.position < 0 || candidate.position >= ctx.sequence_length || candidate.candidate_value < 0 ||
                candidate.position + 1 < window ||
                std::find(voices.begin(), voices.end(), candidate.voice) == voices.end()) return 0.0;
            std::vector<int> pitches;
            const int first = candidate.position - static_cast<int>(window) + 1;
            for (int event = first; event <= candidate.position; ++event) {
                const int index = candidate.voice * ctx.sequence_length + event;
                if (index >= ctx.pitch_vars->size() || index >= ctx.rhythm_vars->size()) return 0.0;
                const auto& rhythm = (*ctx.rhythm_vars)[index];
                if (!rhythm.assigned() || rhythm.val() <= 0) return 0.0;
                const auto& pitch = (*ctx.pitch_vars)[index];
                if (event < candidate.position && !pitch.assigned()) return 0.0;
                pitches.push_back(event == candidate.position ? candidate.candidate_value : pitch.val());
            }
            if (intervals) {
                const int previous = pitches[1] - pitches[0];
                const int current = pitches[2] - pitches[1];
                return (absolute_intervals ? std::abs(previous) == std::abs(current) : previous == current) ? 0.0 : 1.0;
            }
            bool all_equal = true, ascending = true, descending = true;
            for (size_t event = 1; event < pitches.size(); ++event) {
                all_equal = all_equal && pitches[event] == pitches[event - 1];
                ascending = ascending && pitches[event] > pitches[event - 1];
                descending = descending && pitches[event] < pitches[event - 1];
            }
            const bool violation = equal_notes ? all_equal :
                ((direction != "descending" && ascending) || (direction != "ascending" && descending));
            return violation ? 0.0 : 1.0;
        };
    } else {
        compiled->post_constraint = [voices, intervals, equal_notes, window, maximum, absolute_intervals, direction, id](
                ConstraintContext& ctx) {
            if (!ctx.space || !ctx.pitch_vars || !ctx.rhythm_vars)
                throw RuleCompileException(id + ": pitch and rhythm variables required");
            for (int voice : voices) {
                if (voice < 0 || voice >= ctx.num_voices) throw RuleCompileException(id + ": target voice out of range");
                if (window > ctx.sequence_length) continue;
                for (int first = 0; first <= ctx.sequence_length - window; ++first) {
                    const int index = voice * ctx.sequence_length + first;
                    if (index + window > ctx.pitch_vars->size() || index + window > ctx.rhythm_vars->size())
                        throw RuleCompileException(id + ": event variables out of range");
                    BoolVar notes(*ctx.space, 1, 1);
                    for (int offset = 0; offset < window; ++offset)
                        notes = expr(*ctx.space, notes && ((*ctx.rhythm_vars)[index + offset] > 0));
                    if (intervals) {
                        IntVar previous = expr(*ctx.space, (*ctx.pitch_vars)[index + 1] - (*ctx.pitch_vars)[index]);
                        IntVar current = expr(*ctx.space, (*ctx.pitch_vars)[index + 2] - (*ctx.pitch_vars)[index + 1]);
                        rel(*ctx.space, previous, IRT_NQ, current, Reify(notes, RM_IMP));
                        if (absolute_intervals) {
                            IntVar reversed = expr(*ctx.space, -current);
                            rel(*ctx.space, previous, IRT_NQ, reversed, Reify(notes, RM_IMP));
                        }
                    } else {
                        auto forbid_run = [&](IntRelType relation) {
                            BoolVarArgs matches;
                            for (int offset = 0; offset < maximum; ++offset) {
                                BoolVar match(*ctx.space, 0, 1);
                                rel(*ctx.space, (*ctx.pitch_vars)[index + offset + 1], relation,
                                    (*ctx.pitch_vars)[index + offset], Reify(match, RM_EQV));
                                matches << match;
                            }
                            linear(*ctx.space, matches, IRT_LQ, maximum - 1, Reify(notes, RM_IMP));
                        };
                        if (equal_notes) forbid_run(IRT_EQ);
                        else {
                            if (direction != "descending") forbid_run(IRT_GR);
                            if (direction != "ascending") forbid_run(IRT_LE);
                        }
                    }
                }
            }
        };
    }
    return compiled;
}

class DynamicRuleCompiler {
public:
    /**
     * @brief Compile a rule AST into an executable constraint
     */
    static std::unique_ptr<CompiledConstraint> compile_rule(const RuleAST& rule_ast);
    
    /**
     * @brief Compile rule from JSON directly
     */
    static std::unique_ptr<CompiledConstraint> compile_from_json(const nlohmann::json& rule_json);
    
    /**
     * @brief Compile and post an AST constraint directly to the space (PUBLIC for wildcard extension)
     */
    static void compile_and_post_constraint(const ASTNode& node, ConstraintContext& ctx);

    /** Post a measure-mode wildcard predicate only when its referenced events are active notes. */
    static void post_measure_wildcard_predicate(const ASTNode& node, ConstraintContext& ctx);

    /** Boolean form of a validated measure predicate: comparisons, literal membership, AND/OR/NOT. */
    static BoolExpr build_measure_predicate(const ASTNode& node, ConstraintContext& ctx);
    
private:
    /**
     * @brief Compile arithmetic expressions to IntVar or int value
     */
    static IntVar compile_arithmetic_to_var(const ASTNode& node, ConstraintContext& ctx);
    
    /**
     * @brief Get integer value from AST node (for constants)
     */
    static int compile_arithmetic_to_int(const ASTNode& node, ConstraintContext& ctx);
    
    /**
     * @brief Compile variable access to IntVar
     */
    static IntVar compile_variable_to_var(const VariableNode& var_node, ConstraintContext& ctx);
    
    /**
     * @brief Compile voice access expressions to IntVar
     */
    static IntVar compile_voice_access_to_var(const VoiceAccessNode& voice_node, ConstraintContext& ctx);
    
    /**
     * @brief Post binary comparison constraints directly 
     */
    static void post_binary_comparison(const BinaryOpNode& binary_node, ConstraintContext& ctx);
    
    /**
     * @brief Post function-based constraints
     */
    static void post_function_constraint(const FunctionNode& func_node, ConstraintContext& ctx);
    
    /**
     * @brief Post membership constraints for scales/sets
     */
    static void post_membership_constraint(
        const IntVar& var, const std::vector<int>& values, ConstraintContext& ctx);
    
    /**
     * @brief Post membership constraint from binary IN/NOT_IN operation
     */
    static void post_membership_constraint_from_binary(const BinaryOpNode& binary_node, ConstraintContext& ctx);
    
    /**
     * @brief Create all_different constraint
     */
    static void create_distinctness_constraint(const IntVarArray& vars, ConstraintContext& ctx);
    
    /**
     * @brief Resolve variable references in context
     */
    static int resolve_variable_index(const std::string& var_name, ConstraintContext& ctx);
    
    /**
     * @brief Get IntVar array for specific voice and property
     */
    static IntVarArray* get_voice_vars(
        int voice_index, const std::string& property, ConstraintContext& ctx);
};

/**
 * @brief Collection of compiled rules for a solving session
 */
class CompiledRuleSet {
private:
    std::vector<std::unique_ptr<CompiledConstraint>> constraints_;
    std::vector<std::unique_ptr<CompiledConstraint>> heuristics_;
    int last_posted_success_count_ = 0;
    int last_posted_failed_count_ = 0;
    
public:
    void add_constraint(std::unique_ptr<CompiledConstraint> constraint) {
        if (constraint->is_heuristic) {
            heuristics_.push_back(std::move(constraint));
        } else {
            constraints_.push_back(std::move(constraint));
        }
    }
    
    void post_all_constraints(ConstraintContext& ctx) {
        std::cout << "🔧 Posting " << constraints_.size() << " dynamic constraints..." << std::endl;
        last_posted_success_count_ = 0;
        last_posted_failed_count_ = 0;
        
        for (auto& constraint : constraints_) {
            try {
                constraint->execute(ctx);
                ++last_posted_success_count_;
                std::cout << "  ✅ Posted: " << constraint->rule_id << std::endl;
            } catch (const std::exception& e) {
                ++last_posted_failed_count_;
                std::cout << "  ❌ Failed: " << constraint->rule_id << " - " << e.what() << std::endl;
                if (ctx.space && ctx.space->is_measure_count_mode())
                    throw RuleCompileException("Hard constraint '" + constraint->rule_id + "' could not be posted: " + e.what());
            }
        }
    }
    
    void apply_all_heuristics(ConstraintContext& ctx) {
        (void)ctx;
        std::cout << "🎯 Registered " << heuristics_.size()
                  << " heuristic rules for score-based ordering" << std::endl;
    }

    double evaluate_heuristic_score(const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx) const {
        double total = 0.0;
        for (const auto& heuristic : heuristics_) {
            if (!heuristic->has_score_callback()) {
                continue;
            }
            total += heuristic->evaluate_score(ctx, candidate_ctx);
        }
        return total;
    }

    std::vector<std::pair<int, double>> evaluate_heuristic_buckets(
        const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx) const {
        std::map<int, double, std::greater<int>> buckets;
        for (const auto& heuristic : heuristics_) {
            if (!heuristic->has_score_callback()) continue;
            if (heuristic->heuristic_variable_type == "rhythm") continue; // pitch scorer only
            buckets[heuristic->priority] += heuristic->evaluate_score(ctx, candidate_ctx);
        }
        std::vector<std::pair<int, double>> ordered;
        ordered.reserve(buckets.size());
        for (const auto& kv : buckets) ordered.push_back(kv);
        return ordered;
    }

    std::vector<std::pair<int, double>> evaluate_rhythm_heuristic_buckets(
        const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx) const {
        std::map<int, double, std::greater<int>> buckets;
        for (const auto& heuristic : heuristics_) {
            if (!heuristic->has_score_callback()) continue;
            if (heuristic->heuristic_variable_type != "rhythm") continue; // rhythm scorer only
            buckets[heuristic->priority] += heuristic->evaluate_score(ctx, candidate_ctx);
        }
        std::vector<std::pair<int, double>> ordered;
        ordered.reserve(buckets.size());
        for (const auto& kv : buckets) ordered.push_back(kv);
        return ordered;
    }

    bool has_heuristic_scorers() const {
        for (const auto& heuristic : heuristics_) {
            if (heuristic->has_score_callback() && heuristic->heuristic_variable_type != "rhythm")
                return true;
        }
        return false;
    }

    bool has_rhythm_heuristic_scorers() const {
        for (const auto& heuristic : heuristics_) {
            if (heuristic->has_score_callback() && heuristic->heuristic_variable_type == "rhythm")
                return true;
        }
        return false;
    }
    
    size_t constraint_count() const { return constraints_.size(); }
    size_t heuristic_count() const { return heuristics_.size(); }
    size_t total_count() const { return constraints_.size() + heuristics_.size(); }
    int last_posted_success_count() const { return last_posted_success_count_; }
    int last_posted_failed_count() const { return last_posted_failed_count_; }
};

} // namespace DynamicRules

#endif // DYNAMIC_RULE_COMPILER_HH