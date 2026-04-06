/**
 * @file musical_space_basic.cpp
 * @brief Basic MusicalSpace proof-of-concept for real Gecode deployment
 * 
 * This minimal version demonstrates the core dual representation concept
 * working with real Gecode constraint programming.
 */

#include "musical_space.hh"
#include <gecode/minimodel.hh>
#include <iostream>

using namespace Gecode;

namespace ClusterEngine {

// ===============================
// Basic MusicalSpace for Real Gecode Proof-of-Concept
// ===============================

MusicalSpace::MusicalSpace(int num_variables, int num_voices) 
    : num_voices_(num_voices), num_engines_(0), initialized_(false), current_search_index_(0) {
    
    // Create IntVar arrays for dual representation
    absolute_vars_ = IntVarArray(*this, num_variables, 0, 127);
    interval_vars_ = IntVarArray(*this, num_variables, -12, 12);
    
    // Simple constraint: link absolute and interval variables
    for (int i = 1; i < num_variables; ++i) {
        rel(*this, interval_vars_[i], IRT_EQ, expr(*this, absolute_vars_[i] - absolute_vars_[i-1]));
    }
    rel(*this, interval_vars_[0], IRT_EQ, 0);
    
    // Initialize basic engine info
    num_engines_ = num_voices * 2; // rhythm + pitch per voice
    variable_domains_.resize(num_variables, DomainType::ABSOLUTE_DOMAIN);
    // Note: MusicalUtilities are static functions, no instance needed
    
    initialized_ = true;
}

MusicalSpace::MusicalSpace(const MusicalSpace& space) : Space(const_cast<MusicalSpace&>(space)) {
    absolute_vars_.update(*this, const_cast<IntVarArray&>(space.absolute_vars_));
    interval_vars_.update(*this, const_cast<IntVarArray&>(space.interval_vars_));
    num_voices_ = space.num_voices_;
    num_engines_ = space.num_engines_;
    variable_domains_ = space.variable_domains_;
    initialized_ = space.initialized_;
    current_search_index_ = space.current_search_index_;
    // Note: MusicalUtilities are static functions, no instance needed
}

Space* MusicalSpace::copy() {
    return new MusicalSpace(*this);
}

void MusicalSpace::constrain(const Space& best) {
    // Simple: no additional constraints for basic version
    (void)best;
}

// ===============================
// Domain Initialization Methods
// ===============================

void MusicalSpace::initialize_domains(const std::vector<int>& uniform_domain, DomainType type) {
    IntSet domain_set(uniform_domain.data(), static_cast<int>(uniform_domain.size()));
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        dom(*this, absolute_vars_[i], domain_set);
        variable_domains_[i] = type;
    }
}

void MusicalSpace::initialize_domains(const std::vector<std::vector<int>>& domains, 
                                    const std::vector<DomainType>& types) {
    for (size_t i = 0; i < domains.size() && i < static_cast<size_t>(absolute_vars_.size()); ++i) {
        if (!domains[i].empty()) {
            IntSet domain_set(domains[i].data(), static_cast<int>(domains[i].size()));
            dom(*this, absolute_vars_[i], domain_set);
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
    return 0;
}

int MusicalSpace::get_interval_value(int var_idx) const {
    if (var_idx >= 0 && var_idx < interval_vars_.size() && interval_vars_[var_idx].assigned()) {
        return interval_vars_[var_idx].val();
    }
    return 0;
}

int MusicalSpace::get_basic_value(int var_idx) const {
    return get_absolute_value(var_idx);
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
// Simplified Musical Constraint Posting
// ===============================

void MusicalSpace::post_musical_rule(std::shared_ptr<MusicalRule> rule) {
    active_rules_.push_back(rule);
}

void MusicalSpace::post_coordination_constraints() {
    // Basic coordination already handled in constructor
}

void MusicalSpace::post_interval_calculation_constraints() {
    // Already handled in constructor
}

// ===============================
// Engine Information Access (Simplified)
// ===============================

EngineType MusicalSpace::get_engine_type(int engine_id) const {
    return (engine_id % 2 == 0) ? EngineType::RHYTHM_ENGINE : EngineType::PITCH_ENGINE;
}

int MusicalSpace::get_engine_partner(int engine_id) const {
    return (engine_id % 2 == 0) ? engine_id + 1 : engine_id - 1;
}

int MusicalSpace::get_engine_voice(int engine_id) const {
    return engine_id / 2;
}

//
// Musical Intelligence Integration
// ===============================

void MusicalSpace::set_musical_utilities(std::unique_ptr<MusicalUtilities> utils) {
    // Static functions, no instance needed
    (void)utils;
}

MusicalUtilities* MusicalSpace::get_musical_utilities() const {
    // Static functions, no instance needed
    return nullptr;
}

// ===============================
// Solution Analysis
// ===============================

bool MusicalSpace::is_complete_solution() const {
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        if (!absolute_vars_[i].assigned()) return false;
    }
    return true;
}

std::vector<DualCandidate> MusicalSpace::extract_solution() const {
    std::vector<DualCandidate> solution;
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        solution.emplace_back(get_absolute_value(i), get_interval_value(i));
    }
    return solution;
}

void MusicalSpace::print_musical_solution() const {
    std::cout << "🎵 Musical Solution:" << std::endl;
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        std::cout << "  Var " << i << ": absolute=" << get_absolute_value(i) 
                  << " interval=" << get_interval_value(i) << std::endl;
    }
}

// ===============================
// Variable Access for Branching
// ===============================

IntVar MusicalSpace::get_absolute_var(int idx) const {
    return (idx >= 0 && idx < absolute_vars_.size()) ? absolute_vars_[idx] : IntVar();
}

IntVar MusicalSpace::get_interval_var(int idx) const {
    return (idx >= 0 && idx < interval_vars_.size()) ? interval_vars_[idx] : IntVar();
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