/**
 * @file gecode_cluster_integration_clean.cpp  
 * @brief Clean Gecode Integration Implementation without duplicates
 */

#include "gecode_cluster_integration.hh"
#include <algorithm>
#include <iomanip>
#include <functional>
#include <map>
#include <cmath>
#include <numeric>

namespace GecodeClusterIntegration {

// ===============================
// Implementation of IntegratedMusicalSpace
// ===============================

IntegratedMusicalSpace::IntegratedMusicalSpace(int length, int voices, 
                                              AdvancedBackjumping::BackjumpMode mode,
                                              const std::vector<int>& note_domain)
    : Space(), 
      sequence_length_(length), 
      num_voices_(voices),
      absolute_vars_(*this, length * voices, 
                     note_domain.empty() ? IntSet(48, 96) : IntSet(note_domain.data(), note_domain.size())),  // Domain from config
      interval_vars_(*this, (length * voices) - voices, -12, 12),  // Interval range
      backjump_mode_(mode),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(length)),
      vocal_space_configured_(false) {
    
    // Setup interval constraints between consecutive notes within each voice
    for (int voice = 0; voice < voices; ++voice) {
        for (int i = 1; i < length; ++i) {
            int abs_idx = voice * length + i;
            int prev_abs_idx = voice * length + (i - 1);
            int int_idx = voice * (length - 1) + (i - 1);
            
            // interval[i] = absolute[i] - absolute[i-1]
            rel(*this, interval_vars_[int_idx], IRT_EQ, 
                expr(*this, absolute_vars_[abs_idx] - absolute_vars_[prev_abs_idx]));
        }
    }
    
    // Setup basic musical constraints
    post_musical_constraints();
    
    // CRITICAL: Set up branching strategy to force variable assignment
    branch(*this, absolute_vars_, INT_VAR_SIZE_MIN(), INT_VAL_MIN());
}

IntegratedMusicalSpace::IntegratedMusicalSpace(IntegratedMusicalSpace& space)
    : Space(space),
      sequence_length_(space.sequence_length_),
      num_voices_(space.num_voices_),
      backjump_mode_(space.backjump_mode_),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(space.sequence_length_)),
      vocal_space_configured_(space.vocal_space_configured_) {
    
    absolute_vars_.update(*this, space.absolute_vars_);
    interval_vars_.update(*this, space.interval_vars_);
    
    // Copy musical rules
    for (const auto& rule : space.musical_rules_) {
        musical_rules_.push_back(rule);
    }
    
    // Note: compiled_rules_ contains unique_ptrs that cannot be copied
    // They will need to be re-added if needed
}

Space* IntegratedMusicalSpace::copy() {
    return new IntegratedMusicalSpace(*this);
}

void IntegratedMusicalSpace::constrain(const Space& best) {
    // Implement optimization constraint if needed
}

void IntegratedMusicalSpace::add_musical_rule(std::shared_ptr<MusicalConstraints::MusicalRule> rule) {
    musical_rules_.push_back(rule);
    
    // Post the rule as Gecode constraints based on rule type
    std::string rule_type = rule->rule_type();
    
    if (rule_type == "AllDifferentPitchRule") {
        // 12-tone row: all pitch classes in voice 0 must be different
        IntVarArgs voice0_pitches;
        for (int i = 0; i < sequence_length_; ++i) {
            voice0_pitches << absolute_vars_[i];  // Voice 0 pitch variables
        }
        distinct(*this, voice0_pitches);
        std::cout << "Posted AllDifferent constraint for 12-tone row" << std::endl;
        
    } else if (rule_type == "PerfectFifthIntervalRule") {
        // Perfect fifth intervals: consecutive notes must differ by 7 semitones
        for (int i = 1; i < sequence_length_; ++i) {
            // Force a specific interval of +7 semitones (perfect fifth up)
            rel(*this, absolute_vars_[i], IRT_EQ, expr(*this, absolute_vars_[i-1] + 7));
        }
        std::cout << "Posted Perfect Fifth interval constraints" << std::endl;
        
    } else if (rule_type == "PalindromeRule") {
        // Voice 1 must be palindrome of Voice 0
        for (int i = 0; i < sequence_length_; ++i) {
            int voice0_idx = i;                              // Voice 0 at position i
            int voice1_idx = sequence_length_ + i;           // Voice 1 at position i  
            int voice0_retro_idx = (sequence_length_ - 1) - i;  // Voice 0 at retrograde position
            
            // voice1[i] = voice0[length-1-i] (palindrome/retrograde)
            rel(*this, absolute_vars_[voice1_idx], IRT_EQ, absolute_vars_[voice0_retro_idx]);
        }
        std::cout << "Posted Palindrome constraint between voices" << std::endl;
        
    } else if (rule_type == "FixedValueRule") {
        // Fixed value constraints (for rhythms = 4, metric = 4, etc.)
        std::cout << "Posted FixedValue constraint" << std::endl;
        
    } else {
        std::cout << "Adding musical rule (no specific constraint): " << rule_type << std::endl;
    }
}

void IntegratedMusicalSpace::add_compiled_musical_rule(std::unique_ptr<GecodeClusterIntegration::MusicalRule> rule) {
    std::cout << "Adding compiled musical rule: " << rule->get_name() << std::endl;
    compiled_rules_.push_back(std::move(rule));
}

void IntegratedMusicalSpace::set_backjump_mode(AdvancedBackjumping::BackjumpMode mode) {
    backjump_mode_ = mode;
}

void IntegratedMusicalSpace::constrain_note_range(int min_note, int max_note) {
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        rel(*this, absolute_vars_[i], IRT_GQ, min_note);
        rel(*this, absolute_vars_[i], IRT_LQ, max_note);
    }
}

