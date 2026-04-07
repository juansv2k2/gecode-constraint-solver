/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 * Enhanced Rule Architecture
 * 
 * This implements Cluster Engine v4.05's sophisticated rule system
 * with three types of musical constraint rules and intelligent backjumping.
 * 
 * Key Innovation: Rules provide both constraint checking AND backjumping hints
 * to guide search efficiently through the musical solution space.
 * 
 * Based on Cluster Engine v4.05 rule architecture:
 * - IndexRule: Constraints on specific variable indices  
 * - WildcardRule: Pattern-based constraints (musical motifs)
 * - RLRule: Global structure constraints (from variable X to end)
 * - Intelligent backjump distance suggestions for musical search
 */

#ifndef ENHANCED_RULE_ARCHITECTURE_HH
#define ENHANCED_RULE_ARCHITECTURE_HH

#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include "dual_solution_storage.hh"

namespace MusicalConstraints {

// Forward declarations
class RuleEngine;

/**
 * @brief Backjump suggestion for failed rule constraints
 * 
 * Provides intelligent suggestions for constraint domain reductions
 * when a rule fails, supporting advanced musical search heuristics.
 */
struct BackjumpSuggestion {
    int variable_index;                        ///< Variable to modify
    int backjump_distance;                    ///< How far back to jump
    std::vector<int> allowed_absolute_values; ///< Restricted absolute value domain
    std::vector<int> allowed_interval_values; ///< Restricted interval value domain
    std::string explanation;                  ///< Human-readable explanation
    
    BackjumpSuggestion() : variable_index(-1), backjump_distance(1) {}
    
    BackjumpSuggestion(int var_idx, int distance = 1) 
        : variable_index(var_idx), backjump_distance(distance) {}
};

/**
 * @brief Result of rule evaluation with backjump intelligence
 * 
 * This encapsulates both the success/failure of a rule and provides
 * intelligent suggestions for backjumping when the rule fails.
 * 
 * Based on Cluster Engine v4.05's sophisticated backtracking system.
 */
struct RuleResult {
    bool passes;                                      ///< Whether the rule passed (renamed from success)
    int backjump_distance;                           ///< Suggested backjump distance if failed
    std::string failure_reason;                      ///< Human-readable failure explanation
    std::vector<BackjumpSuggestion> backjump_suggestions; ///< Domain restrictions and backjump hints
    
    /// Constructor for successful rule
    RuleResult() : passes(true), backjump_distance(0) {}
    
    /// Constructor for failed rule with backjump suggestion
    RuleResult(bool success, int backjump = 1, const std::string& reason = "")
        : passes(success), backjump_distance(backjump), failure_reason(reason) {}
        
    /// Static factory for success
    static RuleResult Success() { return RuleResult(); }
    
    /// Static factory for failure with backjump
    static RuleResult Failure(int backjump = 1, const std::string& reason = "") {
        return RuleResult(false, backjump, reason);
    }

    /// Add backjump suggestion
    void add_suggestion(const BackjumpSuggestion& suggestion) {
        backjump_suggestions.push_back(suggestion);
    }
};

/**
 * @brief Base class for all musical constraint rules
 * 
 * This provides the common interface for Cluster Engine's rule system.
 * All rules can both check constraints and suggest intelligent backjumping
 * to improve search performance on musical problems.
 */
class MusicalRule {
public:
    virtual ~MusicalRule() = default;
    
    /**
     * @brief Check if the rule is satisfied by current solution state
     * @param storage The dual solution storage containing current assignments
     * @param current_index The variable index being currently assigned  
     * @return RuleResult with success/failure and backjump suggestions
     */
    virtual RuleResult check_rule(const DualSolutionStorage& storage, 
                                 int current_index) const = 0;
    
    /**
     * @brief Get human-readable description of this rule
     * @return String description for debugging and documentation
     */
    virtual std::string description() const = 0;
    
    /**
     * @brief Get the variables this rule depends on
     * @param current_index The index currently being assigned
     * @return Vector of variable indices this rule checks
     */
    virtual std::vector<int> get_dependent_variables(int current_index) const = 0;
    
