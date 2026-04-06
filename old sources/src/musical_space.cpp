#include "musical_space.hh"
#include <gecode/minimodel.hh>
#include <iostream>
#include <algorithm>
#include <random>

using namespace Gecode;

namespace ClusterEngine {

// ===============================
// MusicalSpace Implementation
// ===============================

MusicalSpace::MusicalSpace(int num_variables, int num_voices) 
    : num_voices_(num_voices), num_engines_(0), initialized_(false), current_search_index_(0) {
    
    // Initialize IntVar arrays for dual representation
    initialize_dual_variables(num_variables);
    
    // Setup default engine coordination (rhythm-pitch pairs per voice)
    setup_engine_coordination();
    
    // Initialize musical utilities integration
    musical_utils_ = std::make_unique<MusicalUtilities>();
    
    initialized_ = true;
}

MusicalSpace::MusicalSpace(const MusicalSpace& space) : Space() {
    // Copy IntVar arrays - const_cast needed for Gecode API
    absolute_vars_.update(*this, const_cast<IntVarArray&>(space.absolute_vars_));
    interval_vars_.update(*this, const_cast<IntVarArray&>(space.interval_vars_));
    
    // Copy musical engine information
    engine_info_ = space.engine_info_;
    num_voices_ = space.num_voices_;
    num_engines_ = space.num_engines_;
    
    // Copy domain and constraint information
    variable_domains_ = space.variable_domains_;
    active_rules_ = space.active_rules_;  // Shared pointers are safe to copy
    
    // Copy state
    initialized_ = space.initialized_;
    current_search_index_ = space.current_search_index_;
    
    // Create new musical utilities instance
    musical_utils_ = std::make_unique<MusicalUtilities>();
}

Space* MusicalSpace::copy() {
    return new MusicalSpace(*this);
}

void MusicalSpace::constrain(const Space& best) {
    const MusicalSpace& other = static_cast<const MusicalSpace&>(best);
    
    // Add constraint to find better solutions
    // This could be based on musical criteria (minimize dissonance, etc.)
    
    // For now, implement basic "different solution" constraint
    BoolVarArgs diff(absolute_vars_.size());
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        diff[i] = BoolVar(*this, 0, 1);
        if (other.absolute_vars_[i].assigned()) {
            rel(*this, absolute_vars_[i] != other.absolute_vars_[i].val(), diff[i]);
        }
    }
    linear(*this, diff, IRT_GQ, 1);  // At least one variable must be different
}

void MusicalSpace::initialize_dual_variables(int num_variables) {
    // Create IntVar arrays for dual representation
    absolute_vars_ = IntVarArray(*this, num_variables, 0, 127);  // Default MIDI range
    interval_vars_ = IntVarArray(*this, num_variables, -12, 12); // Default interval range
    
    // Initialize domain types
    variable_domains_.resize(num_variables, DomainType::ABSOLUTE_DOMAIN);
    
    // Link absolute and interval constraints
    link_absolute_interval_constraints();
}

void MusicalSpace::setup_engine_coordination() {
    // Create default rhythm-pitch engine pairs for each voice
    engine_info_.clear();
    
    for (int voice = 0; voice < num_voices_; ++voice) {
        int rhythm_engine = num_engines_++;
        int pitch_engine = num_engines_++;
        
        // Add rhythm engine
        engine_info_.emplace_back(rhythm_engine, EngineType::RHYTHM_ENGINE, pitch_engine, voice, 
                                 DomainType::ABSOLUTE_DOMAIN);
        
        // Add pitch engine  
        engine_info_.emplace_back(pitch_engine, EngineType::PITCH_ENGINE, rhythm_engine, voice, 
                                 DomainType::ABSOLUTE_DOMAIN);
    }
    
    // Add global metric engine if multiple voices
    if (num_voices_ > 1) {
        engine_info_.emplace_back(num_engines_++, EngineType::METRIC_ENGINE, -1, -1, 
                                 DomainType::ABSOLUTE_DOMAIN);
    }
}