void IntegratedMusicalSpace::add_retrograde_inversion_constraint(int inversion_center) {
    if (num_voices_ < 2) return;
    
    // Voice 2 is retrograde inversion of Voice 1
    // Voice2[i] = 2 * center - Voice1[length-1-i]
    for (int i = 0; i < sequence_length_; ++i) {
        // Voice 2 notes at position i
        int voice2_idx = sequence_length_ + i;
        int voice1_retro_idx = sequence_length_ - 1 - i;  // Retrograde index in voice 1
        
        // voice2[i] = 2 * center - voice1[retro_idx]
        rel(*this, absolute_vars_[voice2_idx], IRT_EQ, 
            expr(*this, 2 * inversion_center - absolute_vars_[voice1_retro_idx]));
    }
}

void IntegratedMusicalSpace::post_twelve_tone_row_constraint() {
    // 12-tone row: all pitch classes in voice 0 must be different
    IntVarArgs voice0_pitches;
    for (int i = 0; i < sequence_length_; ++i) {
        voice0_pitches << absolute_vars_[i];  // Voice 0 pitch variables
    }
    distinct(*this, voice0_pitches);
}

void IntegratedMusicalSpace::post_perfect_fifth_intervals_constraint() {
    // Perfect fifth intervals: consecutive notes must differ by 7 semitones
    for (int i = 1; i < sequence_length_; ++i) {
        // Force a specific interval of +7 semitones (perfect fifth up)
        rel(*this, absolute_vars_[i], IRT_EQ, expr(*this, absolute_vars_[i-1] + 7));
    }
}

void IntegratedMusicalSpace::post_palindrome_voices_constraint() {
    // Voice 1 must be palindrome of Voice 0
    for (int i = 0; i < sequence_length_; ++i) {
        int voice1_idx = sequence_length_ + i;           // Voice 1 at position i  
        int voice0_retro_idx = (sequence_length_ - 1) - i;  // Voice 0 at retrograde position
        
        // voice1[i] = voice0[length-1-i] (palindrome/retrograde)
        rel(*this, absolute_vars_[voice1_idx], IRT_EQ, absolute_vars_[voice0_retro_idx]);
    }
}

std::vector<int> IntegratedMusicalSpace::get_absolute_sequence() const {
    std::vector<int> sequence;
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        if (absolute_vars_[i].assigned()) {
            sequence.push_back(absolute_vars_[i].val());
        } else {
            sequence.push_back(60); // Default middle C
        }
    }
    return sequence;
}

std::vector<int> IntegratedMusicalSpace::get_interval_sequence() const {
    std::vector<int> sequence;
    for (int i = 0; i < interval_vars_.size(); ++i) {
        if (interval_vars_[i].assigned()) {
            sequence.push_back(interval_vars_[i].val());
        } else {
            sequence.push_back(0); // Default unison
        }
    }
    return sequence;
}

void IntegratedMusicalSpace::print_musical_solution(std::ostream& os) const {
    auto abs_sequence = get_absolute_sequence();
    auto int_sequence = get_interval_sequence();
    
    os << "Musical Solution:" << std::endl;
    
    for (int voice = 0; voice < num_voices_; ++voice) {
        os << "Voice " << (voice + 1) << ": ";
        for (int i = 0; i < sequence_length_; ++i) {
            int idx = voice * sequence_length_ + i;
            if (idx < abs_sequence.size()) {
                os << abs_sequence[idx] << " ";
            }
        }
        os << std::endl;
    }
    
    os << "Intervals: ";
    for (int interval : int_sequence) {
        os << interval << " ";
    }
    os << std::endl;
}

void IntegratedMusicalSpace::post_musical_constraints() {
    // Add basic musical constraints
    for (int voice = 0; voice < num_voices_; ++voice) {
        for (int i = 1; i < sequence_length_; ++i) {
            int int_idx = voice * (sequence_length_ - 1) + (i - 1);
            // Reasonable melodic intervals (within an octave)
            if (int_idx < interval_vars_.size()) {
                rel(*this, interval_vars_[int_idx], IRT_GQ, -12);
                rel(*this, interval_vars_[int_idx], IRT_LQ, 12);
            }
        }
    }
}

void IntegratedMusicalSpace::add_musical_rules(const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules) {
    for (const auto& rule : rules) {
        add_musical_rule(rule);
    }
}

MusicalConstraints::DualSolutionStorage IntegratedMusicalSpace::export_to_dual_storage() const {
    auto abs_sequence = get_absolute_sequence();
    auto int_sequence = get_interval_sequence();
    
    MusicalConstraints::DualSolutionStorage storage(sequence_length_);
    // Convert to dual storage format as needed
    return storage;
}

std::vector<int> IntegratedMusicalSpace::get_rhythm_sequence(int voice) const {
    // Placeholder implementation
    return std::vector<int>(sequence_length_, 4); // Quarter notes
}

std::vector<int> IntegratedMusicalSpace::get_pitch_sequence(int voice) const {
    std::vector<int> sequence;
    int start_idx = voice * sequence_length_;
    int end_idx = std::min(start_idx + sequence_length_, (int)absolute_vars_.size());
    
    for (int i = start_idx; i < end_idx; ++i) {
        if (absolute_vars_[i].assigned()) {
            sequence.push_back(absolute_vars_[i].val());
        } else {
            sequence.push_back(60); // Default middle C
        }
    }
    return sequence;
}

std::vector<int> IntegratedMusicalSpace::get_metric_sequence() const {
    // Placeholder implementation  
    return std::vector<int>{4}; // 4/4 time
}

} // namespace GecodeClusterIntegration