    /**
     * @brief Get the rule type for optimization and debugging
     * @return String identifying the rule type
     */
    virtual std::string rule_type() const = 0;
};

/**
 * @brief Index-based rules: constraints on specific variable indices
 * 
 * These rules check relationships between specific variables in the solution.
 * Based on Cluster Engine's indexrule class.
 * 
 * Example: "Variables 2, 5, and 8 must form a major chord"
 */
class IndexRule : public MusicalRule {
private:
    std::vector<int> variable_indices_;     ///< Specific variable indices to check
    std::function<bool(const std::vector<int>&)> rule_function_;  ///< The constraint function
    int backjump_distance_;                 ///< Suggested backjump distance
    std::string description_;               ///< Human-readable description
    
public:
    /**
     * @brief Create index-based rule
     * @param variable_indices The specific variable indices this rule checks
     * @param rule_function Function that checks the constraint (receives absolute values)
     * @param description Human-readable description of the rule
     */
    IndexRule(const std::vector<int>& variable_indices,
              std::function<bool(const std::vector<int>&)> rule_function,
              const std::string& description = "Index rule")
        : variable_indices_(variable_indices), rule_function_(rule_function), 
          description_(description) {
        
        // Calculate intelligent backjump distance based on variable dependencies
        if (variable_indices_.size() <= 1) {
            backjump_distance_ = 0;  // No dependencies
        } else {
            auto sorted_indices = variable_indices_;
            std::sort(sorted_indices.rbegin(), sorted_indices.rend());  // Descending
            backjump_distance_ = sorted_indices[0] - sorted_indices[1];
        }
    }
    
    RuleResult check_rule(const DualSolutionStorage& storage, int current_index) const override {
        // Only check if all required variables are assigned
        for (int idx : variable_indices_) {
            if (idx >= current_index) {
                return RuleResult::Success();  // Not ready to check yet
            }
        }
        
        // Gather absolute values for the rule function
        std::vector<int> values;
        for (int idx : variable_indices_) {
            values.push_back(storage.absolute(idx));
        }
        
        // Check the constraint
        bool result = rule_function_(values);
        
        if (result) {
            return RuleResult::Success();
        } else {
            return RuleResult::Failure(backjump_distance_, 
                "Index rule failed on variables: " + indices_to_string());
        }
    }
    
    std::string description() const override { return description_; }
    
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> dependencies;
        for (int idx : variable_indices_) {
            if (idx < current_index) {
                dependencies.push_back(idx);
            }
        }
        return dependencies;
    }
    
    std::string rule_type() const override { return "IndexRule"; }
    
    /// Get the variable indices this rule checks
    const std::vector<int>& variable_indices() const { return variable_indices_; }
    
private:
    std::string indices_to_string() const {
        std::string result = "[";
        for (size_t i = 0; i < variable_indices_.size(); ++i) {
            if (i > 0) result += ", ";
            result += std::to_string(variable_indices_[i]);
        }
        result += "]";
        return result;
    }
};

/**
 * @brief Pattern-based wildcard rules: constraints on relative patterns
 * 
 * These rules check patterns that can occur at any position in the sequence.
 * Based on Cluster Engine's wildcardrule class.
 * 
 * Example: "Every sequence of 3 consecutive notes must contain at least one stepwise motion"
 * Pattern {0, 1, 2} applied starting from any variable.
 */
class WildcardRule : public MusicalRule {
private:
    std::vector<int> pattern_offsets_;      ///< Relative offsets from pattern start
    std::function<bool(const std::vector<int>&, const std::vector<int>&)> rule_function_;  ///< Function receives (abs, intervals)
    int backjump_distance_;                 ///< Suggested backjump distance
    std::string description_;               ///< Human-readable description
    
public:
    /**
     * @brief Create pattern-based wildcard rule
     * @param pattern_offsets Relative offsets from pattern start (must start with 0)
     * @param rule_function Function checking pattern (receives absolute and interval values)
     * @param description Human-readable description
     * 
     * Example: pattern_offsets {0, 1, 3} means check variables [start, start+1, start+3]
     */
    WildcardRule(const std::vector<int>& pattern_offsets,
                 std::function<bool(const std::vector<int>&, const std::vector<int>&)> rule_function,
                 const std::string& description = "Wildcard rule") 
        : pattern_offsets_(pattern_offsets), rule_function_(rule_function), 
          description_(description) {
        
        if (pattern_offsets_.empty() || pattern_offsets_[0] != 0) {
            throw std::invalid_argument("Pattern offsets must start with 0");
        }
        
        // Calculate backjump distance from pattern span
        if (pattern_offsets_.size() <= 1) {
            backjump_distance_ = 0;
        } else {
            int max_offset = *std::max_element(pattern_offsets_.begin(), pattern_offsets_.end());
            backjump_distance_ = max_offset;
        }
    }
    
