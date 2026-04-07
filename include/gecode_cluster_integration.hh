/**
 * @file gecode_cluster_integration.hh  
 * @brief Complete Gecode Integration for Advanced Cluster Functionality
 * 
 * Integrates all cluster-engine features with Gecode constraint programming:
 * - Dynamic Musical Rules as Gecode Propagators
 * - Advanced Backjumping with Gecode Search
 * - Dual Solution Storage with IntVar Arrays
 * - Musical Constraints with Real-time Propagation
 */

#ifndef GECODE_CLUSTER_INTEGRATION_HH
#define GECODE_CLUSTER_INTEGRATION_HH

#include <gecode/driver.hh>
#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <gecode/search.hh>
#include "enhanced_rule_architecture.hh"
#include "advanced_backjumping_strategies.hh"
#include "dual_solution_storage.hh"
#include <memory>
#include <vector>
#include <map>

using namespace Gecode;

namespace GecodeClusterIntegration {

// Forward declarations and base classes for compiled rule system
class MusicalRule {
public:
    enum class RuleType {
        SINGLE_ENGINE_RHYTHM,
        SINGLE_ENGINE_PITCH,
        DUAL_ENGINE_RHYTHM_PITCH,
        MULTI_ENGINE_HARMONIC,
        RETROGRADE_INVERSION,
        CUSTOM
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
    virtual bool is_enabled() const { return enabled_; }
    virtual std::string get_name() const { return name_; }
    virtual RuleType get_type() const { return type_; }
    virtual const std::vector<int>& get_target_engines() const { return target_engines_; }
    virtual const std::vector<int>& get_backtrack_route() const { return backtrack_route_; }
    virtual int get_priority() const { return priority_; }
    virtual void set_enabled(bool enabled) { enabled_ = enabled; }
};

class RuleCompiler;

// ===============================
// Musical Rule Propagator
// ===============================

/**
 * @brief Gecode Propagator that implements MusicalRule constraint checking
 * 
 * Converts cluster-engine musical rules into efficient Gecode propagators
 * with proper domain pruning and failure detection.
 */
class MusicalRulePropagator : public Propagator {
protected:
    /// Array of note variables (absolute values)
    ViewArray<Int::IntView> notes_;
    /// Array of interval variables
    ViewArray<Int::IntView> intervals_;
    /// The musical rule to propagate
    std::shared_ptr<MusicalConstraints::MusicalRule> rule_;
    /// Current variable index being processed
    int current_index_;
    /// Dual solution storage for rule checking
    mutable MusicalConstraints::DualSolutionStorage* solution_storage_;

public:
    /**
     * @brief Constructor for musical rule propagator
     */
    MusicalRulePropagator(Space& home, 
                         ViewArray<Int::IntView> notes,
                         ViewArray<Int::IntView> intervals,
                         std::shared_ptr<MusicalConstraints::MusicalRule> rule,
                         int index);

    /**
     * @brief Constructor for cloning
     */
    MusicalRulePropagator(Space& home, MusicalRulePropagator& prop);

    /**
     * @brief Main propagation function
     */
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med);

    /**
     * @brief Copy propagator during cloning
     */
    virtual Propagator* copy(Space& home);

    /**
     * @brief Cost estimation for propagation scheduling
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;

    /**
     * @brief Reschedule propagator (required by Gecode)
     */
    virtual void reschedule(Space& home);

    /**
     * @brief Size footprint
     */
    virtual size_t dispose(Space& home);

    /**
     * @brief Static posting function for the constraint
     */
    static ExecStatus post(Space& home, 
                          ViewArray<Int::IntView> notes,
                          ViewArray<Int::IntView> intervals,
                          std::shared_ptr<MusicalConstraints::MusicalRule> rule,
                          int index);

private:
    /**
     * @brief Update solution storage from current variable domains
     */
    void update_solution_storage(Space& home) const;

