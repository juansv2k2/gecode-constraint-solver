/**
 * @file gecode_cluster_integration.hh
 * @brief Header file for Gecode Integration with Cluster Functionality
 * 
 * Provides musical rule compilation system integrated with Gecode constraints.
 */

#pragma once

#include "gecode/gecode/int.hh"
#include "gecode/gecode/search.hh"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <map>

namespace GecodeClusterIntegration {

using namespace Gecode;

// ===============================
// Core Musical Rule Types (from Cluster sources)
// ===============================

/**
 * @brief Musical rule execution context
 */
struct RuleExecutionContext;

/**
 * @brief Rule evaluation result
 */
enum class RuleResult {
    PASS = 0,           // Rule constraint satisfied
    FAIL = 1,           // Rule constraint violated  
    INSUFFICIENT_DATA = 2  // Not enough data to evaluate rule
};

/**
 * @class MusicalRule
 * @brief Base class for all musical constraint rules (from Cluster sources)
 */
class MusicalRule {
public:
    enum class RuleType {
        SINGLE_ENGINE_RHYTHM,      // Rhythmic patterns within one voice
        SINGLE_ENGINE_PITCH,       // Pitch patterns within one voice
        DUAL_ENGINE_RHYTHM_PITCH,  // Rhythm-pitch coordination
        MULTI_ENGINE_HARMONIC,     // Multi-voice harmonic constraints
        RETROGRADE_INVERSION,      // Retrograde inversion constraint
        CUSTOM                     // User-defined custom rules
    };

protected:
    std::string name_;
    RuleType type_;
    std::vector<int> target_engines_;
    std::vector<int> backtrack_route_;
    int priority_;
    bool enabled_;

public:
    MusicalRule(const std::string& name, RuleType type, 
                const std::vector<int>& engines, const std::vector<int>& backtrack_route,
                int priority = 1)
        : name_(name), type_(type), target_engines_(engines), 
          backtrack_route_(backtrack_route), priority_(priority), enabled_(true) {}

    virtual ~MusicalRule() = default;

    const std::string& get_name() const { return name_; }
    RuleType get_type() const { return type_; }
    const std::vector<int>& get_target_engines() const { return target_engines_; }
    const std::vector<int>& get_backtrack_route() const { return backtrack_route_; }
    int get_priority() const { return priority_; }
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    // Pure virtual methods to be implemented in derived classes
    virtual std::string get_description() const = 0;
};

/**
 * @class IntegratedMusicalSpace
 * @brief Enhanced musical constraint space with rule compilation system
 */
class IntegratedMusicalSpace : public Space {
private:
    int sequence_length_;
    int voices_;
    
    // Musical variables
    IntVarArray absolute_vars_;
    IntVarArray interval_vars_;
    
    // Musical constraints (retrograde inversion)
    int inversion_center_;
    bool retrograde_enabled_;
    
    // Compiled rule system
    std::vector<std::unique_ptr<MusicalRule>> compiled_rules_;

public:
    IntegratedMusicalSpace(int sequence_length, int voices, int min_note = 60, int max_note = 72, int center = 66) 
        : sequence_length_(sequence_length), voices_(voices), inversion_center_(center), retrograde_enabled_(false) {
        
        // Initialize absolute variables (notes)
        absolute_vars_ = IntVarArray(*this, sequence_length_ * voices, min_note, max_note);
        
        // Initialize interval variables  
        interval_vars_ = IntVarArray(*this, (sequence_length_ - 1) * voices, -12, 12);
        
        // Link intervals to absolute values
        for (int voice = 0; voice < voices_; ++voice) {
            for (int i = 0; i < sequence_length_ - 1; ++i) {
                int abs_idx1 = voice * sequence_length_ + i;
                int abs_idx2 = voice * sequence_length_ + i + 1;
                int int_idx = voice * (sequence_length_ - 1) + i;
                
                rel(*this, interval_vars_[int_idx], IRT_EQ, 
                    expr(*this, absolute_vars_[abs_idx2] - absolute_vars_[abs_idx1]));
            }
        }
    }

