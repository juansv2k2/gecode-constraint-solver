/**
 * @file musical_space_simple.cpp
 * @brief Simplified MusicalSpace implementation for Gecode deployment
 * 
 * This version focuses on the core dual representation and basic constraint posting
 * without the advanced propagator features, to demonstrate real Gecode integration.
 */

#include "musical_space.hh"
#include <gecode/minimodel.hh>
#include <iostream>
#include <algorithm>
#include <random>

using namespace Gecode;

namespace ClusterEngine {

// ===============================
// Simplified MusicalSpace Implementation for Real Gecode
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

MusicalSpace::MusicalSpace(const MusicalSpace& space) : Space(space) {
    // Copy IntVar arrays
    absolute_vars_.update(*this, const_cast<IntVarArray&>(space.absolute_vars_));
    interval_vars_.update(*this, const_cast<IntVarArray&>(space.interval_vars_));
    
    // Copy musical engine information
    engine_info_ = space.engine_info_;
    num_voices_ = space.num_voices_;
    num_engines_ = space.num_engines_;
    
    // Copy domain and constraint information
    variable_domains_ = space.variable_domains_;
    active_rules_ = space.active_rules_;
    
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
    
    // Simple constraint: at least one variable must be different
    BoolVarArgs diff(absolute_vars_.size());
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        diff[i] = BoolVar(*this, 0, 1);
        if (other.absolute_vars_[i].assigned()) {
            // Use correct Gecode API for reified constraint  
            rel(*this, absolute_vars_[i], IRT_NQ, other.absolute_vars_[i].val(), diff[i]);
        }
    }
    linear(*this, diff, IRT_GQ, 1);
}

void MusicalSpace::initialize_dual_variables(int num_variables) {
    // Create IntVar arrays for dual representation
    absolute_vars_ = IntVarArray(*this, num_variables, 0, 127);
    interval_vars_ = IntVarArray(*this, num_variables, -12, 12);
    
    // Initialize domain types
    variable_domains_.resize(num_variables, DomainType::ABSOLUTE_DOMAIN);
    
    // Link absolute and interval constraints
    link_absolute_interval_constraints();
}

void MusicalSpace::setup_engine_coordination() {
    // Create rhythm-pitch engine pairs for each voice
    for (int voice = 0; voice < num_voices_; ++voice) {
        int rhythm_engine = num_engines_++;
        int pitch_engine = num_engines_++;
        
        engine_info_.emplace_back(rhythm_engine, EngineType::RHYTHM_ENGINE, pitch_engine, voice, 
                                 DomainType::ABSOLUTE_DOMAIN);
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
    // Link absolute and interval variables with linear expressions
    for (int i = 1; i < absolute_vars_.size(); ++i) {
        rel(*this, interval_vars_[i], IRT_EQ, expr(*this, absolute_vars_[i] - absolute_vars_[i-1]));
    }
    
    // First interval is conventionally 0
    if (interval_vars_.size() > 0) {
        rel(*this, interval_vars_[0], IRT_EQ, 0);
    }
}

// ===============================
// Domain Initialization Methods
// ===============================

void MusicalSpace::initialize_domains(const std::vector<int>& uniform_domain, DomainType type) {
    IntSet domain_set(uniform_domain.data(), static_cast<int>(uniform_domain.size()));
    
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        Gecode::dom(*this, absolute_vars_[i], domain_set);
        variable_domains_[i] = type;
    }
}

void MusicalSpace::initialize_domains(const std::vector<std::vector<int>>& domains, 
                                    const std::vector<DomainType>& types) {
    for (size_t i = 0; i < domains.size() && i < static_cast<size_t>(absolute_vars_.size()); ++i) {
        if (!domains[i].empty()) {
            IntSet domain_set(domains[i].data(), static_cast<int>(domains[i].size()));
            Gecode::dom(*this, absolute_vars_[i], domain_set);
            if (i < types.size()) {
                variable_domains_[i] = types[i];
            }
        }
    }
}

// ===============================
// Dual Representation Access Methods
// ===============================

