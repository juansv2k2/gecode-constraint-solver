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
    int current_voice = 0;
    
    ConstraintContext(IntegratedMusicalSpace* s, 
                     IntVarArray* p_vars,
                     IntVarArray* r_vars,
                                         int voices, int length,
                                         IntVarArray* m_vars = nullptr)
        : space(s), pitch_vars(p_vars), rhythm_vars(r_vars), 
                    metric_vars(m_vars), num_voices(voices), sequence_length(length) {}

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