    // Copy constructor for search
    IntegratedMusicalSpace(bool share, IntegratedMusicalSpace& s) : Space(share, s) {
        sequence_length_ = s.sequence_length_;
        voices_ = s.voices_;
        inversion_center_ = s.inversion_center_;
        retrograde_enabled_ = s.retrograde_enabled_;
        
        absolute_vars_.update(*this, share, s.absolute_vars_);
        interval_vars_.update(*this, share, s.interval_vars_);
    }

    // Copy method for search
    virtual Space* copy(bool share) {
        return new IntegratedMusicalSpace(share, *this);
    }

    // Branching strategy
    void branch_on_variables() {
        branch(*this, absolute_vars_, INT_VAR_SIZE_MIN(), INT_VAL_MIN());
    }

    // Musical constraint methods
    void add_retrograde_inversion_constraint(int center) {
        inversion_center_ = center;
        retrograde_enabled_ = true;
        
        if (voices_ >= 2) {
            for (int i = 0; i < sequence_length_; ++i) {
                int voice1_idx = i;                                    // First voice
                int voice2_idx = sequence_length_ + (sequence_length_ - 1 - i); // Second voice (retrograde)
                
                rel(*this, absolute_vars_[voice2_idx], IRT_EQ, 
                    expr(*this, 2 * center - absolute_vars_[voice1_idx]));
            }
        }
    }

    // Compiled rule system
    void add_compiled_musical_rule(std::unique_ptr<MusicalRule> rule);
    void apply_compiled_rule_constraints(const MusicalRule& rule);
    bool evaluate_compiled_rules() const;
    
    // JSON interface
    void add_rule_from_json(const std::string& rule_type, const std::map<std::string, int>& parameters);

    // Solution access
    std::vector<int> get_absolute_sequence() const {
        std::vector<int> result;
        for (int i = 0; i < absolute_vars_.size(); ++i) {
            if (absolute_vars_[i].assigned()) {
                result.push_back(absolute_vars_[i].val());
            }
        }
        return result;
    }
    
    std::vector<int> get_interval_sequence() const {
        std::vector<int> result;
        for (int i = 0; i < interval_vars_.size(); ++i) {
            if (interval_vars_[i].assigned()) {
                result.push_back(interval_vars_[i].val());
            }
        }
        return result;
    }

    // Test interface
    bool test_retrograde_inversion() const {
        if (!retrograde_enabled_ || voices_ < 2) return true;
        
        auto abs_vals = get_absolute_sequence();
        if (abs_vals.size() < 2 * sequence_length_) return true;
        
        for (int i = 0; i < sequence_length_; ++i) {
            int voice1_note = abs_vals[i];
            int voice2_note = abs_vals[sequence_length_ + (sequence_length_ - 1 - i)];
            int expected = 2 * inversion_center_ - voice1_note;
            
            if (voice2_note != expected) {
                return false;
            }
        }
        return true;
    }
    
    void print_solution() const {
        auto abs_vals = get_absolute_sequence();
        auto int_vals = get_interval_sequence();
        
        std::cout << "Musical Solution:" << std::endl;
        std::cout << "Absolute values: ";
        for (size_t i = 0; i < abs_vals.size(); ++i) {
            std::cout << abs_vals[i];
            if (i < abs_vals.size() - 1) std::cout << " ";
        }
        std::cout << std::endl;
        
        std::cout << "Intervals: ";
        for (size_t i = 0; i < int_vals.size(); ++i) {
            std::cout << int_vals[i];
            if (i < int_vals.size() - 1) std::cout << " ";
        }
        std::cout << std::endl;
        
        if (retrograde_enabled_ && voices_ >= 2) {
            std::cout << "Retrograde inversion center: " << inversion_center_;
            std::cout << " (✓ Valid: " << (test_retrograde_inversion() ? "Yes" : "No") << ")" << std::endl;
        }
        
        if (!compiled_rules_.empty()) {
            std::cout << "Compiled rules: " << compiled_rules_.size() << " rules loaded" << std::endl;
            std::cout << "Rule evaluation: " << (evaluate_compiled_rules() ? "✓ All passed" : "✗ Failed") << std::endl;
        }
    }
};

} // namespace GecodeClusterIntegration