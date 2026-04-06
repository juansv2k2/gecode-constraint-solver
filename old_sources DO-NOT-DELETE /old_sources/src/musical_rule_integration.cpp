/**
 * @file musical_rule_integration.cpp
 * @brief Complete dynamic musical rule integration system for MusicalSpace
 * 
 * This file implements the complete musical rule integration framework that
 * allows dynamic musical rules to be passed to the solver at runtime and
 * automatically converted to Gecode constraints.
 */

#include "musical_space.hh"
#include "enhanced_rule_architecture.hh"
#include "cluster_engine_musical_rules.hh"
#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <iostream>
#include <memory>
#include <functional>

using namespace Gecode;

namespace ClusterEngine {

// ===============================
// MusicalPropagator Implementation
// ===============================

MusicalPropagator::MusicalPropagator(Home home, const IntVarArgs& abs_vars, 
                                   const IntVarArgs& int_vars,
                                   std::shared_ptr<MusicalRule> rule)
    : Propagator(home), absolute_views_(home, abs_vars), interval_views_(home, int_vars), 
      musical_rule_(rule) {
}

MusicalPropagator::MusicalPropagator(Space& home, const MusicalPropagator& p)
    : Propagator(home, p), musical_rule_(p.musical_rule_) {
    absolute_views_.update(home, p.absolute_views_);
    interval_views_.update(home, p.interval_views_);
}

Propagator* MusicalPropagator::copy(Space& home) {
    return new (home) MusicalPropagator(home, *this);
}

PropCost MusicalPropagator::cost(const Space& home, const ModEventDelta& med) const {
    // Cost is proportional to number of variables involved
    int num_vars = absolute_views_.size() + interval_views_.size();
    if (num_vars <= 3) return PropCost::unary(PropCost::LO);
    if (num_vars <= 6) return PropCost::binary(PropCost::LO);
    return PropCost::linear(PropCost::LO, num_vars);
}

ExecStatus MusicalPropagator::propagate(Space& home, const ModEventDelta& med) {
    // Convert current state to DualSolutionStorage for rule checking
    DualSolutionStorage storage;
    
    // Extract assigned values and add to storage
    for (int i = 0; i < absolute_views_.size(); ++i) {
        if (absolute_views_[i].assigned()) {
            int abs_val = absolute_views_[i].val();
            int int_val = (i > 0 && interval_views_[i].assigned()) ? 
                         interval_views_[i].val() : 0;
            storage.add_candidate(i, DualCandidate(abs_val, int_val));
        }
    }
    
    // Check rule for constraint violations
    RuleResult result = musical_rule_->check_rule(storage, -1);
    
    if (!result.passes) {
        return ES_FAILED;  // Rule violated - constraint failure
    }
    
    // Apply domain reductions based on rule constraints
    bool modified = false;
    
    // If rule provides domain restrictions, apply them
    for (const auto& suggestion : result.backjump_suggestions) {
        int var_idx = suggestion.variable_index;
        if (var_idx >= 0 && var_idx < absolute_views_.size()) {
            
            // Apply absolute domain restrictions if provided
            if (!suggestion.allowed_absolute_values.empty()) {
                IntSet allowed_set(suggestion.allowed_absolute_values.data(), 
                                 static_cast<int>(suggestion.allowed_absolute_values.size()));
                ModEvent me = absolute_views_[var_idx].inter(home, allowed_set);
                if (me_failed(me)) return ES_FAILED;
                if (me_modified(me)) modified = true;
            }
            
            // Apply interval domain restrictions if provided  
            if (!suggestion.allowed_interval_values.empty() && 
                var_idx < interval_views_.size()) {
                IntSet allowed_interval_set(suggestion.allowed_interval_values.data(),
                                          static_cast<int>(suggestion.allowed_interval_values.size()));
                ModEvent me = interval_views_[var_idx].inter(home, allowed_interval_set);
                if (me_failed(me)) return ES_FAILED;
                if (me_modified(me)) modified = true;
            }
        }
    }
    
    // Check if all variables are assigned
    bool all_assigned = true;
    for (int i = 0; i < absolute_views_.size(); ++i) {
        if (!absolute_views_[i].assigned()) {
            all_assigned = false;
            break;
        }
    }
    
    if (all_assigned) {
        // Final rule check with complete assignment
        RuleResult final_result = musical_rule_->check_rule(storage, -1);
        return final_result.passes ? ES_SUBSUMED : ES_FAILED;
    }
    
    return modified ? ES_NOFIX : ES_FIX;
}

ExecStatus MusicalPropagator::post(Home home, const IntVarArgs& abs_vars, 
                                  const IntVarArgs& int_vars,
                                  std::shared_ptr<MusicalRule> rule) {
    (void) new (home) MusicalPropagator(home, abs_vars, int_vars, rule);
    return ES_OK;
}

// ===============================
// Dynamic Rule Processing Implementation
// ===============================

/**
 * @brief Rule type detector for dynamic rule processing
 */
class DynamicRuleProcessor {
private:
    MusicalSpace* space_;
    
public:
    explicit DynamicRuleProcessor(MusicalSpace* space) : space_(space) {}
    