void MusicalSpace::link_absolute_interval_constraints() {
    // Link absolute and interval variables with cluster-engine v4.05 semantics
    for (int i = 1; i < absolute_vars_.size(); ++i) {
        // interval[i] = absolute[i] - absolute[i-1] 
        rel(*this, interval_vars_[i], IRT_EQ, expr(*this, absolute_vars_[i] - absolute_vars_[i-1]));
    }
    
    // First interval is conventionally 0 (no previous note to compare to)
    if (interval_vars_.size() > 0) {
        rel(*this, interval_vars_[0], IRT_EQ, 0);
    }
}

void MusicalSpace::add_engine(EngineType type, int voice, DomainType domain_type) {
    engine_info_.emplace_back(num_engines_++, type, -1, voice, domain_type);
}

void MusicalSpace::set_engine_partner(int engine1_id, int engine2_id) {
    if (engine1_id < engine_info_.size()) {
        engine_info_[engine1_id].partner_engine_id = engine2_id;
    }
    if (engine2_id < engine_info_.size()) {
        engine_info_[engine2_id].partner_engine_id = engine1_id;
    }
}

void MusicalSpace::initialize_domains(const std::vector<int>& uniform_domain, DomainType type) {
    // Initialize all variables with the same domain
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        initialize_variable_domain(i, uniform_domain, type);
        variable_domains_[i] = type;
    }
}

void MusicalSpace::initialize_domains(const std::vector<std::vector<int>>& domains, 
                                     const std::vector<DomainType>& types) {
    if (domains.size() != absolute_vars_.size() || types.size() != absolute_vars_.size()) {
        throw std::invalid_argument("Domain and type vectors must match variable count");
    }
    
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        initialize_variable_domain(i, domains[i], types[i]);
        variable_domains_[i] = types[i];
    }
}

void MusicalSpace::initialize_variable_domain(int var_idx, const std::vector<int>& domain, DomainType type) {
    if (domain.empty()) return;
    
    int min_val = *std::min_element(domain.begin(), domain.end());
    int max_val = *std::max_element(domain.begin(), domain.end());
    
    // Create IntSet for domain constraint
    IntSet domain_set(domain.data(), domain.size());
    
    // Apply domain to appropriate variable based on type
    switch (type) {
        case DomainType::ABSOLUTE_DOMAIN:
        case DomainType::ABSOLUTE_RHYTHM_DOMAIN:
            dom(*this, absolute_vars_[var_idx], domain_set);
            break;
            
        case DomainType::INTERVAL_DOMAIN:
        case DomainType::DURATION_DOMAIN:
            dom(*this, interval_vars_[var_idx], domain_set);
            // Update absolute variable domain accordingly
            if (var_idx > 0) {
                dom(*this, absolute_vars_[var_idx], 
                    absolute_vars_[var_idx-1] + min_val, 
                    absolute_vars_[var_idx-1] + max_val);
            }
            break;
            
        case DomainType::BASIC_DOMAIN:
            // Basic domain applied to absolute vars (symbols mapped to integers)
            dom(*this, absolute_vars_[var_idx], domain_set);
            break;
    }
}

void MusicalSpace::randomize_domains(bool enable) {
    if (!enable) return;
    
    // This would implement cluster-engine's randomized domain ordering
    // For now, we rely on Gecode's built-in randomization in branching
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Could implement domain value reordering here if needed
    // The effect is achieved more naturally through Gecode's random branching
}

// ===============================  
// Dual Representation Access Methods
// ===============================

int MusicalSpace::get_absolute_value(int var_idx) const {
    if (var_idx >= 0 && var_idx < absolute_vars_.size() && absolute_vars_[var_idx].assigned()) {
        return absolute_vars_[var_idx].val();
    }
    return 0;  // Default value for unassigned variables
}

int MusicalSpace::get_interval_value(int var_idx) const {
    if (var_idx >= 0 && var_idx < interval_vars_.size() && interval_vars_[var_idx].assigned()) {
        return interval_vars_[var_idx].val();
    }
    return 0;
}