    /**
     * @brief Apply rule result to variable domains
     */
    ExecStatus apply_rule_result(Space& home, const MusicalConstraints::RuleResult& result);
};

// ===============================
// Retrograde Inversion Propagator
// ===============================

/**
 * @brief Gecode Propagator for Retrograde Inversion Constraint
 * 
 * Enforces that Voice 2 is the exact retrograde inversion of Voice 1:
 * Voice2[i] = 2 * inversion_center - Voice1[length-1-i]
 */
class RetrogradeInversionPropagator : public Propagator {
public:
    /**
     * @brief Static posting function for the retrograde inversion constraint
     */
    static ExecStatus post(Space& home,
                          ViewArray<Int::IntView> voice1,
                          ViewArray<Int::IntView> voice2,
                          int center, int length);
};

// ===============================
// Advanced Backjump Brancher
// ===============================

/**
 * @brief Gecode Brancher with Advanced Backjumping Strategies
 * 
 * Implements cluster-engine v4.05 backjumping modes within Gecode's
 * search framework for intelligent constraint solving.
 */
class AdvancedBackjumpBrancher : public Brancher {
protected:
    /// Note variables for branching
    ViewArray<Int::IntView> notes_;
    /// Interval variables 
    ViewArray<Int::IntView> intervals_;
    /// Backjump strategy coordinator
    std::unique_ptr<AdvancedBackjumping::BackjumpStrategyCoordinator> coordinator_;
    /// Musical rules for analysis
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules_;
    /// Current branching index
    mutable int current_index_;

public:
    /**
     * @brief Constructor for backjump brancher
     */
    AdvancedBackjumpBrancher(Space& home,
                           ViewArray<Int::IntView> notes,
                           ViewArray<Int::IntView> intervals,
                           std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules,
                           AdvancedBackjumping::BackjumpMode mode);

    /**
     * @brief Constructor for cloning
     */
    AdvancedBackjumpBrancher(Space& home, AdvancedBackjumpBrancher& brancher);

    /**
     * @brief Check if branching is available
     */
    virtual bool status(const Space& home) const;

    /**
     * @brief Create choice for branching
     */
    virtual Choice* choice(Space& home);

    /**
     * @brief Create choice for cloning
     */
    virtual Choice* choice(const Space& home, Archive& e);

    /**
     * @brief Commit to a choice
     */
    virtual ExecStatus commit(Space& home, const Choice& c, unsigned int a);

    /**
     * @brief Copy brancher during cloning
     */
    virtual Brancher* copy(Space& home);

    /**
     * @brief Print branching choice
     */
    virtual void print(const Space& home, const Choice& c, unsigned int a,
                      std::ostream& o) const;

    /**
     * @brief Static posting function
     */
    static void post(Space& home,
                    ViewArray<Int::IntView> notes,
                    ViewArray<Int::IntView> intervals,
                    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules,
                    AdvancedBackjumping::BackjumpMode mode);
};

/**
 * @brief Choice for advanced backjump branching
 */
class AdvancedBackjumpChoice : public Choice {
public:
    /// Variable index to branch on
    int var_index;
    /// Value to try
    int value;
    /// Backjump analysis result
    AdvancedBackjumping::AdvancedBackjumpResult backjump_result;

    AdvancedBackjumpChoice(const Brancher& b, int var_idx, int val,
                          const AdvancedBackjumping::AdvancedBackjumpResult& result);

    /// Archive for cloning
    virtual size_t size() const { return sizeof(AdvancedBackjumpChoice); }
    virtual void archive(Archive& e) const;
};

// ===============================
// Integrated Musical Space
// ===============================

/**
 * @brief Complete Gecode Space with Integrated Cluster Functionality
 * 
 * Combines all cluster-engine features into a production-ready
 * Gecode constraint solver for musical generation.
 */
class IntegratedMusicalSpace : public Space {
protected:
    /// Number of notes in sequence
    int sequence_length_;
    /// Number of voices
    int num_voices_;
    /// Note variables (MIDI values)
    IntVarArray absolute_vars_;
    /// Interval variables (semitone differences)
    IntVarArray interval_vars_;
    /// Musical rules for constraint solving
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> musical_rules_;
    /// Compiled rule system from Cluster sources
    std::vector<std::unique_ptr<GecodeClusterIntegration::MusicalRule>> compiled_rules_;
    /// Backjumping mode
    AdvancedBackjumping::BackjumpMode backjump_mode_;
    /// Dual solution storage
    std::unique_ptr<MusicalConstraints::DualSolutionStorage> solution_storage_;
    /// Flag for multi-voice configuration
    bool vocal_space_configured_;

public:
    /**
     * @brief Constructor for integrated musical space
     */
    IntegratedMusicalSpace(int length, int voices, 
                          AdvancedBackjumping::BackjumpMode mode = 
                          AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP,
                          const std::vector<int>& note_domain = {});

    /**
     * @brief Constructor for cloning
     */
    IntegratedMusicalSpace(IntegratedMusicalSpace& space);

    /**
     * @brief Copy space during cloning
     */
    virtual Space* copy();

    /**
     * @brief Constrain space during optimization
     */
    virtual void constrain(const Space& best);

    // ================================
    // Musical Rule Management
    // ================================

    /**
     * @brief Add musical rule to constraint system
     */
    void add_musical_rule(std::shared_ptr<MusicalConstraints::MusicalRule> rule);

