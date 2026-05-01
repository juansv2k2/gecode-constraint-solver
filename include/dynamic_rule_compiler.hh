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
    int num_voices = 0;
    int sequence_length = 0;
    int current_voice = 0;
    
    ConstraintContext(IntegratedMusicalSpace* s, 
                     IntVarArray* p_vars,
                     IntVarArray* r_vars,
                     int voices, int length)
        : space(s), pitch_vars(p_vars), rhythm_vars(r_vars), 
          num_voices(voices), sequence_length(length) {}

    // Returns true if the rhythm domain at (voice, pos) allows a rest (negative tick).
    bool position_can_be_rest(int voice, int pos) const {
        if (!rhythm_vars) return false;
        int idx = voice * sequence_length + pos;
        if (idx < 0 || idx >= (int)rhythm_vars->size()) return false;
        return (*rhythm_vars)[idx].min() < 0;
    }
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
    
    // Function to post constraint to Gecode space
    std::function<void(ConstraintContext& ctx)> post_constraint;
    
    CompiledConstraint(const std::string& id, const std::string& desc)
        : rule_id(id), description(desc) {}
    
    void execute(ConstraintContext& ctx) {
        if (post_constraint) {
            post_constraint(ctx);
        }
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
        
        for (auto& constraint : constraints_) {
            try {
                constraint->execute(ctx);
                std::cout << "  ✅ Posted: " << constraint->rule_id << std::endl;
            } catch (const std::exception& e) {
                std::cout << "  ❌ Failed: " << constraint->rule_id << " - " << e.what() << std::endl;
            }
        }
    }
    
    void apply_all_heuristics(ConstraintContext& ctx) {
        std::cout << "🎯 Applying " << heuristics_.size() << " heuristic rules..." << std::endl;
        
        for (auto& heuristic : heuristics_) {
            try {
                heuristic->execute(ctx);
                std::cout << "  🎵 Applied: " << heuristic->rule_id << " (weight: " << heuristic->weight << ")" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "  ❌ Failed: " << heuristic->rule_id << " - " << e.what() << std::endl;
            }
        }
    }
    
    size_t constraint_count() const { return constraints_.size(); }
    size_t heuristic_count() const { return heuristics_.size(); }
    size_t total_count() const { return constraints_.size() + heuristics_.size(); }
};

} // namespace DynamicRules

#endif // DYNAMIC_RULE_COMPILER_HH