int MusicalSpace::get_basic_value(int var_idx) const {
    // Basic values are stored in absolute_vars_ for symbol-to-integer mapping
    return get_absolute_value(var_idx);
}

std::vector<int> MusicalSpace::get_absolute_sequence(int start_idx, int length) const {
    std::vector<int> sequence;
    sequence.reserve(length);
    
    for (int i = start_idx; i < start_idx + length && i < absolute_vars_.size(); ++i) {
        sequence.push_back(get_absolute_value(i));
    }
    
    return sequence;
}

std::vector<int> MusicalSpace::get_interval_sequence(int start_idx, int length) const {
    std::vector<int> sequence;
    sequence.reserve(length);
    
    for (int i = start_idx; i < start_idx + length && i < interval_vars_.size(); ++i) {
        sequence.push_back(get_interval_value(i));
    }
    
    return sequence;
}

// ===============================
// Musical Constraint Posting  
// ===============================

void MusicalSpace::post_musical_rule(std::shared_ptr<MusicalRule> rule) {
    active_rules_.push_back(rule);
    
    // Post the rule as a Gecode propagator
    MusicalPropagator::post(*this, absolute_vars_, interval_vars_, rule);
}

void MusicalSpace::post_coordination_constraints() {
    // Post constraints linking rhythm and pitch engines
    for (const auto& engine : engine_info_) {
        if (engine.partner_engine_id != -1 && engine.type == EngineType::RHYTHM) {
            // Rhythm-pitch coordination constraints could be added here
            // For example: when rhythm has a rest, pitch should be rest too
        }
    }
}

void MusicalSpace::post_interval_calculation_constraints() {
    // Already handled in link_absolute_interval_constraints()
    // Could add additional musical interval constraints here
    
    // Example: Limit large melodic leaps
    for (int i = 1; i < interval_vars_.size(); ++i) {
        rel(*this, abs(interval_vars_[i]) <= 12);  // Max octave leap
    }
}

// ===============================
// Engine Coordination Methods
// ===============================

EngineType MusicalSpace::get_engine_type(int engine_id) const {
    if (engine_id >= 0 && engine_id < engine_info_.size()) {
        return engine_info_[engine_id].type;
    }
    return EngineType::PITCH;  // Default
}

int MusicalSpace::get_engine_partner(int engine_id) const {
    if (engine_id >= 0 && engine_id < engine_info_.size()) {
        return engine_info_[engine_id].partner_engine_id;
    }
    return -1;
}

int MusicalSpace::get_engine_voice(int engine_id) const {
    if (engine_id >= 0 && engine_id < engine_info_.size()) {
        return engine_info_[engine_id].voice;
    }
    return -1;
}

// ===============================
// Musical Intelligence Integration
// ===============================

void MusicalSpace::set_musical_utilities(std::unique_ptr<MusicalUtilities> utils) {
    musical_utils_ = std::move(utils);
}

MusicalUtilities* MusicalSpace::get_musical_utilities() const {
    return musical_utils_.get();
}

// ===============================
// Solution Analysis and Extraction
// ===============================

bool MusicalSpace::is_complete_solution() const {
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        if (!absolute_vars_[i].assigned()) {
            return false;
        }
    }
    return true;
}

std::vector<DualCandidate> MusicalSpace::extract_solution() const {
    std::vector<DualCandidate> solution;
    solution.reserve(absolute_vars_.size());
    
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        int abs_val = absolute_vars_[i].assigned() ? absolute_vars_[i].val() : 0;
        int int_val = interval_vars_[i].assigned() ? interval_vars_[i].val() : 0;
        solution.emplace_back(abs_val, int_val);
    }
    
    return solution;
}

void MusicalSpace::print_musical_solution() const {
    std::cout << "🎵 Musical Solution:" << std::endl;
    std::cout << "Variables: " << absolute_vars_.size() << std::endl;
    std::cout << "Voices: " << num_voices_ << std::endl;
    std::cout << "Engines: " << num_engines_ << std::endl;
    
    auto solution = extract_solution();
    for (size_t i = 0; i < solution.size(); ++i) {
        std::cout << "  Var " << i << ": absolute=" << solution[i].absolute_value 
                  << " interval=" << solution[i].interval_value << std::endl;
    }
}