    /**
     * @brief Process any type of musical rule dynamically
     */
    bool process_musical_rule(std::shared_ptr<MusicalRule> rule) {
        if (!rule) return false;
        
        std::string rule_type = rule->rule_type();
        
        // Route to appropriate processing method based on rule type
        if (rule_type == "IndexRule") {
            return process_index_rule(rule);
        }
        else if (rule_type == "MotifRule") {
            return process_motif_rule(rule);
        }
        else if (rule_type == "RhythmPitchRule") {
            return process_rhythm_pitch_rule(rule);
        }
        else if (rule_type == "HarmonicRule") {
            return process_harmonic_rule(rule);
        }
        else if (rule_type == "VoiceLeadingRule") {
            return process_voice_leading_rule(rule);
        }
        else if (rule_type == "ScaleRule") {
            return process_scale_rule(rule);
        }
        else {
            // Generic propagator for unknown rule types
            return process_generic_rule(rule);
        }
    }
    
private:
    /**
     * @brief Process index-based rules (specific variable constraints)
     */
    bool process_index_rule(std::shared_ptr<MusicalRule> rule) {
        // Get all variables involved in this rule
        std::vector<int> dependent_vars = rule->get_dependent_variables(-1);
        
        if (dependent_vars.empty()) return false;
        
        // Create variable arguments for the rule
        IntVarArgs abs_vars, int_vars;
        for (int idx : dependent_vars) {
            abs_vars << space_->get_absolute_var(idx);
            int_vars << space_->get_interval_var(idx);
        }
        
        // Post the musical propagator
        return MusicalPropagator::post(*space_, abs_vars, int_vars, rule) == ES_OK;
    }
    
    /**
     * @brief Process motif rules (sequence patterns)
     */
    bool process_motif_rule(std::shared_ptr<MusicalRule> rule) {
        // Motif rules typically apply to contiguous sequences
        // We'll apply the rule as a propagator over the full sequence
        
        IntVarArgs abs_vars = space_->get_absolute_vars();
        IntVarArgs int_vars = space_->get_interval_vars();
        
        return MusicalPropagator::post(*space_, abs_vars, int_vars, rule) == ES_OK;
    }
    
    /**
     * @brief Process rhythm-pitch coordination rules
     */
    bool process_rhythm_pitch_rule(std::shared_ptr<MusicalRule> rule) {
        // Get rhythm and pitch engine variables
        // For now, apply to all variables (could be optimized)
        
        IntVarArgs abs_vars = space_->get_absolute_vars();
        IntVarArgs int_vars = space_->get_interval_vars();
        
        return MusicalPropagator::post(*space_, abs_vars, int_vars, rule) == ES_OK;
    }
    
