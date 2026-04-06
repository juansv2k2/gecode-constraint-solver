#ifndef MUSICAL_SPACE_HH
#define MUSICAL_SPACE_HH

#include <gecode/int.hh>
#include <gecode/search.hh>
#include <vector>
#include <memory>
#include "cluster_engine_interface.hh"
#include "musical_utilities.hh"

using namespace Gecode;

namespace ClusterEngine {

// Forward declarations
class MusicalRule;
class MusicalUtilities;

/**
 * @brief Domain type constants (from cluster-engine v4.05)
 * These define how musical values are interpreted and stored
 */
enum class DomainType : int {
    BASIC_DOMAIN = -1,      // Basic values (symbols, objects)
    INTERVAL_DOMAIN = 0,    // Melodic intervals, durations, differences  
    ABSOLUTE_DOMAIN = 1,    // Absolute pitches, timepoints, numerical values
    ABSOLUTE_RHYTHM_DOMAIN = 2,  // Rhythm with absolute timepoints
    DURATION_DOMAIN = 3     // Duration values with accumulation
};

/**
 * @brief Dual representation candidate for musical variables
 * Stores both absolute value and interval/duration in single structure
 */
struct DualCandidate {
    int absolute_value;  // Pitch, timepoint, etc.
    int interval_value;  // Melodic interval, duration, etc.
    
    DualCandidate(int abs = 0, int interval = 0) 
        : absolute_value(abs), interval_value(interval) {}
};

/**
 * @brief Musical engine information for multi-engine coordination
 * Based on cluster-engine's engine coordination system
 */
struct MusicalEngineInfo {
    int engine_id;
    EngineType type;          // RHYTHM, PITCH, METRIC
    int partner_engine_id;    // Coordinated engine (-1 if none)
    int voice;               // Musical voice number (-1 if global)
    DomainType domain_type;  // How values are interpreted
    
    MusicalEngineInfo(int id, EngineType t, int partner = -1, int v = -1, 
                      DomainType dt = DomainType::ABSOLUTE_DOMAIN)
        : engine_id(id), type(t), partner_engine_id(partner), voice(v), domain_type(dt) {}
};

/**
 * @brief MusicalSpace - Gecode Space for musical constraint satisfaction
 * 
 * This class implements the core Gecode Space with dual solution representation
 * from cluster-engine v4.05. It provides:
 * - IntVar arrays for musical variables
 * - Dual absolute/interval access methods
 * - Multi-engine coordination for rhythm/pitch/metric
 * - Integration with existing MusicalRule and MusicalUtilities systems
 * - True constraint propagation through Gecode's propagator system
 */
class MusicalSpace : public Space {
private:
    // Core Gecode variables
    IntVarArray absolute_vars_;    // Main variable array (absolute values)
    IntVarArray interval_vars_;    // Auxiliary variables (intervals/durations)
    
    // Musical engine coordination
    std::vector<MusicalEngineInfo> engine_info_;
    int num_voices_;
    int num_engines_;
    
    // Domain and constraint management  
    std::vector<DomainType> variable_domains_;
    std::vector<std::shared_ptr<MusicalRule>> active_rules_;
    
    // Musical intelligence integration
    std::unique_ptr<MusicalUtilities> musical_utils_;
    
    // Internal state management
    bool initialized_;
    int current_search_index_;
    
    // Helper methods for dual representation
    void initialize_dual_variables(int num_variables);
    void setup_engine_coordination();
    void link_absolute_interval_constraints();
    
    // Domain initialization methods
    void initialize_variable_domain(int var_idx, const std::vector<int>& domain, DomainType type);
    void initialize_uniform_domains(const std::vector<int>& domain, DomainType type);
    void initialize_dynamic_domains(const std::vector<std::vector<int>>& domains, 
                                   const std::vector<DomainType>& types);
    
public:
    // Constructors
    MusicalSpace(int num_variables, int num_voices);
    MusicalSpace(const MusicalSpace& space);  // Copy constructor for search
    
    // Gecode Space interface
    virtual Space* copy() override;
    virtual void constrain(const Space& best) override;
    
    // Musical engine setup
    void add_engine(EngineType type, int voice = -1, DomainType domain_type = DomainType::ABSOLUTE_DOMAIN);
    void set_engine_partner(int engine1_id, int engine2_id);  // Rhythm-pitch coordination
    
    // Domain initialization (cluster-engine v4.05 style)
    void initialize_domains(const std::vector<int>& uniform_domain, DomainType type = DomainType::ABSOLUTE_DOMAIN);
    void initialize_domains(const std::vector<std::vector<int>>& domains, 
                           const std::vector<DomainType>& types);
    void randomize_domains(bool enable = true);
    
    // Dual representation access (cluster-engine v4.05 macros translated)
    int get_absolute_value(int var_idx) const;              // a() macro equivalent
    int get_interval_value(int var_idx) const;              // i() or d() macro equivalent  
    int get_basic_value(int var_idx) const;                 // b() macro equivalent
    
