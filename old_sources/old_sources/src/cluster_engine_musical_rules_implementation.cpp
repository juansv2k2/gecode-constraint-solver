/**
 * @file cluster_engine_musical_rules_implementation.cpp
 * @brief Implementation of Musical Rule Factories and Common Constraint Functions
 */

#include "cluster_engine_musical_rules.hh"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace ClusterEngine {
namespace MusicalRules {

// =============================================================================
// Musical Pattern Constants
// =============================================================================

const std::vector<MusicalValue> CommonMusicalPatterns::QUARTER_NOTE_PATTERN = {0.25, 0.25, 0.25, 0.25};
const std::vector<MusicalValue> CommonMusicalPatterns::EIGHTH_NOTE_PATTERN = {0.125, 0.125, 0.125, 0.125};
const std::vector<MusicalValue> CommonMusicalPatterns::DOTTED_RHYTHM_PATTERN = {0.375, 0.125, 0.25, 0.25};
const std::vector<MusicalValue> CommonMusicalPatterns::SYNCOPATED_PATTERN = {0.125, 0.25, 0.125, 0.25, 0.25};

const std::vector<int> CommonMusicalPatterns::MAJOR_SCALE = {0, 2, 4, 5, 7, 9, 11};
const std::vector<int> CommonMusicalPatterns::MINOR_SCALE = {0, 2, 3, 5, 7, 8, 10};
const std::vector<int> CommonMusicalPatterns::PENTATONIC_SCALE = {0, 2, 4, 7, 9};
const std::vector<int> CommonMusicalPatterns::CHROMATIC_SCALE = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

const std::vector<std::vector<int>> CommonMusicalPatterns::MAJOR_TRIADS = {{0, 4, 7}, {2, 5, 9}, {4, 7, 11}};
const std::vector<std::vector<int>> CommonMusicalPatterns::MINOR_TRIADS = {{0, 3, 7}, {2, 5, 8}, {4, 7, 10}};
const std::vector<std::vector<int>> CommonMusicalPatterns::SEVENTH_CHORDS = {{0, 4, 7, 11}, {0, 3, 7, 10}, {2, 5, 9, 0}};

const std::vector<std::vector<int>> CommonMusicalPatterns::II_V_I_PROGRESSION = {{2, 5, 9}, {7, 11, 2}, {0, 4, 7}};
const std::vector<std::vector<int>> CommonMusicalPatterns::CIRCLE_OF_FIFTHS = {{0}, {7}, {2}, {9}, {4}, {11}, {6}};

const std::vector<int> CommonMusicalPatterns::ASCENDING_CONTOUR = {1, 1, 1, 1};
const std::vector<int> CommonMusicalPatterns::DESCENDING_CONTOUR = {-1, -1, -1, -1};
const std::vector<int> CommonMusicalPatterns::ARCH_CONTOUR = {1, 1, -1, -1};
const std::vector<int> CommonMusicalPatterns::V_CONTOUR = {-1, -1, 1, 1};

// =============================================================================
// Helper Functions
// =============================================================================

bool MusicalRuleFactory::is_consonant_interval(double interval) {
    double mod_interval = fmod(std::abs(interval), 12.0);
    // Unison, minor 3rd, major 3rd, perfect 4th, perfect 5th, minor 6th, major 6th, octave
    return (mod_interval == 0 || mod_interval == 3 || mod_interval == 4 ||
            mod_interval == 5 || mod_interval == 7 || mod_interval == 8 ||
            mod_interval == 9 || mod_interval == 12);
}

bool MusicalRuleFactory::is_perfect_interval(double interval) {
    double mod_interval = fmod(std::abs(interval), 12.0);
    // Unison, perfect 4th, perfect 5th, octave
    return (mod_interval == 0 || mod_interval == 5 || mod_interval == 7 || mod_interval == 12);
}

double MusicalRuleFactory::calculate_interval(MusicalValue pitch1, MusicalValue pitch2) {
    return std::abs(pitch2 - pitch1);
}

int MusicalRuleFactory::get_scale_degree(MusicalValue pitch, const std::vector<int>& scale) {
    int pitch_class = static_cast<int>(pitch) % 12;
    auto it = std::find(scale.begin(), scale.end(), pitch_class);
    return (it != scale.end()) ? static_cast<int>(std::distance(scale.begin(), it)) : -1;
}

bool MusicalRuleFactory::is_strong_beat_position(double position, const std::pair<int,int>& time_signature) {
    double beat_duration = 1.0 / time_signature.second; // e.g., 1/4 for quarter note beat
    double beat_position = fmod(position, beat_duration);
    return std::abs(beat_position) < 0.01; // Very close to beat boundary
}

// =============================================================================
// Single Engine Rhythm Rule Implementations
// =============================================================================

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_no_repeated_durations(
    int rhythm_engine, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const MusicalSequence& current, const MusicalSequence& previous) -> bool {
        if (current.empty() || previous.empty()) return true;
        return current[0] != previous[0]; // No repeated durations
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "No Repeated Durations", rhythm_engine, rule_function, 
        backtrack_route.empty() ? create_self_backtrack(rhythm_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_rhythmic_canon(
    int rhythm_engine, const MusicalSequence& pattern, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [pattern](const MusicalSequence& current, const MusicalSequence& previous) -> bool {
        // Check if rhythm matches the canon pattern
        for (size_t i = 0; i < std::min(current.size(), pattern.size()); ++i) {
            if (std::abs(current[i] - pattern[i % pattern.size()]) > 0.01) {
                return false;
            }
        }
        return true;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "Rhythmic Canon", rhythm_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(rhythm_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_accelerando(
    int rhythm_engine, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const MusicalSequence& current, const MusicalSequence& previous) -> bool {
        if (current.empty() || previous.empty()) return true;
        
        // Current durations should be shorter than previous (accelerando)
        double avg_current = std::accumulate(current.begin(), current.end(), 0.0) / current.size();
        double avg_previous = std::accumulate(previous.begin(), previous.end(), 0.0) / previous.size();
        
        return avg_current <= avg_previous;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "Accelerando", rhythm_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(rhythm_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_ritardando(
    int rhythm_engine, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const MusicalSequence& current, const MusicalSequence& previous) -> bool {
        if (current.empty() || previous.empty()) return true;
        
        // Current durations should be longer than previous (ritardando)
        double avg_current = std::accumulate(current.begin(), current.end(), 0.0) / current.size();
        double avg_previous = std::accumulate(previous.begin(), previous.end(), 0.0) / previous.size();
        
        return avg_current >= avg_previous;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "Ritardando", rhythm_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(rhythm_engine) : backtrack_route);
}

// =============================================================================
// Single Engine Pitch Rule Implementations
// =============================================================================

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_no_repeated_pitches(
    int pitch_engine, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const MusicalSequence& current, const MusicalSequence& previous) -> bool {
        if (current.empty() || previous.empty()) return true;
        
        for (MusicalValue current_pitch : current) {
            if (current_pitch < 0) continue; // Skip rests
            for (MusicalValue previous_pitch : previous) {
                if (previous_pitch < 0) continue; // Skip rests
                if (std::abs(current_pitch - previous_pitch) < 0.01) {
                    return false; // Repeated pitch found
                }
            }
        }
        return true;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "No Repeated Pitches", pitch_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(pitch_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_stepwise_motion(
    int pitch_engine, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const MusicalSequence& current, const MusicalSequence& previous) -> bool {
        if (current.empty() || previous.empty()) return true;
        
        // Check intervals between consecutive non-rest pitches
        MusicalValue last_previous_pitch = -1;
        for (auto it = previous.rbegin(); it != previous.rend(); ++it) {
            if (*it >= 0) {
                last_previous_pitch = *it;
                break;
            }
        }
        
        if (last_previous_pitch < 0) return true; // No previous pitch to compare
        
        for (MusicalValue current_pitch : current) {
            if (current_pitch < 0) continue; // Skip rests
            
            double interval = calculate_interval(last_previous_pitch, current_pitch);
            if (interval > 2.0) return false; // Larger than whole tone
            
            last_previous_pitch = current_pitch;
        }
        
        return true;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "Stepwise Motion", pitch_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(pitch_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_no_large_leaps(
    int pitch_engine, int max_interval, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [max_interval](const MusicalSequence& current, const MusicalSequence& previous) -> bool {
        if (current.empty() || previous.empty()) return true;
        
        // Get last pitch from previous cell
        MusicalValue last_pitch = -1;
        for (auto it = previous.rbegin(); it != previous.rend(); ++it) {
            if (*it >= 0) {
                last_pitch = *it;
                break;
            }
        }
        
        if (last_pitch < 0) return true;
        
        // Check intervals with current cell pitches
        for (MusicalValue current_pitch : current) {
            if (current_pitch < 0) continue;
            
            double interval = calculate_interval(last_pitch, current_pitch);
            if (interval > max_interval) return false;
            
            last_pitch = current_pitch;
        }
        
        return true;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "No Large Leaps", pitch_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(pitch_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_pitch_range(
    int pitch_engine, MusicalValue min_pitch, MusicalValue max_pitch,
    const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [min_pitch, max_pitch](const MusicalSequence& current) -> bool {
        for (MusicalValue pitch : current) {
            if (pitch < 0) continue; // Skip rests
            if (pitch < min_pitch || pitch > max_pitch) return false;
        }
        return true;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "Pitch Range", pitch_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(pitch_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_scale_membership(
    int pitch_engine, const std::vector<int>& scale_degrees,
    const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [scale_degrees](const MusicalSequence& current) -> bool {
        for (MusicalValue pitch : current) {
            if (pitch < 0) continue; // Skip rests
            
            int pitch_class = static_cast<int>(pitch) % 12;
            if (std::find(scale_degrees.begin(), scale_degrees.end(), pitch_class) == scale_degrees.end()) {
                return false;
            }
        }
        return true;
    };
    
    return RuleCompiler::compile_single_engine_cells_rule(
        "Scale Membership", pitch_engine, rule_function,
        backtrack_route.empty() ? create_self_backtrack(pitch_engine) : backtrack_route);
}

// =============================================================================
// Two Engine Rhythm-Pitch Rule Implementations
// =============================================================================

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_strong_beat_emphasis(
    int rhythm_engine, int pitch_engine, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const std::vector<std::pair<MusicalValue, MusicalValue>>& pairs) -> bool {
        if (pairs.empty()) return true;
        
        // Strong beats (longer durations) should have higher pitches
        for (const auto& pair : pairs) {
            MusicalValue duration = pair.first;
            MusicalValue pitch = pair.second;
            
            if (pitch < 0) continue; // Skip rests
            
            // Simple heuristic: longer durations should correlate with higher pitches
            // This is a simplified version - more complex metric analysis would be needed for full implementation
            if (duration >= 0.25 && pitch < 60) { // Quarter note or longer should be middle C or higher
                return false;
            }
        }
        
        return true;
    };
    
    return RuleCompiler::compile_dual_engine_rhythm_pitch_rule(
        "Strong Beat Emphasis", rhythm_engine, pitch_engine, rule_function,
        backtrack_route.empty() ? create_other_engine_backtrack(rhythm_engine) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_no_syncopation(
    int rhythm_engine, int pitch_engine, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const std::vector<std::pair<MusicalValue, MusicalValue>>& pairs) -> bool {
        // Avoid syncopated patterns (simplified check)
        for (size_t i = 1; i < pairs.size(); ++i) {
            MusicalValue prev_duration = pairs[i-1].first;
            MusicalValue curr_duration = pairs[i].first;
            
            // Avoid short-long-short patterns that create syncopation
            if (i < pairs.size() - 1) {
                MusicalValue next_duration = pairs[i+1].first;
                if (prev_duration < 0.25 && curr_duration >= 0.25 && next_duration < 0.25) {
                    return false;
                }
            }
        }
        return true;
    };
    
    return RuleCompiler::compile_dual_engine_rhythm_pitch_rule(
        "No Syncopation", rhythm_engine, pitch_engine, rule_function,
        backtrack_route.empty() ? create_other_engine_backtrack(rhythm_engine) : backtrack_route);
}

// =============================================================================
// Multi Engine Harmonic Rule Implementations
// =============================================================================

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_no_parallel_perfect_intervals(
    const std::vector<int>& pitch_engines, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const std::vector<std::vector<MusicalValue>>& slices) -> bool {
        if (slices.size() < 2) return true;
        
        const auto& prev_slice = slices[slices.size() - 2];
        const auto& curr_slice = slices[slices.size() - 1];
        
        if (prev_slice.size() < 2 || curr_slice.size() < 2) return true;
        
        // Check all voice pairs for parallel perfect intervals
        for (size_t i = 0; i < prev_slice.size(); ++i) {
            for (size_t j = i + 1; j < prev_slice.size(); ++j) {
                if (prev_slice[i] < 0 || prev_slice[j] < 0 || 
                    curr_slice[i] < 0 || curr_slice[j] < 0) continue;
                
                double prev_interval = calculate_interval(prev_slice[i], prev_slice[j]);
                double curr_interval = calculate_interval(curr_slice[i], curr_slice[j]);
                
                // Check for parallel perfect intervals (unison, 4th, 5th, octave)
                if (is_perfect_interval(prev_interval) && is_perfect_interval(curr_interval)) {
                    if (std::abs(prev_interval - curr_interval) < 0.1) {
                        return false; // Parallel perfect intervals detected
                    }
                }
            }
        }
        
        return true;
    };
    
    return RuleCompiler::compile_multi_engine_harmonic_rule(
        "No Parallel Perfect Intervals", pitch_engines, rule_function,
        backtrack_route.empty() ? create_multi_engine_backtrack(pitch_engines) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_consonant_harmonies(
    const std::vector<int>& pitch_engines, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const std::vector<std::vector<MusicalValue>>& slices) -> bool {
        if (slices.empty()) return true;
        
        const auto& current_slice = slices.back();
        if (current_slice.size() < 2) return true;
        
        // Check that all intervals in current harmonic slice are consonant
        for (size_t i = 0; i < current_slice.size(); ++i) {
            for (size_t j = i + 1; j < current_slice.size(); ++j) {
                if (current_slice[i] < 0 || current_slice[j] < 0) continue;
                
                double interval = calculate_interval(current_slice[i], current_slice[j]);
                if (!is_consonant_interval(interval)) {
                    return false;
                }
            }
        }
        
        return true;
    };
    
    return RuleCompiler::compile_multi_engine_harmonic_rule(
        "Consonant Harmonies", pitch_engines, rule_function,
        backtrack_route.empty() ? create_multi_engine_backtrack(pitch_engines) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_voice_leading(
    const std::vector<int>& pitch_engines, int max_step_size,
    const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [max_step_size](const std::vector<std::vector<MusicalValue>>& slices) -> bool {
        if (slices.size() < 2) return true;
        
        const auto& prev_slice = slices[slices.size() - 2];
        const auto& curr_slice = slices[slices.size() - 1];
        
        // Check voice leading for each voice
        for (size_t i = 0; i < std::min(prev_slice.size(), curr_slice.size()); ++i) {
            if (prev_slice[i] < 0 || curr_slice[i] < 0) continue;
            
            double interval = calculate_interval(prev_slice[i], curr_slice[i]);
            if (interval > max_step_size) {
                return false; // Voice leading too large
            }
        }
        
        return true;
    };
    
    return RuleCompiler::compile_multi_engine_harmonic_rule(
        "Voice Leading", pitch_engines, rule_function,
        backtrack_route.empty() ? create_multi_engine_backtrack(pitch_engines) : backtrack_route);
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_no_voice_crossing(
    const std::vector<int>& pitch_engines, const BacktrackRoute& backtrack_route) {
    
    auto rule_function = [](const std::vector<std::vector<MusicalValue>>& slices) -> bool {
        if (slices.empty()) return true;
        
        const auto& current_slice = slices.back();
        if (current_slice.size() < 2) return true;
        
        // Check that voices maintain their relative order (no crossing)
        for (size_t i = 0; i < current_slice.size() - 1; ++i) {
            if (current_slice[i] < 0 || current_slice[i+1] < 0) continue;
            
            // Assuming voices are ordered from lowest to highest
            if (current_slice[i] > current_slice[i+1]) {
                return false; // Voice crossing detected
            }
        }
        
        return true;
    };
    
    return RuleCompiler::compile_multi_engine_harmonic_rule(
        "No Voice Crossing", pitch_engines, rule_function,
        backtrack_route.empty() ? create_multi_engine_backtrack(pitch_engines) : backtrack_route);
}

// =============================================================================
// Backtrack Route Utilities
// =============================================================================

BacktrackRoute MusicalRuleFactory::create_self_backtrack(int engine_id) {
    return {engine_id};
}

BacktrackRoute MusicalRuleFactory::create_other_engine_backtrack(int engine_id) {
    // In cluster-engine, engines are paired: rhythm(0,2,4...) and pitch(1,3,5...)
    return {engine_id % 2 == 0 ? engine_id + 1 : engine_id - 1};
}

BacktrackRoute MusicalRuleFactory::create_multi_engine_backtrack(const std::vector<int>& engine_ids) {
    return engine_ids;
}

// =============================================================================
// Preset Rule Sets Implementation
// =============================================================================

void PresetRuleSets::add_classical_counterpoint_rules(
    RuleManager& manager, const std::vector<int>& voices, int cantus_firmus_voice) {
    
    // Add basic rules for each voice
    for (int voice : voices) {
        if (voice % 2 == 1) { // Pitch engines
            manager.add_rule(MusicalRuleFactory::create_stepwise_motion(voice));
            manager.add_rule(MusicalRuleFactory::create_no_large_leaps(voice, 8));
            manager.add_rule(MusicalRuleFactory::create_pitch_range(voice, 48, 84)); // C3 to C6
        }
    }
    
    // Add harmonic rules for multiple voices
    if (voices.size() > 1) {
        std::vector<int> pitch_voices;
        for (int voice : voices) {
            if (voice % 2 == 1) pitch_voices.push_back(voice);
        }
        
        if (pitch_voices.size() >= 2) {
            manager.add_rule(MusicalRuleFactory::create_no_parallel_perfect_intervals(pitch_voices));
            manager.add_rule(MusicalRuleFactory::create_consonant_harmonies(pitch_voices));
            manager.add_rule(MusicalRuleFactory::create_voice_leading(pitch_voices, 2));
            manager.add_rule(MusicalRuleFactory::create_no_voice_crossing(pitch_voices));
        }
    }
}

void PresetRuleSets::add_jazz_improvisation_rules(
    RuleManager& manager, const std::vector<int>& rhythm_engines, 
    const std::vector<int>& pitch_engines) {
    
    // Jazz allows more chromatic motion and rhythmic flexibility
    for (int pitch_engine : pitch_engines) {
        manager.add_rule(MusicalRuleFactory::create_no_large_leaps(pitch_engine, 12)); // Allow larger leaps
        manager.add_rule(MusicalRuleFactory::create_pitch_range(pitch_engine, 36, 96)); // Wider range
    }
    
    // Rhythm-pitch coordination
    for (size_t i = 0; i < std::min(rhythm_engines.size(), pitch_engines.size()); ++i) {
        manager.add_rule(MusicalRuleFactory::create_strong_beat_emphasis(rhythm_engines[i], pitch_engines[i]));
    }
}

void PresetRuleSets::add_minimalist_rules(
    RuleManager& manager, const std::vector<int>& engines) {
    
    // Very simple rules for minimalist composition
    for (int engine : engines) {
        if (engine % 2 == 0) { // Rhythm engines
            manager.add_rule(MusicalRuleFactory::create_no_repeated_durations(engine));
        } else { // Pitch engines  
            manager.add_rule(MusicalRuleFactory::create_stepwise_motion(engine));
        }
    }
}

void PresetRuleSets::add_basic_rules(
    RuleManager& manager, const std::vector<int>& rhythm_engines,
    const std::vector<int>& pitch_engines) {
    
    // Basic constraints for any musical context
    for (int rhythm_engine : rhythm_engines) {
        manager.add_rule(MusicalRuleFactory::create_no_repeated_durations(rhythm_engine));
    }
    
    for (int pitch_engine : pitch_engines) {
        manager.add_rule(MusicalRuleFactory::create_no_repeated_pitches(pitch_engine));
        manager.add_rule(MusicalRuleFactory::create_pitch_range(pitch_engine, 48, 84));
    }
}

} // namespace MusicalRules
} // namespace ClusterEngine