    /**
     * @brief Process harmonic rules (chord and interval constraints)
     */
    bool process_harmonic_rule(std::shared_ptr<MusicalRule> rule) {
        // Apply to relevant voice variables
        std::vector<int> dependent_vars = rule->get_dependent_variables(-1);
        
        IntVarArgs abs_vars, int_vars;
        if (dependent_vars.empty()) {
            // Apply to all variables if no specific dependencies
            abs_vars = space_->get_absolute_vars();
            int_vars = space_->get_interval_vars();
        } else {
            for (int idx : dependent_vars) {
                abs_vars << space_->get_absolute_var(idx);
                int_vars << space_->get_interval_var(idx);
            }
        }
        
        return MusicalPropagator::post(*space_, abs_vars, int_vars, rule) == ES_OK;
    }
    
    /**
     * @brief Process voice leading rules (voice movement constraints)
     */
    bool process_voice_leading_rule(std::shared_ptr<MusicalRule> rule) {
        // Voice leading rules typically apply to consecutive variables in voices
        std::vector<int> dependent_vars = rule->get_dependent_variables(-1);
        
        IntVarArgs abs_vars, int_vars;
        for (int idx : dependent_vars) {
            abs_vars << space_->get_absolute_var(idx);
            int_vars << space_->get_interval_var(idx);
        }
        
        return MusicalPropagator::post(*space_, abs_vars, int_vars, rule) == ES_OK;
    }
    
    /**
     * @brief Process scale rules (modal and tonal constraints)
     */
    bool process_scale_rule(std::shared_ptr<MusicalRule> rule) {
        // Scale rules typically apply to all pitch variables
        IntVarArgs abs_vars = space_->get_absolute_vars();
        IntVarArgs int_vars = space_->get_interval_vars();
        
        return MusicalPropagator::post(*space_, abs_vars, int_vars, rule) == ES_OK;
    }
    
    /**
     * @brief Process generic rules (fallback for unknown types)
     */
    bool process_generic_rule(std::shared_ptr<MusicalRule> rule) {
        // Apply as generic propagator to all variables
        IntVarArgs abs_vars = space_->get_absolute_vars();
        IntVarArgs int_vars = space_->get_interval_vars();
        
        return MusicalPropagator::post(*space_, abs_vars, int_vars, rule) == ES_OK;
    }
};

// ===============================
// Enhanced Rule Integration Methods for MusicalSpace
// ===============================

void MusicalSpace::post_musical_rule(std::shared_ptr<MusicalRule> rule) {
    if (!rule) return;
    
    // Store the rule for later reference
    active_rules_.push_back(rule);
    
    // Process the rule dynamically
    DynamicRuleProcessor processor(this);
    bool success = processor.process_musical_rule(rule);
    
    if (!success) {
        std::cerr << "Warning: Failed to process musical rule: " 
                  << rule->description() << std::endl;
    }
}

/**
 * @brief Post multiple musical rules in batch
 */
void MusicalSpace::post_musical_rules(const std::vector<std::shared_ptr<MusicalRule>>& rules) {
    for (auto rule : rules) {
        post_musical_rule(rule);
    }
}

/**
 * @brief Remove and replace all current musical rules
 */
void MusicalSpace::set_musical_rules(const std::vector<std::shared_ptr<MusicalRule>>& rules) {
    // Note: In Gecode, we can't actually remove propagators once posted,
    // but we can clear the active_rules_ list for tracking purposes
    active_rules_.clear();
    
    // Post all new rules
    post_musical_rules(rules);
}

/**
 * @brief Get currently active musical rules
 */
const std::vector<std::shared_ptr<MusicalRule>>& MusicalSpace::get_active_rules() const {
    return active_rules_;
}

/**
 * @brief Check if a specific rule type is currently active
 */
bool MusicalSpace::has_rule_type(const std::string& rule_type) const {
    for (const auto& rule : active_rules_) {
        if (rule && rule->rule_type() == rule_type) {
            return true;
        }
    }
    return false;
}

} // namespace ClusterEngine