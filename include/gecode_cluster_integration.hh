/**
 * @file gecode_cluster_integration.hh
 * @brief Header file for Gecode Integration with Cluster Functionality
 * 
 * Provides musical rule compilation system integrated with Gecode constraints.
 */

#pragma once

#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <gecode/search.hh>
#include "advanced_backjumping_strategies.hh"
#include "dual_solution_storage.hh"
#include "enhanced_rule_architecture.hh"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <ostream>

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

    // Musical variables
    IntVarArray absolute_vars_;
    IntVarArray interval_vars_;
    IntVarArray rhythm_vars_;  // Per-voice duration variables (empty if not configured)
    
    // Per-voice state
    int num_voices_;
    AdvancedBackjumping::BackjumpMode backjump_mode_;
    std::unique_ptr<MusicalConstraints::DualSolutionStorage> solution_storage_;
    bool vocal_space_configured_;

    // Musical rules (MusicalConstraints layer)
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> musical_rules_;

    // Compiled rule system (GecodeClusterIntegration layer)
    std::vector<std::unique_ptr<MusicalRule>> compiled_rules_;

    // Internal helpers
    void post_musical_constraints();
    void add_musical_rule(std::shared_ptr<MusicalConstraints::MusicalRule> rule);

public:
    // Constructor (Gecode 6: no bool share)
    // Single global domain for all voices
    IntegratedMusicalSpace(int length, int voices,
                           AdvancedBackjumping::BackjumpMode mode,
                           const std::vector<int>& note_domain,
                           unsigned int random_seed = 0);

    // Constructor with per-voice domains (voice_domains[v] = domain for voice v)
    IntegratedMusicalSpace(int length, int voices,
                           AdvancedBackjumping::BackjumpMode mode,
                           const std::vector<std::vector<int>>& voice_domains,
                           unsigned int random_seed = 0);

    // Constructor with per-voice pitch AND rhythm domains
    IntegratedMusicalSpace(int length, int voices,
                           AdvancedBackjumping::BackjumpMode mode,
                           const std::vector<std::vector<int>>& voice_domains,
                           const std::vector<std::vector<int>>& voice_rhythm_domains,
                           unsigned int random_seed = 0);

    // Copy constructor for search (Gecode 6: single argument)
    IntegratedMusicalSpace(IntegratedMusicalSpace& s);

    // Copy method for search (Gecode 6: no bool share)
    virtual Space* copy() override;

    // Optimization constraint hook
    virtual void constrain(const Space& best) override;

    // ---- Accessors ----
    IntVarArray& get_absolute_vars() { return absolute_vars_; }
    IntVarArray& get_interval_vars() { return interval_vars_; }
    IntVarArray& get_rhythm_vars() { return rhythm_vars_; }
    const IntVarArray& get_absolute_vars() const { return absolute_vars_; }
    const IntVarArray& get_interval_vars() const { return interval_vars_; }
    const IntVarArray& get_rhythm_vars() const { return rhythm_vars_; }
    bool has_rhythm_vars() const { return rhythm_vars_.size() > 0; }

    // ---- Rule system ----
    void add_musical_rules(const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules);
    void add_compiled_musical_rule(std::unique_ptr<MusicalRule> rule);
    void apply_compiled_rule_constraints(const MusicalRule& rule);
    bool evaluate_compiled_rules() const;
    void set_backjump_mode(AdvancedBackjumping::BackjumpMode mode);

    // ---- Specific musical constraints ----
    void post_twelve_tone_row_constraint();
    void post_perfect_fifth_intervals_constraint();
    void post_palindrome_voices_constraint();
    void add_retrograde_inversion_constraint(int inversion_center);
    void constrain_note_range(int min_note, int max_note);

    // ---- Solution access ----
    std::vector<int> get_absolute_sequence() const;
    std::vector<int> get_interval_sequence() const;
    std::vector<int> get_pitch_sequence(int voice) const;
    std::vector<int> get_rhythm_sequence_from_vars(int voice) const;
    std::vector<int> get_rhythm_sequence(int voice) const;
    std::vector<int> get_metric_sequence() const;
    MusicalConstraints::DualSolutionStorage export_to_dual_storage() const;
    void print_musical_solution(std::ostream& os = std::cout) const;

    // JSON interface
    void add_rule_from_json(const std::string& rule_type, const std::map<std::string, int>& parameters);
};

} // namespace GecodeClusterIntegration