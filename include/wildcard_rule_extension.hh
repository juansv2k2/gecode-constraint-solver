/**
 * @file wildcard_rule_extension.hh
 * @brief Extension to dynamic rule compiler for wildcard/pattern-based rules
 * 
 * Implements cluster-engine style wildcard rules that iterate across positions
 * and voices, allowing constraints like:
 * - "For all positions i, constraint X must hold"
 * - "For all voices, no parallel fifths" 
 * - "Every sequence of 3 notes must contain stepwise motion"
 * 
 * Based on cluster engine's wildcard rule architecture.
 */

#ifndef WILDCARD_RULE_EXTENSION_HH
#define WILDCARD_RULE_EXTENSION_HH

#include "dynamic_rule_compiler.hh"
#include "rule_ast.hh"
#include <gecode/int.hh>
#include <functional>
#include <vector>

using namespace Gecode;

namespace DynamicRules {

/**
 * @brief Wildcard rule types supported
 */
enum class WildcardType {
    FOR_ALL_POSITIONS,      // Apply constraint to all positions in sequence
    FOR_ALL_VOICES,         // Apply constraint to all voices 
    FOR_ALL_PAIRS,          // Apply constraint to all pairs of elements
    SLIDING_WINDOW,         // Apply constraint to overlapping windows
    SEQUENCE_PATTERN        // Pattern matching across sequence
};

/**
 * @brief Wildcard rule specification
 */
struct WildcardSpec {
    WildcardType type;
    std::vector<int> pattern_offsets;   // Pattern shape: [0, 1, 3] means check positions [i, i+1, i+3]
    int window_size = 1;                // For sliding window rules
    int step_size = 1;                  // Step between applications
    bool cross_voices = false;          // Whether rule can apply across voices
    std::string constraint_expr;        // The constraint to apply
    
    WildcardSpec(WildcardType t, const std::string& expr) 
        : type(t), constraint_expr(expr) {}
};

/**
 * @brief Extended compiler that can handle wildcard rules
 */
class WildcardRuleCompiler : public DynamicRuleCompiler {
public:
    /**
     * @brief Compile wildcard rule from JSON
     * 
     * JSON format:
     * {
     *   "rule_id": "stepwise_motion_everywhere",
     *   "rule_type": "wildcard_constraint", 
     *   "wildcard_type": "sliding_window",
     *   "pattern_offsets": [0, 1],
     *   "constraint": "abs(voice[0].pitch[i] - voice[0].pitch[i+1]) <= 2",
     *   "scope": "all_positions"
     * }
     */
    static std::unique_ptr<CompiledConstraint> compile_wildcard_from_json(
        const nlohmann::json& rule_json);
    
    /**
     * @brief Create constraint that applies rule to all positions
     */
    static std::function<void(ConstraintContext&)> create_for_all_positions_constraint(
        const WildcardSpec& spec, 
        const std::unique_ptr<RuleAST>& constraint_ast);
    
    /**
     * @brief Create constraint that applies rule to all voices
     */
    static std::function<void(ConstraintContext&)> create_for_all_voices_constraint(
        const WildcardSpec& spec,
        const std::unique_ptr<RuleAST>& constraint_ast);
    
    /**
     * @brief Create sliding window constraint
     */
    static std::function<void(ConstraintContext&)> create_sliding_window_constraint(
        const WildcardSpec& spec,
        const std::unique_ptr<RuleAST>& constraint_ast);
    
    /**
     * @brief Apply constraint at specific position/voice with variable substitution
     */
    static void apply_constraint_at_position(
        const ASTNode& constraint_node,
        ConstraintContext& ctx,
        int voice_idx,
        int position_idx,
        const std::vector<int>& pattern_offsets = {0});

private:
    /**
     * @brief Parse wildcard specification from JSON
     */
    static WildcardSpec parse_wildcard_spec(const nlohmann::json& rule_json);
    
    /**
     * @brief Substitute pattern variables in constraint AST
     * @param node AST node to process
     * @param voice_idx Current voice index
     * @param position_idx Current position index  
     * @param pattern_offsets Pattern offset mapping
     * @return New AST node with substituted variables
     */
    static std::unique_ptr<ASTNode> substitute_pattern_variables(
        const ASTNode& node,
        int voice_idx,
        int position_idx, 
        const std::vector<int>& pattern_offsets);
    
    /**
     * @brief Compile substituted AST node to Gecode expression
     */
    static IntArgs compile_to_gecode_expression(
        const ASTNode& node,
        ConstraintContext& ctx,
        int voice_idx,
        int position_idx);
};

/**
 * @brief Factory for common wildcard rule patterns
 */
class WildcardRuleFactory {
public:
    /**
     * @brief No parallel fifths across all voices
     */
    static nlohmann::json create_no_parallel_fifths_rule() {
        return {
            {"rule_id", "no_parallel_fifths"},
            {"rule_type", "wildcard_constraint"},
            {"wildcard_type", "for_all_pairs"},
            {"constraint", "not ((voice[i].pitch[pos] - voice[j].pitch[pos]) % 12 == 7 and (voice[i].pitch[pos+1] - voice[j].pitch[pos+1]) % 12 == 7)"},
            {"scope", "cross_voices"},
            {"description", "No parallel perfect fifths between any voices"}
        };
    }
    
    /**
     * @brief Stepwise motion in all voices
     */
    static nlohmann::json create_stepwise_motion_rule() {
        return {
            {"rule_id", "stepwise_motion"},
            {"rule_type", "wildcard_constraint"},
            {"wildcard_type", "sliding_window"},
            {"pattern_offsets", {0, 1}},
            {"constraint", "abs(voice[v].pitch[i] - voice[v].pitch[i+1]) <= 2"},
            {"scope", "each_voice"},
            {"description", "Prefer stepwise motion in all voices"}
        };
    }
    
    /**
     * @brief No repeated notes in any voice
     */
    static nlohmann::json create_no_repetition_rule() {
        return {
            {"rule_id", "no_repeated_notes"},
            {"rule_type", "wildcard_constraint"},
            {"wildcard_type", "sliding_window"},
            {"pattern_offsets", {0, 1}},
            {"constraint", "voice[v].pitch[i] != voice[v].pitch[i+1]"},
            {"scope", "each_voice"},
            {"description", "No immediate repetitions in any voice"}
        };
    }
    
    /**
     * @brief Ensure variety in pitch range across sequence
     */
    static nlohmann::json create_pitch_variety_rule() {
        return {
            {"rule_id", "pitch_variety"},
            {"rule_type", "wildcard_constraint"},
            {"wildcard_type", "sequence_pattern"},
            {"pattern_offsets", {0, 1, 2}},
            {"constraint", "not (voice[v].pitch[i] == voice[v].pitch[i+1] and voice[v].pitch[i+1] == voice[v].pitch[i+2])"},
            {"scope", "each_voice"},
            {"description", "No three identical consecutive pitches"}
        };
    }
    
    /**
     * @brief Musical phrase structure - end phrases on stable notes
     */
    static nlohmann::json create_phrase_endings_rule() {
        return {
            {"rule_id", "stable_phrase_endings"},
            {"rule_type", "wildcard_constraint"},
            {"wildcard_type", "for_all_positions"},
            {"constraint", "(pos % 4 == 3) implies (voice[0].pitch[pos] % 12 in [0, 4, 7])"},
            {"scope", "phrase_structure"},
            {"description", "End phrases on tonic triad tones"}
        };
    }
};

} // namespace DynamicRules

#endif // WILDCARD_RULE_EXTENSION_HH