int MusicalSpace::get_absolute_value(int var_idx) const {
    if (var_idx >= 0 && var_idx < absolute_vars_.size() && absolute_vars_[var_idx].assigned()) {
        return absolute_vars_[var_idx].val();
    }
    return 0; // Default if not assigned
}

int MusicalSpace::get_interval_value(int var_idx) const {
    if (var_idx >= 0 && var_idx < interval_vars_.size() && interval_vars_[var_idx].assigned()) {
        return interval_vars_[var_idx].val();
    }
    return 0; // Default if not assigned
}

int MusicalSpace::get_basic_value(int var_idx) const {
    return get_absolute_value(var_idx); // In basic mode, return absolute value
}

std::vector<int> MusicalSpace::get_absolute_sequence(int start_idx, int length) const {
    std::vector<int> sequence;
    for (int i = start_idx; i < start_idx + length && i < absolute_vars_.size(); ++i) {
        sequence.push_back(get_absolute_value(i));
    }
    return sequence;
}

std::vector<int> MusicalSpace::get_interval_sequence(int start_idx, int length) const {
    std::vector<int> sequence;
    for (int i = start_idx; i < start_idx + length && i < interval_vars_.size(); ++i) {
        sequence.push_back(get_interval_value(i));
    }
    return sequence;
}

// ===============================
// Musical Constraint Posting (Simplified)
// ===============================

void MusicalSpace::post_musical_rule(std::shared_ptr<MusicalRule> rule) {
    active_rules_.push_back(rule);
    // For this simplified version, we don't create custom propagators
    // The rules will be checked in the solution validation
}

void MusicalSpace::post_coordination_constraints() {
    // Simple coordination: ensure rhythm and pitch engines work together
    // This is a placeholder - full coordination would be more complex
}

void MusicalSpace::post_interval_calculation_constraints() {
    // Already handled in link_absolute_interval_constraints()
}

// ===============================
// Engine Information Access
// ===============================

EngineType MusicalSpace::get_engine_type(int engine_id) const {
    if (engine_id >= 0 && engine_id < static_cast<int>(engine_info_.size())) {
        return engine_info_[engine_id].type;
    }
    return EngineType::PITCH_ENGINE; // Default
}

int MusicalSpace::get_engine_partner(int engine_id) const {
    if (engine_id >= 0 && engine_id < static_cast<int>(engine_info_.size())) {
        return engine_info_[engine_id].partner_engine_id;
    }
    return -1;
}

int MusicalSpace::get_engine_voice(int engine_id) const {
    if (engine_id >= 0 && engine_id < static_cast<int>(engine_info_.size())) {
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
// Solution Analysis
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
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        DualCandidate candidate(get_absolute_value(i), get_interval_value(i));
        solution.push_back(candidate);
    }
    return solution;
}

void MusicalSpace::print_musical_solution() const {
    std::cout << "🎵 Musical Solution:" << std::endl;
    std::cout << "Variables: " << absolute_vars_.size() << std::endl;
    std::cout << "Voices: " << num_voices_ << std::endl;
    std::cout << "Engines: " << num_engines_ << std::endl;
    
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        std::cout << "  Var " << i << ": absolute=" << get_absolute_value(i) 
                  << " interval=" << get_interval_value(i) << std::endl;
    }
}

// ===============================
// Variable Access for Branching
// ===============================

IntVar MusicalSpace::get_absolute_var(int idx) const {
    if (idx >= 0 && idx < absolute_vars_.size()) {
        return absolute_vars_[idx];
    }
    return IntVar(); // Invalid variable
}

IntVar MusicalSpace::get_interval_var(int idx) const {
    if (idx >= 0 && idx < interval_vars_.size()) {
        return interval_vars_[idx];
    }
    return IntVar(); // Invalid variable
}

IntVarArgs MusicalSpace::get_absolute_vars() const {
    IntVarArgs args(absolute_vars_.size());
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        args[i] = absolute_vars_[i];
    }
    return args;
}

IntVarArgs MusicalSpace::get_interval_vars() const {
    IntVarArgs args(interval_vars_.size());
    for (int i = 0; i < interval_vars_.size(); ++i) {
        args[i] = interval_vars_[i];
    }
    return args;
}

} // namespace ClusterEngine