    RuleResult check_rule(const DualSolutionStorage& storage, int current_index) const override {
        // Find the pattern start position for current_index
        int max_offset = *std::max_element(pattern_offsets_.begin(), pattern_offsets_.end());
        int pattern_start = current_index - max_offset;
        
        if (pattern_start < 0) {
            return RuleResult::Success();  // Pattern not yet applicable
        }
        
        // Gather values for the pattern
        std::vector<int> abs_values, interval_values;
        for (int offset : pattern_offsets_) {
            int var_index = pattern_start + offset;
            abs_values.push_back(storage.absolute(var_index));
            interval_values.push_back(storage.interval(var_index));
        }
        
        // Check the pattern constraint
        bool result = rule_function_(abs_values, interval_values);
        
        if (result) {
            return RuleResult::Success();
        } else {
            return RuleResult::Failure(backjump_distance_,
                "Wildcard pattern failed at position " + std::to_string(pattern_start));
        }
    }
    
    std::string description() const override { return description_; }
    
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> dependencies;
        int max_offset = *std::max_element(pattern_offsets_.begin(), pattern_offsets_.end());
        int pattern_start = current_index - max_offset;
        
        if (pattern_start >= 0) {
            for (int offset : pattern_offsets_) {
                int var_index = pattern_start + offset;
                if (var_index < current_index) {
                    dependencies.push_back(var_index);
                }
            }
        }
        return dependencies;
    }
    
    std::string rule_type() const override { return "WildcardRule"; }
    
    /// Get the pattern offsets
    const std::vector<int>& pattern_offsets() const { return pattern_offsets_; }
};

/**
 * @brief Global structure rules: constraints from a variable to sequence end
 * 
 * These rules check global properties of the musical sequence.
 * Based on Cluster Engine's RL-rule class.
 * 
 * Example: "From variable 10 onwards, maintain the same tonal center"
 */
class RLRule : public MusicalRule {
private:
    int first_variable_;                    ///< First variable where rule applies
    std::function<bool(const std::vector<int>&, const std::vector<int>&)> rule_function_;  ///< Global constraint function
    std::string description_;               ///< Human-readable description
    bool is_heuristic_;                    ///< Whether this is a heuristic rule (backjump = 0)
    
public:
    /**
     * @brief Create global structure rule
     * @param first_variable First variable index where rule starts applying
     * @param rule_function Function checking global property (receives all abs and interval values from first_variable)
     * @param description Human-readable description
     * @param is_heuristic If true, rule provides guidance but doesn't force backjumping
     */
    RLRule(int first_variable,
           std::function<bool(const std::vector<int>&, const std::vector<int>&)> rule_function,
           const std::string& description = "RL rule",
           bool is_heuristic = false)
        : first_variable_(first_variable), rule_function_(rule_function),
          description_(description), is_heuristic_(is_heuristic) {}
    
    RuleResult check_rule(const DualSolutionStorage& storage, int current_index) const override {
        if (current_index < first_variable_) {
            return RuleResult::Success();  // Rule not yet applicable
        }
        
        // Gather all values from first_variable to current_index
        std::vector<int> abs_values, interval_values;
        for (int i = first_variable_; i <= current_index; ++i) {
            abs_values.push_back(storage.absolute(i));
            interval_values.push_back(storage.interval(i));
        }
        
        // Check the global constraint
        bool result = rule_function_(abs_values, interval_values);
        
        if (result) {
            return RuleResult::Success();
        } else {
            // RL rules suggest stepping back (not jumping) unless they're heuristic
            int backjump = is_heuristic_ ? 0 : 1;
            return RuleResult::Failure(backjump,
                "RL rule failed from variable " + std::to_string(first_variable_));
        }
    }
    
