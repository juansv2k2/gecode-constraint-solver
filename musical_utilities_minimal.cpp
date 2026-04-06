/**
 * @file musical_utilities_minimal.cpp 
 * @brief Minimal musical utilities stub for basic Gecode integration
 */

#include "musical_utilities.hh"
#include <cmath>

namespace ClusterEngine {

// ===============================
// Minimal MusicalUtilities Implementation
// ===============================

MusicalUtilities::MusicalUtilities() {
    // Basic initialization
}

MusicalUtilities::~MusicalUtilities() = default;

// ===============================
// Core Musical Analysis (Simplified)
// ===============================

double MusicalUtilities::calculate_pitch_class(int midi_note) const {
    return static_cast<double>(midi_note % 12);
}

double MusicalUtilities::calculate_interval_analysis(int note1, int note2) const {
    return std::abs(note1 - note2);
}

bool MusicalUtilities::analyze_harmonic_context(const std::vector<int>& pitches, 
                                               int index, int& harmonic_weight) const {
    if (pitches.empty() || index >= static_cast<int>(pitches.size())) {
        harmonic_weight = 0;
        return false;
    }
    
    harmonic_weight = 1; // Basic weight
    return true;
}

bool MusicalUtilities::calculate_rhythmic_weight(const std::vector<int>& rhythm_values,
                                                int position, double& weight) const {
    if (rhythm_values.empty() || position >= static_cast<int>(rhythm_values.size())) {
        weight = 1.0;
        return false;
    }
    
    weight = (position % 2 == 0) ? 1.5 : 1.0; // Strong/weak beats
    return true;
}

bool MusicalUtilities::validate_melodic_motion(const std::vector<int>& pitches, 
                                              int start_idx, int end_idx) const {
    if (pitches.empty() || start_idx >= end_idx || 
        end_idx >= static_cast<int>(pitches.size())) {
        return false;
    }
    
    // Simple validation: no jumps > octave
    for (int i = start_idx; i < end_idx; ++i) {
        if (std::abs(pitches[i+1] - pitches[i]) > 12) {
            return false;
        }
    }
    return true;
}

double MusicalUtilities::calculate_dissonance_factor(const std::vector<int>& chord) const {
    if (chord.size() < 2) return 0.0;
    
    double dissonance = 0.0;
    for (size_t i = 0; i < chord.size(); ++i) {
        for (size_t j = i + 1; j < chord.size(); ++j) {
            int interval = std::abs(chord[i] - chord[j]) % 12;
            
            // Basic dissonance mapping 
            if (interval == 1 || interval == 6 || interval == 10 || interval == 11) {
                dissonance += 1.0; // Dissonant intervals
            }
        }
    }
    
    return dissonance;
}

bool MusicalUtilities::check_voice_leading_rules(const std::vector<std::vector<int>>& voices,
                                                int position) const {
    if (voices.empty() || position <= 0) return true;
    
    // Basic voice leading: no parallel fifths/octaves
    for (size_t v1 = 0; v1 < voices.size(); ++v1) {
        for (size_t v2 = v1 + 1; v2 < voices.size(); ++v2) {
            if (position >= static_cast<int>(voices[v1].size()) || 
                position >= static_cast<int>(voices[v2].size())) continue;
                
            int prev_interval = std::abs(voices[v1][position-1] - voices[v2][position-1]) % 12;
            int curr_interval = std::abs(voices[v1][position] - voices[v2][position]) % 12;
            
            // Parallel fifths (7) or octaves (0)
            if ((prev_interval == 7 && curr_interval == 7) ||
                (prev_interval == 0 && curr_interval == 0)) {
                return false;
            }
        }
    }
    
    return true;
}

std::vector<int> MusicalUtilities::generate_scale_degrees(int root, const std::string& mode) const {
    std::vector<int> degrees;
    
    // Major scale pattern (other modes simplified to this for now)
    std::vector<int> major_pattern = {0, 2, 4, 5, 7, 9, 11};
    
    for (int degree : major_pattern) {
        degrees.push_back(root + degree);
    }
    
    return degrees;
}

bool MusicalUtilities::fits_modal_context(int pitch, int root, const std::string& mode) const {
    auto scale_degrees = generate_scale_degrees(root, mode);
    int pitch_class = pitch % 12;
    int root_class = root % 12;
    
    for (int degree : scale_degrees) {
        if ((degree % 12) == (pitch_class - root_class + 12) % 12) {
            return true;
        }
    }
    
    return false;
}

// ===============================
// Rhythm Analysis (Simplified)
// ===============================

double MusicalUtilities::calculate_rhythmic_tension(const std::vector<int>& durations, 
                                                   int position) const {
    if (durations.empty() || position >= static_cast<int>(durations.size())) {
        return 0.0;
    }
    
    // Tension based on syncopation (off-beat emphasis)
    return (position % 2 == 1) ? 0.7 : 0.3;
}

bool MusicalUtilities::analyze_rhythmic_pattern(const std::vector<int>& pattern,
                                               std::string& pattern_name) const {
    if (pattern.empty()) {
        pattern_name = "empty";
        return false;
    }
    
    if (pattern.size() == 4) {
        pattern_name = "4/4_basic";
    } else if (pattern.size() == 3) {
        pattern_name = "3/4_waltz";
    } else {
        pattern_name = "irregular";
    }
    
    return true;
}

double MusicalUtilities::calculate_swing_ratio(const std::vector<int>& eighth_notes) const {
    // Simplified: assume straight eighths
    (void)eighth_notes;
    return 1.0;  // 1:1 ratio (straight)
}

} // namespace ClusterEngine