    // Multi-variable access for musical sequences
    std::vector<int> get_absolute_sequence(int start_idx, int length) const;
    std::vector<int> get_interval_sequence(int start_idx, int length) const;
    
    // Musical constraint posting
    void post_musical_rule(std::shared_ptr<MusicalRule> rule);
    void post_musical_rules(const std::vector<std::shared_ptr<MusicalRule>>& rules);
    void set_musical_rules(const std::vector<std::shared_ptr<MusicalRule>>& rules);
    void post_coordination_constraints();  // Link rhythm-pitch engines
    void post_interval_calculation_constraints();  // Link absolute-interval relationships
    
    // Advanced musical constraint propagators
    void post_consonance_constraints(int consonance_threshold = 1);  // 0=perfect, 1=consonant, 2=mild dissonance
    void post_voice_leading_constraints(int voice1_start, int voice2_start, int length);
    void post_melodic_contour_constraints(int voice_start, int length);
    void post_harmonic_rhythm_constraints();
    
    // Enhanced musical constraints
    void post_scale_constraints(const std::vector<int>& scale_degrees, int root_note = 60);
    void post_cadential_constraints();
    void post_rhythmic_consistency_constraints();
    void post_range_constraints(int min_pitch = 36, int max_pitch = 84);
    
    // Engine coordination methods
    int get_engine_count() const { return num_engines_; }
    int get_voice_count() const { return num_voices_; }
    EngineType get_engine_type(int engine_id) const;
    int get_engine_partner(int engine_id) const;
    int get_engine_voice(int engine_id) const;
    
    // Musical intelligence integration
    void set_musical_utilities(std::unique_ptr<MusicalUtilities> utils);
    MusicalUtilities* get_musical_utilities() const;
    
    // Solution analysis and extraction
    bool is_complete_solution() const;
    std::vector<DualCandidate> extract_solution() const;
    void print_musical_solution() const;
    
    // Search state management
    void set_search_index(int index) { current_search_index_ = index; }
    int get_search_index() const { return current_search_index_; }
    
    // Variable access for constraints and branching
    IntVar get_absolute_var(int idx) const;
    IntVar get_interval_var(int idx) const;
    IntVarArgs get_absolute_vars() const;
    IntVarArgs get_interval_vars() const;
    
    // Musical rule management
    const std::vector<std::shared_ptr<MusicalRule>>& get_active_rules() const;
    bool has_rule_type(const std::string& rule_type) const;
    size_t get_active_rule_count() const { return active_rules_.size(); }
    
    // Engine-based variable access
    IntVarArgs get_engine_variables(int engine_id) const;
    std::vector<int> get_engine_variable_indices(int engine_id) const;
};

/**
 * @brief Musical propagator base class for converting MusicalRule to Gecode
 * 
 * This bridges our existing MusicalRule system with Gecode's propagator architecture
 */
class MusicalPropagator : public Propagator {
protected:
    ViewArray<Int::IntView> absolute_views_;
    ViewArray<Int::IntView> interval_views_;
    std::shared_ptr<MusicalRule> musical_rule_;
    
public:
    MusicalPropagator(Home home, const IntVarArgs& abs_vars, const IntVarArgs& int_vars,
                      std::shared_ptr<MusicalRule> rule);
    MusicalPropagator(Space& home, const MusicalPropagator& p);
    
    virtual Propagator* copy(Space& home) override;
    virtual ExecStatus propagate(Space& home, const ModEventDelta& med) override;
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const override;
    
    static ExecStatus post(Home home, const IntVarArgs& abs_vars, const IntVarArgs& int_vars,
                          std::shared_ptr<MusicalRule> rule);
};

/**
 * @brief Musical branching strategy for search optimization
 * Based on cluster-engine's heuristic rule system
 */
class MusicalBranching : public Brancher {
private:
    ViewArray<Int::IntView> absolute_views_;
    ViewArray<Int::IntView> interval_views_;
    std::vector<std::shared_ptr<MusicalRule>> heuristic_rules_;
    
public:
    MusicalBranching(Home home, const IntVarArgs& abs_vars, const IntVarArgs& int_vars);
    MusicalBranching(Space& home, const MusicalBranching& b);
    
    virtual Brancher* copy(Space& home) override;
    virtual bool status(const Space& home) const override;
    virtual Choice* choice(Space& home) override;
    virtual Choice* choice(const Space& home, Archive& e) override;
    virtual ExecStatus commit(Space& home, const Choice& c, unsigned int a) override;
    
    void add_heuristic_rule(std::shared_ptr<MusicalRule> rule);
    
    static void post(Home home, const IntVarArgs& abs_vars, const IntVarArgs& int_vars);
};

} // namespace ClusterEngine

#endif // MUSICAL_SPACE_HH