// ===============================
// Variable Access for Search
// ===============================

IntVar MusicalSpace::get_absolute_var(int idx) const {
    if (idx >= 0 && idx < absolute_vars_.size()) {
        return absolute_vars_[idx];
    }
    throw std::out_of_range("Variable index out of range");
}

IntVar MusicalSpace::get_interval_var(int idx) const {
    if (idx >= 0 && idx < interval_vars_.size()) {
        return interval_vars_[idx];
    }
    throw std::out_of_range("Variable index out of range");
}

IntVarArgs MusicalSpace::get_absolute_vars() const {
    IntVarArgs vars(absolute_vars_.size());
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        vars[i] = absolute_vars_[i];
    }
    return vars;
}

IntVarArgs MusicalSpace::get_interval_vars() const {
    IntVarArgs vars(interval_vars_.size());
    for (int i = 0; i < interval_vars_.size(); ++i) {
        vars[i] = interval_vars_[i];
    }
    return vars;
}

IntVarArgs MusicalSpace::get_engine_variables(int engine_id) const {
    // For simplicity, map engine to variable ranges
    // In a full implementation, this would be more sophisticated
    int vars_per_engine = absolute_vars_.size() / num_engines_;
    int start_idx = engine_id * vars_per_engine;
    int end_idx = std::min(start_idx + vars_per_engine, (int)absolute_vars_.size());
    
    IntVarArgs engine_vars(end_idx - start_idx);
    for (int i = start_idx; i < end_idx; ++i) {
        engine_vars[i - start_idx] = absolute_vars_[i];
    }
    
    return engine_vars;
}

std::vector<int> MusicalSpace::get_engine_variable_indices(int engine_id) const {
    std::vector<int> indices;
    
    int vars_per_engine = absolute_vars_.size() / num_engines_;
    int start_idx = engine_id * vars_per_engine;
    int end_idx = std::min(start_idx + vars_per_engine, (int)absolute_vars_.size());
    
    for (int i = start_idx; i < end_idx; ++i) {
        indices.push_back(i);
    }
    
    return indices;
}

// ===============================
// MusicalPropagator Implementation  
// ===============================

MusicalPropagator::MusicalPropagator(Home home, const IntVarArgs& abs_vars, 
                                     const IntVarArgs& int_vars, 
                                     std::shared_ptr<MusicalRule> rule)
    : Propagator(home), musical_rule_(rule) {
    
    absolute_views_.init(home, abs_vars.size());
    interval_views_.init(home, int_vars.size());
    
    for (int i = 0; i < abs_vars.size(); ++i) {
        absolute_views_[i].init(abs_vars[i]);
    }
    for (int i = 0; i < int_vars.size(); ++i) {
        interval_views_[i].init(int_vars[i]);
    }
    
    absolute_views_.subscribe(home, *this, Int::PC_INT_DOM);
    interval_views_.subscribe(home, *this, Int::PC_INT_DOM);
}

MusicalPropagator::MusicalPropagator(Space& home, const MusicalPropagator& p)
    : Propagator(home, p), musical_rule_(p.musical_rule_) {
    
    absolute_views_.update(home, p.absolute_views_);
    interval_views_.update(home, p.interval_views_);
}

Propagator* MusicalPropagator::copy(Space& home) {
    return new (home) MusicalPropagator(home, *this);
}

ExecStatus MusicalPropagator::propagate(Space& home, const ModEventDelta& med) {
    // Convert current variable assignments to MusicalCandidate format
    std::vector<MusicalCandidate> candidates;
    candidates.reserve(absolute_views_.size());
    
    for (int i = 0; i < absolute_views_.size(); ++i) {
        int abs_val = absolute_views_[i].assigned() ? absolute_views_[i].val() : -1;
        int int_val = interval_views_[i].assigned() ? interval_views_[i].val() : 0;
        candidates.emplace_back(abs_val, int_val);
    }
    
    // Check musical rule constraint
    if (musical_rule_ && !musical_rule_->evaluate(candidates)) {
        return ES_FAILED;  // Rule violation - constraint failed
    }
    
    // Could implement domain pruning here based on rule analysis
    // For now, just validate the rule
    
    return ES_FIX;  // Constraint satisfied
}

