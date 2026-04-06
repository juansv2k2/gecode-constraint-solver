/**
 * @file musical_space_demo_impl.cpp
 * @brief Minimal MusicalSpace implementation for dynamic rule demo
 */

#include "musical_space.hh"
#include <gecode/minimodel.hh>
#include <iostream>

using namespace Gecode;

namespace ClusterEngine {

// ===============================
// Minimal MusicalSpace Implementation for Demo
// ===============================

MusicalSpace::MusicalSpace(int num_variables, int num_voices) 
    : num_voices_(num_voices), num_engines_(0), initialized_(false), current_search_index_(0) {
    
    // Create IntVar arrays for dual representation
    absolute_vars_ = IntVarArray(*this, num_variables, 0, 127);
    interval_vars_ = IntVarArray(*this, num_variables, -12, 12);
    
    // Initialize domains
    variable_domains_.resize(num_variables, DomainType::ABSOLUTE_DOMAIN);
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
}

Space* MusicalSpace::copy() {
    return new MusicalSpace(*this);
}

void MusicalSpace::constrain(const Space& best) {
    // Basic constraint implementation
}

void MusicalSpace::initialize_domains(const std::vector<int>& uniform_domain, DomainType type) {
    IntSet domain_set(uniform_domain.data(), static_cast<int>(uniform_domain.size()));
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        dom(*this, absolute_vars_[i], domain_set);
        variable_domains_[i] = type;
    }
}

void MusicalSpace::post_scale_constraints(const std::vector<int>& scale_degrees, int root_note) {
    // Create scale constraint: restrict variables to scale pitches
    std::vector<int> scale_pitches;
    for (int octave = 4; octave <= 6; ++octave) {
        for (int degree : scale_degrees) {
            int pitch = octave * 12 + root_note % 12 + degree;
            if (pitch >= 36 && pitch <= 96) {
                scale_pitches.push_back(pitch);
            }
        }
    }
    
    if (!scale_pitches.empty()) {
        IntSet scale_set(scale_pitches.data(), static_cast<int>(scale_pitches.size()));
        for (int i = 0; i < absolute_vars_.size(); ++i) {
            dom(*this, absolute_vars_[i], scale_set);
        }
    }
}

void MusicalSpace::post_range_constraints(int min_pitch, int max_pitch) {
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        rel(*this, absolute_vars_[i], IRT_GQ, min_pitch);
        rel(*this, absolute_vars_[i], IRT_LQ, max_pitch);
    }
}

void MusicalSpace::post_consonance_constraints(int consonance_threshold) {
    // Basic consonance constraint: prefer unisons and octaves
    for (int i = 0; i < absolute_vars_.size() - 1; ++i) {
        // Prefer consonant intervals (simplified)
        rel(*this, expr(*this, abs(absolute_vars_[i+1] - absolute_vars_[i]) % 12), 
            IRT_LQ, 5 + consonance_threshold);
    }
}

void MusicalSpace::post_voice_leading_constraints(int voice1_start, int voice2_start, int length) {
    // Basic voice leading: limit voice crossings
    for (int i = 0; i < length - 1; ++i) {
        if (voice1_start + i < absolute_vars_.size() && voice2_start + i < absolute_vars_.size()) {
            rel(*this, absolute_vars_[voice1_start + i], IRT_LQ, 
                absolute_vars_[voice2_start + i] + 12);
        }
    }
}

void MusicalSpace::post_melodic_contour_constraints(int voice_start, int length) {
    // Limit melodic leaps
    for (int i = voice_start; i < voice_start + length - 1 && i + 1 < absolute_vars_.size(); ++i) {
        rel(*this, expr(*this, abs(absolute_vars_[i+1] - absolute_vars_[i])), IRT_LQ, 7);
    }
}

void MusicalSpace::post_cadential_constraints() {
    // Basic cadential constraint: end on tonic
    if (absolute_vars_.size() > 0) {
        rel(*this, absolute_vars_[absolute_vars_.size()-1] % 12, IRT_EQ, 0); // End on C
    }
}

void MusicalSpace::print_musical_solution() const {
    std::cout << "   Musical Solution: ";
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        if (absolute_vars_[i].assigned()) {
            std::cout << absolute_vars_[i].val();
            if (i < absolute_vars_.size() - 1) std::cout << " -> ";
        } else {
            std::cout << "?";
            if (i < absolute_vars_.size() - 1) std::cout << " -> ";
        }
    }
    std::cout << std::endl;
}

// Stub implementations for other methods
void MusicalSpace::post_musical_rule(std::shared_ptr<MusicalRule> rule) {}
void MusicalSpace::post_coordination_constraints() {}

} // namespace ClusterEngine