    /**
     * @brief Add multiple rules at once
     */
    void add_musical_rules(const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules);

    /**     * @brief Add compiled musical rule from Cluster rule system
     */
    void add_compiled_musical_rule(std::unique_ptr<GecodeClusterIntegration::MusicalRule> rule);

    /**     * @brief Set backjumping mode for search
     */
    void set_backjump_mode(AdvancedBackjumping::BackjumpMode mode);

    /**
     * @brief Constrain note variables to a specific MIDI range
     */
    void constrain_note_range(int min_note, int max_note);
    
    /**
     * @brief Add retrograde inversion constraint between voices
     * @param inversion_center MIDI note value used as inversion center
     */
    void add_retrograde_inversion_constraint(int inversion_center);
    
    /**
     * @brief Post 12-tone row constraint (all different pitches in voice 0)
     */
    void post_twelve_tone_row_constraint();
    
    /**
     * @brief Post perfect fifth interval constraint between consecutive notes in voice 0
     */
    void post_perfect_fifth_intervals_constraint();
    
    /**
     * @brief Post palindrome constraint (voice 1 is reverse of voice 0)
     */
    void post_palindrome_voices_constraint();

    // ================================
    // Solution Access
    // ================================

    /**
     * @brief Get current musical sequence
     */
    std::vector<int> get_absolute_sequence() const;

    /**
     * @brief Get interval sequence
     */
    std::vector<int> get_interval_sequence() const;

    /**
     * @brief Print musical solution
     */
    void print_musical_solution(std::ostream& os = std::cout) const;

    /**
     * @brief Export to DualSolutionStorage for compatibility
     */
    MusicalConstraints::DualSolutionStorage export_to_dual_storage() const;

    // ================================
    // Variable Access (for Dynamic Rules)
    // ================================
    
    /**
     * @brief Get absolute pitch variables
     */
    IntVarArray& get_absolute_vars() { return absolute_vars_; }
    
    /**
     * @brief Get interval variables 
     */
    IntVarArray& get_interval_vars() { return interval_vars_; }

    /**
     * @brief Get rhythm sequence for specific voice
     */
    std::vector<int> get_rhythm_sequence(int voice) const;

    /**
     * @brief Get pitch sequence for specific voice  
     */
    std::vector<int> get_pitch_sequence(int voice) const;

    /**
     * @brief Get metric sequence (time signatures)
     */
    std::vector<int> get_metric_sequence() const;

    /**
     * @brief Get number of engines (for debugging)
     */
    int get_num_engines() const;

private:
    /**
     * @brief Initialize constraint posting
     */
    void post_musical_constraints();

    /**
     * @brief Setup advanced backjump branching
     */
    void setup_backjump_branching();
};

// ===============================
// High-level Integration Functions
// ===============================

/**
 * @brief Create fully integrated musical solver
 */
class IntegratedMusicalSolver : public Script {
protected:
    /// The integrated musical space
    std::unique_ptr<IntegratedMusicalSpace> musical_space_;
    /// Configuration options
    struct SolverOptions {
        int sequence_length = 8;
        int num_voices = 1;
        AdvancedBackjumping::BackjumpMode backjump_mode = 
            AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
        bool enable_musical_rules = true;
        bool enable_advanced_backjumping = true;
        int min_note = 60;  // C4
        int max_note = 84;  // C6
    } options_;

public:
    /**
     * @brief Constructor with options
     */
    IntegratedMusicalSolver(const Options& opt);

    /**
     * @brief Copy constructor for cloning
     */
    IntegratedMusicalSolver(IntegratedMusicalSolver& solver);

    /**
     * @brief Copy during cloning
     */
    virtual Space* copy();

    /**
     * @brief Print solution
     */
    virtual void print(std::ostream& os) const;

    /**
     * @brief Configure solver options
     */
    void configure(const SolverOptions& opts);

    /**
     * @brief Add custom musical rules
     */
    void add_rule(std::shared_ptr<MusicalConstraints::MusicalRule> rule);
};

/**
 * @brief Factory for creating common musical rule sets
 */
class MusicalRuleFactory {
public:
    /**
     * @brief Create basic musical rules (no repetition, reasonable intervals)
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> 
    create_basic_rules();

    /**
     * @brief Create advanced jazz harmony rules
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> 
    create_jazz_rules();

    /**
     * @brief Create classical voice leading rules
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> 
    create_voice_leading_rules();

    /**
     * @brief Create cluster-engine compatible rule set
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> 
    create_cluster_engine_rules();
};

} // namespace GecodeClusterIntegration

#endif // GECODE_CLUSTER_INTEGRATION_HH