PropCost MusicalPropagator::cost(const Space& home, const ModEventDelta& med) const {
    return PropCost::linear(PropCost::LO, absolute_views_.size() + interval_views_.size());
}

ExecStatus MusicalPropagator::post(Home home, const IntVarArgs& abs_vars, 
                                   const IntVarArgs& int_vars,
                                   std::shared_ptr<MusicalRule> rule) {
    (void) new (home) MusicalPropagator(home, abs_vars, int_vars, rule);
    return ES_OK;
}

// ===============================
// MusicalBranching Implementation
// ===============================

MusicalBranching::MusicalBranching(Home home, const IntVarArgs& abs_vars, const IntVarArgs& int_vars)
    : Brancher(home) {
    
    absolute_views_.init(home, abs_vars.size());
    interval_views_.init(home, int_vars.size());
    
    for (int i = 0; i < abs_vars.size(); ++i) {
        absolute_views_[i].init(abs_vars[i]);
    }
    for (int i = 0; i < int_vars.size(); ++i) {
        interval_views_[i].init(int_vars[i]);
    }
}

MusicalBranching::MusicalBranching(Space& home, const MusicalBranching& b)
    : Brancher(home, b) {
    
    absolute_views_.update(home, b.absolute_views_);
    interval_views_.update(home, b.interval_views_);
    heuristic_rules_ = b.heuristic_rules_;
}

Brancher* MusicalBranching::copy(Space& home) {
    return new (home) MusicalBranching(home, *this);
}

bool MusicalBranching::status(const Space& home) const {
    for (int i = 0; i < absolute_views_.size(); ++i) {
        if (!absolute_views_[i].assigned()) {
            return true;  // More branching needed
        }
    }
    return false;  // All variables assigned
}

class MusicalChoice : public Choice {
public:
    int variable;
    int value;
    
    MusicalChoice(const Brancher& b, int var, int val) 
        : Choice(b, 2), variable(var), value(val) {}
    
    virtual void archive(Archive& e) const override {
        Choice::archive(e);
        e << variable << value;
    }
};

Choice* MusicalBranching::choice(Space& home) {
    // Find first unassigned variable - simple strategy for now
    for (int i = 0; i < absolute_views_.size(); ++i) {
        if (!absolute_views_[i].assigned()) {
            // Choose middle value - could use heuristic rules here
            int min_val = absolute_views_[i].min();
            int max_val = absolute_views_[i].max();
            int chosen_val = (min_val + max_val) / 2;
            
            return new MusicalChoice(*this, i, chosen_val);
        }
    }
    return nullptr;
}

Choice* MusicalBranching::choice(const Space& home, Archive& e) {
    int variable, value;
    e >> variable >> value;
    return new MusicalChoice(*this, variable, value);
}

ExecStatus MusicalBranching::commit(Space& home, const Choice& c, unsigned int a) {
    const MusicalChoice& mc = static_cast<const MusicalChoice&>(c);
    
    if (a == 0) {
        // First alternative: x = value
        return me_failed(absolute_views_[mc.variable].eq(home, mc.value)) ? ES_FAILED : ES_OK;
    } else {
        // Second alternative: x != value  
        return me_failed(absolute_views_[mc.variable].nq(home, mc.value)) ? ES_FAILED : ES_OK;
    }
}

void MusicalBranching::add_heuristic_rule(std::shared_ptr<MusicalRule> rule) {
    heuristic_rules_.push_back(rule);
}

void MusicalBranching::post(Home home, const IntVarArgs& abs_vars, const IntVarArgs& int_vars) {
    (void) new (home) MusicalBranching(home, abs_vars, int_vars);
}

} // namespace ClusterEngine