    std::string description() const override { return description_; }
    
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> dependencies;
        if (current_index >= first_variable_) {
            for (int i = first_variable_; i < current_index; ++i) {
                dependencies.push_back(i);
            }
        }
        return dependencies;
    }
    
    std::string rule_type() const override { 
        return is_heuristic_ ? "HeuristicRLRule" : "RLRule"; 
    }
    
    /// Get the first variable where this rule applies
    int first_variable() const { return first_variable_; }
    
    /// Check if this is a heuristic rule
    bool is_heuristic() const { return is_heuristic_; }
};

/**
 * @brief Rule engine that coordinates multiple rules and manages backjumping
 * 
 * This class manages the application of multiple rules and combines their
 * backjumping suggestions intelligently, following Cluster Engine v4.05's
 * sophisticated backjumping modes.
 */
class RuleEngine {
private:
    std::vector<std::unique_ptr<MusicalRule>> rules_;          ///< Collection of all rules
    std::vector<std::unique_ptr<MusicalRule>> heuristic_rules_; ///< Collection of heuristic rules
    
public:
    /**
     * @brief Add a strict constraint rule
     * @param rule Rule to add (ownership transferred)
     */
    void add_rule(std::unique_ptr<MusicalRule> rule) {
        rules_.push_back(std::move(rule));
    }
    
    /**
     * @brief Add a heuristic rule (provides guidance, doesn't force backjumping)
     * @param rule Heuristic rule to add (ownership transferred)
     */
    void add_heuristic_rule(std::unique_ptr<MusicalRule> rule) {
        heuristic_rules_.push_back(std::move(rule));
    }
    
    /**
     * @brief Check all applicable rules and return combined result
     * @param storage Current solution storage
     * @param current_index Variable index being assigned
     * @return Combined rule result with intelligent backjump suggestion
     */
    RuleResult check_all_rules(const DualSolutionStorage& storage, int current_index) const {
        std::vector<RuleResult> failures;
        
        // Check all strict rules
        for (const auto& rule : rules_) {
            RuleResult result = rule->check_rule(storage, current_index);
            if (!result.passes) {
                failures.push_back(result);
                // For immediate fail mode (Cluster Engine Mode 2), return immediately
                // For consensus mode (Mode 3), collect all failures first
            }
        }
        
        if (failures.empty()) {
            return RuleResult::Success();
        }
        
        // Combine backjump suggestions (use minimum for conservative approach)
        int min_backjump = failures[0].backjump_distance;
        std::string combined_reason = "Multiple rules failed: ";
        
        for (size_t i = 0; i < failures.size(); ++i) {
            if (failures[i].backjump_distance < min_backjump) {
                min_backjump = failures[i].backjump_distance;
            }
            if (i > 0) combined_reason += "; ";
            combined_reason += failures[i].failure_reason;
        }
        
        return RuleResult::Failure(min_backjump, combined_reason);
    }
    
    /**
     * @brief Check heuristic rules for search guidance
     * @param storage Current solution storage
     * @param current_index Variable index being assigned
     * @return Heuristic evaluation (always succeeds, provides guidance)
     */
    RuleResult check_heuristic_rules(const DualSolutionStorage& storage, int current_index) const {
        for (const auto& rule : heuristic_rules_) {
            RuleResult result = rule->check_rule(storage, current_index);
            // Heuristic rules provide information but don't force failures
            // This information could be used for search guidance
        }
        return RuleResult::Success();  // Heuristics never fail
    }
    
    /**
     * @brief Get count of rules
     */
    size_t rule_count() const { return rules_.size(); }
    size_t heuristic_rule_count() const { return heuristic_rules_.size(); }
    
    /**
     * @brief Print all rules for debugging
     */
    void print_rules(std::ostream& os = std::cout) const {
        os << "Strict Rules (" << rule_count() << "):\n";
        for (size_t i = 0; i < rules_.size(); ++i) {
            os << "  " << i << ": " << rules_[i]->description() 
               << " [" << rules_[i]->rule_type() << "]\n";
        }
        
        os << "Heuristic Rules (" << heuristic_rule_count() << "):\n";
        for (size_t i = 0; i < heuristic_rules_.size(); ++i) {
            os << "  " << i << ": " << heuristic_rules_[i]->description() 
               << " [" << heuristic_rules_[i]->rule_type() << "]\n";
        }
    }
};

} // namespace MusicalConstraints

#endif // ENHANCED_RULE_ARCHITECTURE_HH